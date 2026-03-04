#include "sailfish-osk.h"

#if defined(SAILFISH) && defined(SAILFISH_MALIIT)

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>
#include <maliit-2/maliit-glib/maliitbus.h>
#include <maliit-2/maliit-glib/maliitcontext.h>
#include <maliit-2/maliit-glib/maliitserver.h>

static MaliitServer *osk_server = NULL;
static MaliitContext *osk_context = NULL;
static mudclient *osk_mud = NULL;
static int osk_initialized = 0;
static GMutex osk_init_mutex;
static char *osk_preedit = NULL;
static int osk_hide_pending = 0;
static int osk_last_orientation = 0;
static guint osk_show_timeout_id = 0;
static int osk_first_show = 1;

static void sailfish_osk_reset_input_method(void) {
    if (osk_server == NULL) {
        return;
    }

    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        G_DBUS_PROXY(osk_server), "reset", NULL, G_DBUS_CALL_FLAGS_NONE, 2000,
        NULL, &error);

    if (result != NULL) {
        g_variant_unref(result);
        return;
    }

    if (error != NULL) {
        fprintf(stderr, "SAILFISH OSK: reset failed: %s\n", error->message);
    }
    g_clear_error(&error);
}

static void sailfish_osk_update_widget_state(void) {
    if (osk_server == NULL) {
        return;
    }

    GVariantBuilder state_builder;
    g_variant_builder_init(&state_builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&state_builder, "{sv}", "focusState",
                          g_variant_new_boolean(TRUE));
    g_variant_builder_add(&state_builder, "{sv}", "enterKeyEnabled",
                          g_variant_new_boolean(TRUE));

    GVariant *state = g_variant_builder_end(&state_builder);
    g_variant_ref_sink(state);

    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        G_DBUS_PROXY(osk_server), "updateWidgetInformation",
        g_variant_new("(@a{sv}b)", state, TRUE), G_DBUS_CALL_FLAGS_NONE, 2000,
        NULL, &error);

    if (result != NULL) {
        g_variant_unref(result);
        return;
    }

    if (error != NULL) {
        fprintf(stderr, "SAILFISH OSK: updateWidgetInformation failed: %s\n",
                error->message);
    }
    g_clear_error(&error);
}

static int sailfish_osk_get_orientation_angle(mudclient *mud) {
    const char *env = getenv("SAILFISH_OSK_ANGLE");
    if (env != NULL && env[0] != '\0') {
        int angle = atoi(env);
        if (angle == 0 || angle == 90 || angle == 180 || angle == 270) {
            return angle;
        }
    }

    if (mud != NULL && mud->options != NULL) {
        switch (mud->options->orientation) {
        case OPTIONS_ORIENTATION_PORTRAIT:
            return 0;
        case OPTIONS_ORIENTATION_LANDSCAPE_INVERTED:
            return 270;
        case OPTIONS_ORIENTATION_LANDSCAPE:
        default:
            return 90;
        }
    }

    if (mud != NULL && mud->game_width >= mud->game_height) {
        return 90;
    }

    return 0;
}

static void sailfish_osk_send_text(const char *text) {
    if (osk_mud == NULL || text == NULL) {
        return;
    }

    const unsigned char *p = (const unsigned char *)text;
    for (; *p; p++) {
        if (!isprint(*p)) {
            continue;
        }
        mudclient_key_pressed(osk_mud, *p, *p);
    }
}

static void sailfish_osk_send_backspaces(int count) {
    if (osk_mud == NULL || count <= 0) {
        return;
    }

    for (int i = 0; i < count; i++) {
        mudclient_key_pressed(osk_mud, K_BACKSPACE, K_BACKSPACE);
        mudclient_key_released(osk_mud, K_BACKSPACE);
    }
}

static void sailfish_osk_apply_text_delta(const char *old_text,
                                          const char *new_text) {
    if (new_text == NULL) {
        new_text = "";
    }

    if (old_text == NULL) {
        old_text = "";
    }

    size_t old_len = strlen(old_text);
    size_t new_len = strlen(new_text);
    size_t prefix = 0;

    while (prefix < old_len && prefix < new_len &&
           old_text[prefix] == new_text[prefix]) {
        prefix++;
    }

    if (old_len > prefix) {
        sailfish_osk_send_backspaces((int)(old_len - prefix));
    }

    if (new_len > prefix) {
        sailfish_osk_send_text(new_text + prefix);
    }
}

static gboolean sailfish_osk_handle_commit_string(
    MaliitContext *object, GDBusMethodInvocation *invocation,
    const gchar *arg_unnamed_arg0, gint arg_unnamed_arg1,
    gint arg_unnamed_arg2, gint arg_unnamed_arg3, gpointer user_data) {
    (void)object;
    (void)arg_unnamed_arg1;
    (void)arg_unnamed_arg2;
    (void)arg_unnamed_arg3;
    (void)user_data;

    sailfish_osk_apply_text_delta(osk_preedit, arg_unnamed_arg0);

    g_free(osk_preedit);
    osk_preedit = NULL;

    maliit_context_complete_commit_string(object, invocation);
    return TRUE;
}

static gboolean sailfish_osk_handle_update_preedit(
    MaliitContext *object, GDBusMethodInvocation *invocation,
    const gchar *arg_unnamed_arg0, GVariant *arg_unnamed_arg1,
    gint arg_unnamed_arg2, gint arg_unnamed_arg3, gint arg_unnamed_arg4,
    gpointer user_data) {
    (void)object;
    if (arg_unnamed_arg0 != NULL) {
        sailfish_osk_apply_text_delta(osk_preedit, arg_unnamed_arg0);
        g_free(osk_preedit);
        osk_preedit = g_strdup(arg_unnamed_arg0);
    } else {
        sailfish_osk_apply_text_delta(osk_preedit, "");
        g_free(osk_preedit);
        osk_preedit = NULL;
    }
    (void)arg_unnamed_arg1;
    (void)arg_unnamed_arg2;
    (void)arg_unnamed_arg3;
    (void)arg_unnamed_arg4;
    (void)user_data;

    maliit_context_complete_update_preedit(object, invocation);
    return TRUE;
}

static gboolean sailfish_osk_handle_im_initiated_hide(
    MaliitContext *object, GDBusMethodInvocation *invocation,
    gpointer user_data) {
    (void)object;
    (void)user_data;

    sailfish_osk_hide();
    maliit_context_complete_im_initiated_hide(object, invocation);
    return TRUE;
}

static gboolean sailfish_osk_handle_key_event(
    MaliitContext *object, GDBusMethodInvocation *invocation,
    gint arg_unnamed_arg0, gint arg_unnamed_arg1, gint arg_unnamed_arg2,
    const gchar *arg_unnamed_arg3, gboolean arg_unnamed_arg4,
    gint arg_unnamed_arg5, guchar arg_unnamed_arg6, gpointer user_data) {
    (void)object;
    int backspace = 0;
    int enter = 0;

    if (arg_unnamed_arg3 != NULL && arg_unnamed_arg3[0] != '\0') {
        unsigned char ch = (unsigned char)arg_unnamed_arg3[0];
        if (ch == '\b' || ch == 0x7f) {
            backspace = 1;
        } else if (ch == '\n' || ch == '\r') {
            enter = 1;
        }
    }

    if (!backspace) {
        if (arg_unnamed_arg0 == 65288 || arg_unnamed_arg1 == 65288 ||
            arg_unnamed_arg2 == 65288) {
            backspace = 1;
        } else if (arg_unnamed_arg0 == 8 || arg_unnamed_arg1 == 8 ||
                   arg_unnamed_arg2 == 8) {
            backspace = 1;
        } else if (arg_unnamed_arg1 == 16777219 || arg_unnamed_arg2 == 16777219) {
            backspace = 1;
        } else if (arg_unnamed_arg1 == 16777223 || arg_unnamed_arg2 == 16777223) {
            backspace = 1;
        }
    }

    if (!enter) {
        if (arg_unnamed_arg1 == 16777220 || arg_unnamed_arg2 == 16777220) {
            enter = 1;
        } else if (arg_unnamed_arg1 == 16777221 ||
                   arg_unnamed_arg2 == 16777221) {
            enter = 1;
        }
    }

    if (backspace && arg_unnamed_arg0 == 6) {
        sailfish_osk_send_backspaces(1);
    }

    if (enter && arg_unnamed_arg0 == 6) {
        mudclient_key_pressed(osk_mud, K_ENTER, K_ENTER);
        mudclient_key_released(osk_mud, K_ENTER);
        osk_hide_pending = 1;
    }

    (void)arg_unnamed_arg0;
    (void)arg_unnamed_arg1;
    (void)arg_unnamed_arg2;
    (void)arg_unnamed_arg3;
    (void)arg_unnamed_arg4;
    (void)arg_unnamed_arg5;
    (void)arg_unnamed_arg6;
    (void)user_data;

    maliit_context_complete_key_event(object, invocation);
    return TRUE;
}

static void sailfish_osk_init(void) {
    g_mutex_lock(&osk_init_mutex);

    if (osk_initialized) {
        g_mutex_unlock(&osk_init_mutex);
        return;
    }

    if (!maliit_is_running()) {
        fprintf(stderr, "SAILFISH OSK: Maliit not running\n");
    }

    GError *error = NULL;
    osk_server = maliit_get_server_sync(NULL, &error);
    if (osk_server == NULL) {
        fprintf(stderr, "SAILFISH OSK: get_server failed: %s\n",
                error ? error->message : "unknown error");
        g_clear_error(&error);
    }

    osk_context = maliit_get_context_sync(NULL, &error);
    if (osk_context == NULL) {
        fprintf(stderr, "SAILFISH OSK: get_context failed: %s\n",
                error ? error->message : "unknown error");
        g_clear_error(&error);
    }

    if (osk_context != NULL) {
        g_signal_connect(osk_context, "handle-commit-string",
                         G_CALLBACK(sailfish_osk_handle_commit_string), NULL);
        g_signal_connect(osk_context, "handle-update-preedit",
                         G_CALLBACK(sailfish_osk_handle_update_preedit), NULL);
        g_signal_connect(osk_context, "handle-im-initiated-hide",
                         G_CALLBACK(sailfish_osk_handle_im_initiated_hide),
                         NULL);
        g_signal_connect(osk_context, "handle-key-event",
                         G_CALLBACK(sailfish_osk_handle_key_event), NULL);
        fprintf(stderr, "SAILFISH OSK: context handlers connected\n");
    }

    osk_initialized = 1;
    g_mutex_unlock(&osk_init_mutex);
}

static gboolean sailfish_osk_show_delayed(gpointer data) {
    (void)data;

    if (osk_server != NULL) {
        GError *error = NULL;
        if (!maliit_server_call_show_input_method_sync(osk_server, NULL,
                                                       &error)) {
            fprintf(stderr, "SAILFISH OSK: show_input_method failed: %s\n",
                    error ? error->message : "unknown error");
            g_clear_error(&error);
        }
    }

    osk_show_timeout_id = 0;
    return G_SOURCE_REMOVE;
}

void sailfish_osk_show(mudclient *mud, const char *text, int is_password) {
    (void)text;
    (void)is_password;

    osk_mud = mud;
    sailfish_osk_init();

    if (osk_server == NULL) {
        return;
    }

    // Recover from stale input-method state where Enter can be disabled.
    sailfish_osk_reset_input_method();

    GError *error = NULL;
    int angle = sailfish_osk_get_orientation_angle(mud);
    osk_last_orientation = angle;

    if (!maliit_server_call_app_orientation_about_to_change_sync(
            osk_server, angle, NULL, &error)) {
        g_clear_error(&error);
    }

    if (!maliit_server_call_app_orientation_changed_sync(osk_server, angle,
                                                         NULL, &error)) {
        g_clear_error(&error);
    }

    if (!maliit_server_call_activate_context_sync(osk_server, NULL, &error)) {
        fprintf(stderr, "SAILFISH OSK: activate_context failed: %s\n",
                error ? error->message : "unknown error");
        g_clear_error(&error);
    }

    sailfish_osk_update_widget_state();

    if (osk_show_timeout_id != 0) {
        g_source_remove(osk_show_timeout_id);
        osk_show_timeout_id = 0;
    }

    if (osk_first_show) {
        osk_first_show = 0;
        osk_show_timeout_id =
            g_timeout_add(150, sailfish_osk_show_delayed, NULL);
    } else {
        if (!maliit_server_call_show_input_method_sync(osk_server, NULL,
                                                       &error)) {
            fprintf(stderr, "SAILFISH OSK: show_input_method failed: %s\n",
                    error ? error->message : "unknown error");
            g_clear_error(&error);
        }
    }
}

void sailfish_osk_hide(void) {
    if (osk_server == NULL) {
        return;
    }

    if (osk_show_timeout_id != 0) {
        g_source_remove(osk_show_timeout_id);
        osk_show_timeout_id = 0;
    }

    GError *error = NULL;
    if (!maliit_server_call_hide_input_method_sync(osk_server, NULL, &error)) {
        fprintf(stderr, "SAILFISH OSK: hide_input_method failed: %s\n",
                error ? error->message : "unknown error");
        g_clear_error(&error);
    }
}

void sailfish_osk_poll(mudclient *mud) {
    (void)mud;
    while (g_main_context_pending(NULL)) {
        g_main_context_iteration(NULL, FALSE);
    }

    if (osk_hide_pending) {
        osk_hide_pending = 0;
        sailfish_osk_hide();
    }
}

#else

void sailfish_osk_show(mudclient *mud, const char *text, int is_password) {
    (void)mud;
    (void)text;
    (void)is_password;
}

void sailfish_osk_hide(void) {}

void sailfish_osk_poll(mudclient *mud) { (void)mud; }

#endif
