#pragma once

#include "mudclient.h"

#ifdef SAILFISH
void sailfish_osk_show(mudclient *mud, const char *text, int is_password);
void sailfish_osk_hide(void);
void sailfish_osk_poll(mudclient *mud);
#else
static inline void sailfish_osk_show(mudclient *mud, const char *text,
                                     int is_password) {
    (void)mud;
    (void)text;
    (void)is_password;
}
static inline void sailfish_osk_hide(void) {}
static inline void sailfish_osk_poll(mudclient *mud) { (void)mud; }
#endif
