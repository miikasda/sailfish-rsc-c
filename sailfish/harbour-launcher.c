#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static const char *hybris_path =
    "/usr/lib64:/usr/libexec/droid-hybris/system/lib64:/vendor/lib64:/system/lib64";

static void set_hybris_env(void) {
    const char *current = getenv("LD_LIBRARY_PATH");

    if (current == NULL || current[0] == '\0') {
        setenv("LD_LIBRARY_PATH", hybris_path, 1);
        return;
    }

    size_t len = strlen(hybris_path) + 1 + strlen(current) + 1;
    char *merged = malloc(len);

    if (merged == NULL) {
        return;
    }

    snprintf(merged, len, "%s:%s", hybris_path, current);
    setenv("LD_LIBRARY_PATH", merged, 1);
    free(merged);
}

int main(int argc, char **argv) {
    set_hybris_env();

    char **child_argv = calloc((size_t)argc + 1, sizeof(char *));
    if (child_argv == NULL) {
        perror("calloc");
        return 1;
    }

    child_argv[0] = "/usr/bin/harbour-rsc-c.bin";
    for (int i = 1; i < argc; ++i) {
        child_argv[i] = argv[i];
    }

    execv(child_argv[0], child_argv);
    perror("execv");
    free(child_argv);
    return 1;
}
