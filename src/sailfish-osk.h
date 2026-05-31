#pragma once

#include "mudclient.h"

#ifdef SAILFISH
void sailfish_osk_show(mudclient *mud, const char *text, int is_password);
void sailfish_osk_hide(void);
void sailfish_osk_poll(mudclient *mud);
int sailfish_osk_is_visible(void);
int sailfish_osk_get_window_size(mudclient *mud, int *width, int *height);
#else
static inline void sailfish_osk_show(mudclient *mud, const char *text,
                                     int is_password) {
    (void)mud;
    (void)text;
    (void)is_password;
}
static inline void sailfish_osk_hide(void) {}
static inline void sailfish_osk_poll(mudclient *mud) { (void)mud; }
static inline int sailfish_osk_is_visible(void) { return 0; }
static inline int sailfish_osk_get_window_size(mudclient *mud, int *width,
                                               int *height) {
    (void)mud;
    (void)width;
    (void)height;
    return 0;
}
#endif
