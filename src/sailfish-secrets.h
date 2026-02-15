#ifndef SAILFISH_SECRETS_H
#define SAILFISH_SECRETS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int sailfish_secrets_store_username(const char *username);
int sailfish_secrets_store_password(const char *password);
int sailfish_secrets_load_username(char *username, size_t username_len);
int sailfish_secrets_load_password(char *password, size_t password_len);
int sailfish_secrets_clear_username(void);
int sailfish_secrets_clear_password(void);

#ifdef __cplusplus
}
#endif

#endif
