#include "mudclient.h"

#if defined(SDL12) || defined(SDL2)

#ifdef __SWITCH__
#define MAX_KBD_STR_SIZE 200
static SwkbdConfig switch_keyboard;
static char switch_keyboard_buffer[MAX_KBD_STR_SIZE] = {0};
static uint8_t switch_mouse_button = 1;
#endif

static int mudclient_horizontal_drag = 0;
static int mudclient_vertical_drag = 0;

static double mudclient_pinch_distance = 0;

static int mudclient_has_right_clicked = 0;

static int mudclient_touch_start = 0; // ms
static int mudclient_touch_start_x = 0;
static int mudclient_touch_start_y = 0;

static int64_t mudclient_finger_1_id = 0;
static int64_t mudclient_finger_2_id = 0;

#ifdef SAILFISH
static void sailfish_rotate_touch(mudclient *mud, int window_width,
                                  int window_height, int *touch_x,
                                  int *touch_y) {
    if (window_width <= 0 || window_height <= 0 || mud->game_width <= 0 ||
        mud->game_height <= 0) {
        return;
    }

    float scale_w = window_width / (float)mud->game_height;
    float scale_h = window_height / (float)mud->game_width;
    float scale = scale_w < scale_h ? scale_w : scale_h;

    int scaled_w = (int)(mud->game_height * scale);
    int scaled_h = (int)(mud->game_width * scale);

    if (scaled_w <= 0 || scaled_h <= 0) {
        return;
    }

    int x_offset = (window_width - scaled_w) / 2;
    int y_offset = (window_height - scaled_h) / 2;

    int local_x = *touch_x - x_offset;
    int local_y = *touch_y - y_offset;

    if (local_x < 0) {
        local_x = 0;
    } else if (local_x >= scaled_w) {
        local_x = scaled_w - 1;
    }

    if (local_y < 0) {
        local_y = 0;
    } else if (local_y >= scaled_h) {
        local_y = scaled_h - 1;
    }

    int rotated_x = (local_y * mud->game_width) / scaled_h;
    int rotated_y =
        ((scaled_w - 1 - local_x) * mud->game_height) / scaled_w;

    *touch_x = rotated_x;
    *touch_y = rotated_y;
}

static SDL_Window *sailfish_get_window(mudclient *mud) {
#ifdef RENDER_GL
    if (mud->gl_window != NULL) {
        return mud->gl_window;
    }
#endif
    return mud->window;
}
#endif

void mudclient_poll_events(mudclient *mud) {
    if (!mudclient_has_right_clicked && !mudclient_horizontal_drag &&
        !mudclient_vertical_drag && mudclient_finger_1_down &&
        !mudclient_finger_2_down &&
        get_ticks() - mudclient_touch_start >= mud->options->touch_menu_delay) {
        mudclient_mouse_pressed(mud, mud->mouse_x, mud->mouse_y, 3);
        mudclient_mouse_released(mud, mud->mouse_x, mud->mouse_y, 3);
        mudclient_has_right_clicked = 1;
    }

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exit(0);
            break;
        case SDL_KEYDOWN: {
            char char_code;
            int code;
            get_sdl_keycodes(&event.key.keysym, &char_code, &code);
#ifdef SDL_TEXTINPUT
            if (isdigit((unsigned char)char_code)) {
		return;
            }
#endif
            mudclient_key_pressed(mud, code, char_code);
            break;
        }
        case SDL_KEYUP: {
            char char_code;
            int code;
            get_sdl_keycodes(&event.key.keysym, &char_code, &code);
            mudclient_key_released(mud, code);

#ifdef ANDROID
            if (code == K_ENTER) {
                SDL_StopTextInput();
            }
#endif
#ifdef SAILFISH
            if (code == K_ENTER || code == K_ESCAPE) {
                sailfish_osk_hide();
            }
#endif
            break;
        }
        case SDL_MOUSEMOTION:
            if (!mudclient_is_touch(mud)) {
#ifdef SAILFISH
                int window_width = mud->game_width;
                int window_height = mud->game_height;

#ifdef SDL2
                SDL_Window *window = sailfish_get_window(mud);
                if (window != NULL) {
                    SDL_GetWindowSize(window, &window_width, &window_height);
                }
#endif

                int touch_x = event.motion.x;
                int touch_y = event.motion.y;

                int raw_x = touch_x;
                int raw_y = touch_y;
                mud->window_mouse_x = raw_x;
                mud->window_mouse_y = raw_y;
                sailfish_rotate_touch(mud, window_width, window_height,
                                      &touch_x, &touch_y);
#ifdef SAILFISH
                static int logged = 0;
                if (!logged) {
                    logged = 1;
                    printf("SAILFISH mouse motion raw=%d,%d rotated=%d,%d win=%dx%d game=%dx%d\n",
                           raw_x, raw_y, touch_x, touch_y, window_width,
                           window_height, mud->game_width, mud->game_height);
                    fflush(stdout);
                }
#endif
                mudclient_mouse_moved(mud, touch_x, touch_y);
#else
                mudclient_mouse_moved(mud, event.motion.x, event.motion.y);
#endif
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (!mudclient_is_touch(mud)) {
#ifdef SAILFISH
                int window_width = mud->game_width;
                int window_height = mud->game_height;

#ifdef SDL2
                SDL_Window *window = sailfish_get_window(mud);
                if (window != NULL) {
                    SDL_GetWindowSize(window, &window_width, &window_height);
                }
#endif

                int touch_x = event.button.x;
                int touch_y = event.button.y;

                int raw_x = touch_x;
                int raw_y = touch_y;
                mud->window_mouse_x = raw_x;
                mud->window_mouse_y = raw_y;
                sailfish_rotate_touch(mud, window_width, window_height,
                                      &touch_x, &touch_y);
#ifdef SAILFISH
                static int logged_down = 0;
                if (!logged_down) {
                    logged_down = 1;
                    printf("SAILFISH mouse down raw=%d,%d rotated=%d,%d win=%dx%d game=%dx%d\n",
                           raw_x, raw_y, touch_x, touch_y, window_width,
                           window_height, mud->game_width, mud->game_height);
                    fflush(stdout);
                }
#endif
                mudclient_mouse_pressed(mud, touch_x, touch_y,
                                        event.button.button);
#else
                mudclient_mouse_pressed(mud, event.button.x, event.button.y,
                                        event.button.button);
#endif
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (!mudclient_is_touch(mud)) {
#ifdef SAILFISH
                int window_width = mud->game_width;
                int window_height = mud->game_height;

#ifdef SDL2
                SDL_Window *window = sailfish_get_window(mud);
                if (window != NULL) {
                    SDL_GetWindowSize(window, &window_width, &window_height);
                }
#endif

                int touch_x = event.button.x;
                int touch_y = event.button.y;

                int raw_x = touch_x;
                int raw_y = touch_y;
                mud->window_mouse_x = raw_x;
                mud->window_mouse_y = raw_y;
                sailfish_rotate_touch(mud, window_width, window_height,
                                      &touch_x, &touch_y);
#ifdef SAILFISH
                static int logged_up = 0;
                if (!logged_up) {
                    logged_up = 1;
                    printf("SAILFISH mouse up raw=%d,%d rotated=%d,%d win=%dx%d game=%dx%d\n",
                           raw_x, raw_y, touch_x, touch_y, window_width,
                           window_height, mud->game_width, mud->game_height);
                    fflush(stdout);
                }
#endif
                mudclient_mouse_released(mud, touch_x, touch_y,
                                         event.button.button);
#else
                mudclient_mouse_released(mud, event.button.x, event.button.y,
                                         event.button.button);
#endif
            }
            break;
#ifndef SDL12
        case SDL_MOUSEWHEEL:
            if (mud->options->mouse_wheel) {
                if (event.wheel.y != 0) {
                    mud->mouse_scroll_delta = (event.wheel.y > 0 ? -1 : 1);
                }

                if (event.wheel.x != 0) {
                    int direction = event.wheel.x > 0 ? 1 : -1;

                    mud->camera_rotation =
                        (mud->camera_rotation + (direction * 3)) & 0xff;
                }
            }
            break;
        case SDL_FINGERMOTION: {
#ifdef ANDROID
            if (SDL_IsTextInputActive()) {
                SDL_StopTextInput();
                return;
            }
#endif

            int window_width = mud->game_width;
            int window_height = mud->game_height;

#ifdef SDL2
#ifdef SAILFISH
            SDL_Window *window = sailfish_get_window(mud);
#else
            SDL_Window *window = mud->window;
#endif
            if (window != NULL) {
                SDL_GetWindowSize(window, &window_width, &window_height);
            }
#endif

            int touch_x = event.tfinger.x * window_width;
            int touch_y = event.tfinger.y * window_height;

#ifdef SAILFISH
            mud->window_mouse_x = touch_x;
            mud->window_mouse_y = touch_y;
            sailfish_rotate_touch(mud, window_width, window_height, &touch_x,
                                  &touch_y);
#endif

#ifdef __SWITCH__
            mudclient_mouse_moved(mud, touch_x, touch_y);
#else
            if (!mudclient_is_touch(mud)) {
                break;
            }

            int64_t finger_id = event.tfinger.fingerId;

            if (finger_id == mudclient_finger_1_id) {
                mudclient_finger_1_x = touch_x;
                mudclient_finger_1_y = touch_y;
            } else if (finger_id == mudclient_finger_2_id) {
                mudclient_finger_2_x = touch_x;
                mudclient_finger_2_y = touch_y;
            }

            if (mud->options->touch_pinch != 0 && mudclient_finger_1_down &&
                mudclient_finger_2_down) {
                double pinch_distance =
                    distance(mudclient_finger_1_x, mudclient_finger_1_y,
                             mudclient_finger_2_x, mudclient_finger_2_y);

                if (mudclient_pinch_distance > 0) {
                    float scale = mud->options->touch_pinch / 100.0f;

                    mud->mouse_scroll_delta =
                        (mudclient_pinch_distance - pinch_distance) * scale;
                }

                mudclient_pinch_distance = pinch_distance;
                mudclient_has_right_clicked = 1;
            } else if (mudclient_finger_1_down && !mudclient_finger_2_down) {
                int delta_x = touch_x - mudclient_touch_start_x;
                int delta_y = touch_y - mudclient_touch_start_y;

                if (!mudclient_horizontal_drag && abs(delta_x) > 30) {
                    mudclient_horizontal_drag = 1;

                    mudclient_mouse_pressed(mud, mudclient_touch_start_x,
                                            mudclient_touch_start_y, 2);
                }

                if (mud->options->touch_vertical_drag != 0 &&
                    mud->show_ui_tab == 0 &&
                    (mudclient_vertical_drag || abs(delta_y) > 30)) {
                    mudclient_vertical_drag = 1;

                    mud->mouse_scroll_delta =
                        (event.tfinger.dy *
                         (mud->options->touch_vertical_drag / 100.0f)) *
                        window_height;
                }

                mudclient_mouse_moved(mud, touch_x, touch_y);
            }
#endif
            break;
        }
        case SDL_FINGERDOWN: {
#ifdef ANDROID
            if (SDL_IsTextInputActive()) {
                SDL_StopTextInput();
                return;
            }
#endif

            int window_width = mud->game_width;
            int window_height = mud->game_height;

#ifdef SDL2
#ifdef SAILFISH
            SDL_Window *window = sailfish_get_window(mud);
#else
            SDL_Window *window = mud->window;
#endif
            if (window != NULL) {
                SDL_GetWindowSize(window, &window_width, &window_height);
            }
#endif

            int touch_x = event.tfinger.x * window_width;
            int touch_y = event.tfinger.y * window_height;

#ifdef SAILFISH
            mud->window_mouse_x = touch_x;
            mud->window_mouse_y = touch_y;
            sailfish_rotate_touch(mud, window_width, window_height, &touch_x,
                                  &touch_y);
#endif

#ifdef __SWITCH__
            mudclient_mouse_pressed(mud, touch_x, touch_y, switch_mouse_button);
#else
            if (!mudclient_is_touch(mud)) {
                break;
            }

            int64_t finger_id = event.tfinger.fingerId;

            if (!mudclient_finger_1_down) {
                mudclient_has_right_clicked = 0;

                mudclient_finger_1_id = finger_id;
                mudclient_finger_1_down = 1;

                mudclient_finger_1_x = touch_x;
                mudclient_finger_1_y = touch_y;

                mudclient_touch_start = get_ticks();

                mudclient_touch_start_x = touch_x;
                mudclient_touch_start_y = touch_y;

                mudclient_mouse_moved(mud, touch_x, touch_y);
            } else if (!mudclient_finger_2_down) {
                mudclient_finger_2_id = finger_id;
                mudclient_finger_2_down = 1;

                mudclient_finger_2_x = touch_x;
                mudclient_finger_2_y = touch_y;
            } else {
                break;
            }
#endif
            break;
        }
        case SDL_FINGERUP: {
#ifdef ANDROID
            if (SDL_IsTextInputActive()) {
                SDL_StopTextInput();
                return;
            }
#endif

            int window_width = mud->game_width;
            int window_height = mud->game_height;

#ifdef SDL2
#ifdef SAILFISH
            SDL_Window *window = sailfish_get_window(mud);
#else
            SDL_Window *window = mud->window;
#endif
            if (window != NULL) {
                SDL_GetWindowSize(window, &window_width, &window_height);
            }
#endif

            int touch_x = event.tfinger.x * window_width;
            int touch_y = event.tfinger.y * window_height;

#ifdef SAILFISH
            mud->window_mouse_x = touch_x;
            mud->window_mouse_y = touch_y;
            sailfish_rotate_touch(mud, window_width, window_height, &touch_x,
                                  &touch_y);
#endif

#ifdef __SWITCH__
            mudclient_mouse_released(mud, touch_x, touch_y,
                                     switch_mouse_button);
#else
            if (!mudclient_is_touch(mud)) {
                break;
            }

            int64_t finger_id = event.tfinger.fingerId;

            if (mudclient_finger_1_down && finger_id == mudclient_finger_1_id) {
                mudclient_finger_1_down = 0;

                if (!mudclient_has_right_clicked && !mudclient_vertical_drag &&
                    !mudclient_horizontal_drag &&
                    mudclient_pinch_distance == 0) {
                    mudclient_mouse_pressed(mud, touch_x, touch_y, 0);
                    mudclient_mouse_released(mud, touch_x, touch_y, 0);
                } else {
                    mudclient_vertical_drag = 0;

                    if (mudclient_horizontal_drag) {
                        mudclient_mouse_released(mud, mud->mouse_x,
                                                 mud->mouse_y, 2);

                        mudclient_horizontal_drag = 0;
                    }
                }
            } else if (mudclient_finger_2_down &&
                       finger_id == mudclient_finger_2_id) {
                mudclient_finger_2_down = 0;
                mudclient_pinch_distance = 0;
            }
#endif
            break;
        }
#endif
#ifdef __SWITCH__
        case SDL_JOYBUTTONDOWN:
            switch (event.jbutton.button) {
            case 0: // A Button
                mudclient_key_pressed(mud, K_ENTER, K_ENTER);
                break;
            case 1: // B Button
                mudclient_key_pressed(mud, K_BACKSPACE, K_BACKSPACE);
                break;
            case 2: // X Button
                mudclient_key_pressed(mud, K_TAB, -1);
                break;
            case 3: // Y Button
                mudclient_key_pressed(mud, K_HOME, -1);
                break;
            case 6: // L Button
                mudclient_key_pressed(mud, K_ESCAPE, -1);
                break;
            case 7: // R Button
                if (mud->options->display_fps == 0)
                    mud->options->display_fps = 1;
                else
                    mud->options->display_fps = 0;
                break;
            case 8: // ZL
                switch_mouse_button = 3;
                break;
            case 9: // ZR
                // Reserved
                break;
            case 11: // Minus Button
                mudclient_key_pressed(mud, K_F1, -1);
                break;
            case 10: // Plus Button
                swkbdCreate(&switch_keyboard, 0);
                swkbdConfigSetType(&switch_keyboard, SwkbdType_QWERTY);
                swkbdConfigSetBlurBackground(&switch_keyboard, 0);

                swkbdConfigSetTextDrawType(&switch_keyboard,
                                           SwkbdTextDrawType_Box);

                swkbdConfigSetReturnButtonFlag(&switch_keyboard, 0);
                swkbdConfigSetStringLenMax(&switch_keyboard, MAX_KBD_STR_SIZE);
                swkbdConfigSetOkButtonText(&switch_keyboard, "Submit");

                swkbdShow(&switch_keyboard, switch_keyboard_buffer,
                          sizeof(switch_keyboard_buffer));

                for (int i = 0; i < sizeof(switch_keyboard_buffer); i++) {
                    mudclient_key_pressed(mud, -1, switch_keyboard_buffer[i]);
                }

                swkbdClose(&switch_keyboard);
                break;
            case 12: // DPAD LEFT
            case 16: // Left Stick Left
                mudclient_key_pressed(mud, K_LEFT, -1);
                break;
            case 13: // DPAD UP
            case 17: // Left Stick Up
                mudclient_key_pressed(mud, K_UP, -1);
                break;
            case 14: // DPAD RIGHT
            case 18: // Left Stick Right
                mudclient_key_pressed(mud, K_RIGHT, -1);
                break;
            case 15: // DPAD DOWN
            case 19: // Left Stick Down
                mudclient_key_pressed(mud, K_DOWN, -1);
                break;
            case 20: // Right Stick Left
                break;
            case 21: // Right Stick Up
                mudclient_key_pressed(mud, K_PAGE_UP, -1);
                break;
            case 22: // Right Stick Right
                break;
            case 23: // Right Stick Down
                mudclient_key_pressed(mud, K_PAGE_DOWN, -1);
                break;
            }
            break;
        case SDL_JOYBUTTONUP:
            switch (event.jbutton.button) {
            case 0: // A Button
                mudclient_key_released(mud, K_ENTER);
                break;
            case 1: // B Button
                mudclient_key_released(mud, K_BACKSPACE);
                break;
            case 2: // X Button
                mudclient_key_released(mud, K_TAB);
                break;
            case 3: // Y Button
                mudclient_key_released(mud, K_HOME);
                break;
            case 6: // L Button
                mudclient_key_released(mud, K_ESCAPE);
                break;
            case 7: // R Button
                break;
            case 8: // ZL
                switch_mouse_button = 1;
                break;
            case 9: // ZR
                // Reserved
                break;
            case 11: // Minus Button
                mudclient_key_released(mud, K_F1);
                break;
            case 12: // DPAD LEFT
            case 16: // Left Stick Left
                mudclient_key_released(mud, K_LEFT);
                break;
            case 13: // DPAD UP
            case 17: // Left Stick Up
                mudclient_key_released(mud, K_UP);
                break;
            case 14: // DPAD RIGHT
            case 18: // Left Stick Right
                mudclient_key_released(mud, K_RIGHT);
                break;
            case 15: // DPAD DOWN
            case 19: // Left Stick Down
                mudclient_key_released(mud, K_DOWN);
                break;
            case 20: // Right Stick Left
                break;
            case 21: // Right Stick Up
                mudclient_key_released(mud, K_PAGE_UP);
                break;
            case 22: // Right Stick Right
                break;
            case 23: // Right Stick Down
                mudclient_key_released(mud, K_PAGE_DOWN);
                break;
            }
            break;
#endif
#ifdef SDL12
        case SDL_VIDEORESIZE:
            mudclient_sdl1_on_resize(mud, event.resize.w, event.resize.h);
            break;
#else
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                mudclient_on_resize(mud);
            }
            break;
        case SDL_TEXTINPUT:
            if (strlen(event.text.text) == 1) {
                char ch = event.text.text[0];

                if (isprint((unsigned char)ch)) {
                    mudclient_key_pressed(mud, ch, ch);
                }
            }
            break;
#endif
        }
    }

#ifdef SAILFISH
    sailfish_osk_poll(mud);
#endif
}
#endif
