#include "mudclient.h"

#ifdef SDL2
#ifdef SAILFISH
#include <SDL2/SDL_syswm.h>
#include <wayland-client.h>

#ifndef SDL_HINT_QTWAYLAND_CONTENT_ORIENTATION
#define SDL_HINT_QTWAYLAND_CONTENT_ORIENTATION                                 \
    "SDL_QTWAYLAND_CONTENT_ORIENTATION"
#endif

static int mudclient_sailfish_parse_version(const char *text, int *major,
                                            int *minor) {
    const char *p = text;

    while (*p != '\0' && !isdigit((unsigned char)*p)) {
        p++;
    }

    if (*p == '\0') {
        return 0;
    }

    *major = atoi(p);

    while (isdigit((unsigned char)*p)) {
        p++;
    }

    if (*p != '.') {
        *minor = 0;
        return 1;
    }

    p++;
    *minor = atoi(p);

    return 1;
}

static int mudclient_sailfish_read_version_file(const char *path,
                                                int allow_any_line,
                                                int *major, int *minor) {
    FILE *file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    char line[256];

    while (fgets(line, sizeof(line), file) != NULL) {
        if ((allow_any_line || strncmp(line, "VERSION_ID=", 11) == 0) &&
            mudclient_sailfish_parse_version(line, major, minor)) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

static int mudclient_sailfish_is_5_1_or_newer(void) {
    int major = 0;
    int minor = 0;

    if (!mudclient_sailfish_read_version_file("/etc/os-release", 0, &major,
                                              &minor) &&
        !mudclient_sailfish_read_version_file("/usr/lib/os-release", 0,
                                              &major, &minor) &&
        !mudclient_sailfish_read_version_file("/etc/sailfish-release", 1,
                                              &major, &minor)) {
        return 0;
    }

    return major > 5 || (major == 5 && minor >= 1);
}

static int mudclient_sailfish_use_legacy_orientation(void) {
    static int use_legacy = -1;

    if (use_legacy < 0) {
        const char *mode = getenv("RSC_SAILFISH_ORIENTATION");

        if (mode != NULL && strcmp(mode, "hint") == 0) {
            use_legacy = 0;
        } else if (mode != NULL && strcmp(mode, "legacy") == 0) {
            use_legacy = 1;
        } else {
            /* SFOS < 5.1 needs the direct Wayland buffer-transform path. */
            use_legacy = !mudclient_sailfish_is_5_1_or_newer();
        }
    }

    return use_legacy;
}

static const char *mudclient_sailfish_content_orientation(const mudclient *mud) {
    switch (mud->options->orientation) {
    case OPTIONS_ORIENTATION_PORTRAIT:
        return "portrait";
    case OPTIONS_ORIENTATION_LANDSCAPE_INVERTED:
        return "inverted-landscape";
    case OPTIONS_ORIENTATION_LANDSCAPE:
    default:
        return "landscape";
    }
}

static void mudclient_sailfish_set_orientation_hint(mudclient *mud) {
    if (!mudclient_sailfish_use_legacy_orientation()) {
        SDL_SetHint(SDL_HINT_QTWAYLAND_CONTENT_ORIENTATION,
                    mudclient_sailfish_content_orientation(mud));
    }
}

static enum wl_output_transform
mudclient_get_sailfish_transform(const mudclient *mud) {
    switch (mud->options->orientation) {
    case OPTIONS_ORIENTATION_PORTRAIT:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case OPTIONS_ORIENTATION_LANDSCAPE_INVERTED:
        return WL_OUTPUT_TRANSFORM_90;
    case OPTIONS_ORIENTATION_LANDSCAPE:
    default:
        return WL_OUTPUT_TRANSFORM_270;
    }
}

void mudclient_sailfish_apply_orientation(mudclient *mud) {
    if (!mudclient_sailfish_use_legacy_orientation()) {
        mudclient_sailfish_set_orientation_hint(mud);
        return;
    }

    SDL_Window *window = mud->window;

#ifdef RENDER_GL
    if (window == NULL) {
        window = mud->gl_window;
    }
#endif

    if (window == NULL) {
        return;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);

    if (SDL_GetWindowWMInfo(window, &info) == SDL_TRUE &&
        info.subsystem == SDL_SYSWM_WAYLAND && info.info.wl.surface != NULL) {
        wl_surface_set_buffer_transform(info.info.wl.surface,
                                        mudclient_get_sailfish_transform(mud));
        wl_surface_commit(info.info.wl.surface);
    }
}
#endif

#ifdef __SWITCH__
static SDL_Joystick *joystick;
#endif

void get_sdl_keycodes(SDL_Keysym *keysym, char *char_code, int *code) {
    *code = -1;
    *char_code = -1;

    switch (keysym->scancode) {
    case SDL_SCANCODE_LEFT:
        *code = K_LEFT;
        break;
    case SDL_SCANCODE_RIGHT:
        *code = K_RIGHT;
        break;
    case SDL_SCANCODE_UP:
        *code = K_UP;
        break;
    case SDL_SCANCODE_DOWN:
        *code = K_DOWN;
        break;
    case SDL_SCANCODE_PAGEUP:
        *code = K_PAGE_UP;
        break;
    case SDL_SCANCODE_PAGEDOWN:
        *code = K_PAGE_DOWN;
        break;
    case SDL_SCANCODE_HOME:
        *code = K_HOME;
        break;
    case SDL_SCANCODE_F1:
        *code = K_F1;
        break;
    case SDL_SCANCODE_ESCAPE:
        *code = K_ESCAPE;
        break;
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_RETURN:
        *code = K_ENTER;
        *char_code = K_ENTER;
        break;
    // TODO: Swallow "bad inputs" by default? ie. numlock, capslock
    case SDL_SCANCODE_NUMLOCKCLEAR:
        *code = -1;
        *char_code = 1;
        break;
    case SDL_SCANCODE_CAPSLOCK:
        *code = -1;
        *char_code = 1;
        break;
    case SDL_SCANCODE_TAB:
        *code = K_TAB;
        *char_code = K_TAB;
        break;
    case SDL_SCANCODE_BACKSPACE:
        *code = K_BACKSPACE;
        *char_code = K_BACKSPACE;
        break;
    default:
        break;
    }
}

void mudclient_start_application(mudclient *mud, char *title) {
#ifdef __SWITCH__
    Result romfs_res = romfsInit();

    if (romfs_res) {
        mud_error("romfsInit: %08lX\n", romfs_res);
        exit(1);
    }
#endif

    int init = SDL_INIT_VIDEO;

#ifdef SAILFISH
    /* Sailfish OS: ensure PulseAudio tags are set before SDL audio init. */
    setenv("PULSE_PROP_application.name", "RS Classic", 1);
    setenv("PULSE_PROP_media.role", "x-maemo", 1);
#endif

    if (mud->options->members && !mud->options->lowmem) {
        init |= SDL_INIT_AUDIO;
    }

#ifdef __SWITCH__
    init |= SDL_INIT_JOYSTICK;
#endif

    if (SDL_Init(init) < 0) {
        mud_error("SDL_Init(): %s\n", SDL_GetError());
        exit(1);
    }

#ifdef SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#endif

#ifdef __SWITCH__
    SDL_JoystickEventState(SDL_ENABLE);
    joystick = SDL_JoystickOpen(0);
#endif

/* XXX: currently require non-callback-based audio from SDL >= 2.0.4 */
#ifdef SDL_VERSION_ATLEAST
#if SDL_VERSION_ATLEAST(2, 0, 4)
    if (mud->options->members && !mud->options->lowmem) {
        SDL_AudioSpec wanted_audio;

        wanted_audio.freq = SAMPLE_RATE;
        wanted_audio.format = AUDIO_S16;
        wanted_audio.channels = 1;
        wanted_audio.silence = 0;
        wanted_audio.samples = 1024;
        wanted_audio.callback = NULL;

        if (SDL_OpenAudio(&wanted_audio, NULL) < 0) {
            mud_error("SDL_OpenAudio(): %s\n", SDL_GetError());
        }
    }
#endif
#endif

    uint32_t windowflags = SDL_WINDOW_SHOWN;

#if !defined(WII) && !defined(_3DS) && !defined(EMSCRIPTEN)
    windowflags |= SDL_WINDOW_RESIZABLE;
#endif

#ifdef RENDER_GL
    windowflags |= SDL_WINDOW_OPENGL;

#ifdef SAILFISH
    {
        const char *candidates[] = {
            "/usr/lib64/libGLESv2.so.2",
            "/usr/libexec/droid-hybris/system/lib64/libGLESv2.so",
            "/system/lib64/libGLESv2.so",
            "libGLESv2.so.2",
            "libGLESv2.so",
        };
        size_t i;
        int loaded = 0;

        for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
            if (SDL_GL_LoadLibrary(candidates[i]) == 0) {
                loaded = 1;
                break;
            }
        }

        if (!loaded) {
            fprintf(stderr,
                    "SAILFISH: SDL_GL_LoadLibrary failed: %s\n",
                    SDL_GetError());
        }
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#elif defined(EMSCRIPTEN)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#elif defined(OPENGL15)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#elif defined(OPENGL20)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    // SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
#endif /* EMSCRIPTEN */
#endif /* RENDER_GL */

#ifdef SAILFISH
    mudclient_sailfish_set_orientation_hint(mud);
#endif

    mud->window =
        SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         mud->game_width, mud->game_height, windowflags);

#ifdef SAILFISH
    if (mud->window != NULL) {
        SDL_DisplayMode mode;

        if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
            int width = mode.w;
            int height = mode.h;

            SDL_SetWindowBordered(mud->window, SDL_FALSE);
            SDL_SetWindowPosition(mud->window, 0, 0);
            SDL_SetWindowSize(mud->window, width, height);
            SDL_SetWindowFullscreen(mud->window,
                                    SDL_WINDOW_FULLSCREEN_DESKTOP);
        }

#ifdef SAILFISH
        mudclient_sailfish_apply_orientation(mud);
#endif

        #ifndef RENDER_GL
        mudclient_on_resize(mud);
        #endif
    }

#endif

    SDL_SetWindowMinimumSize(mud->window, MUD_MIN_WIDTH, MUD_MIN_HEIGHT);

    mud->default_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    mud->hand_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

#ifdef RENDER_GL
    mud->gl_window = mud->window;
    mud->window = NULL;

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        mud_error("unable to initialize sdl_image: %s\n", IMG_GetError());
    }

    SDL_GLContext *context = SDL_GL_CreateContext(mud->gl_window);

    if (!context) {
        mud_error("SDL_GL_CreateContext(): %s\n", SDL_GetError());
        exit(1);
    }

    if (SDL_GL_MakeCurrent(mud->gl_window, context) != 0) {
        mud_error("SDL_GL_MakeCurrent(): %s\n", SDL_GetError());
        exit(1);
    }

#ifdef SAILFISH
    mudclient_on_resize(mud);
#endif
#endif
}
#endif
