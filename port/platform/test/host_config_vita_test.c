#include <stdio.h>
#include <string.h>
#include "host_config.h"
#include "host_fs.h"

#ifndef ICO_HOST_VITA_DATA_DIR
#error "test requires a controlled Vita data directory"
#endif

static int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);               \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

int main(void)
{
    char exe[ICO_PATH_MAX];
    char tiny[5];
    char data[ICO_PATH_MAX];
    char ini[ICO_PATH_MAX];
    char expected[ICO_PATH_MAX];
    char joined[ICO_PATH_MAX];

    CHECK(ico_host_exe_dir(exe, sizeof(exe)) == 0);
    CHECK(strcmp(exe, "app0:") == 0);
    memset(tiny, 'x', sizeof(tiny));
    CHECK(ico_host_exe_dir(tiny, sizeof(tiny)) == -1 && tiny[0] == '\0');
    CHECK(ico_host_pref_dir(data, sizeof(data)) == 0);
    CHECK(strcmp(data, ICO_HOST_VITA_DATA_DIR) == 0);
    CHECK(ico_host_pref_is_portable() == 0);
    CHECK(ico_host_ini_path(ini, sizeof(ini)) == 0);
    CHECK(snprintf(expected, sizeof(expected), "%s/ico-pc.ini", data) > 0);
    CHECK(strcmp(ini, expected) == 0);
    CHECK(ico_path_is_absolute("app0:/data/ico.o2r"));
    CHECK(ico_path_is_absolute("ux0:data/ico-pc/Ico_PAL.iso"));
    CHECK(ico_path_is_absolute("uma0:data/Ico_PAL.iso"));
    CHECK(!ico_path_is_absolute("relative/Ico_PAL.iso"));
    CHECK(ico_path_join(joined, sizeof(joined), data, "Ico_PAL.iso") == 0);
    CHECK(strcmp(joined, "host_config_vita_test_dir/Ico_PAL.iso") == 0);
    CHECK(ico_path_join(joined, sizeof(joined), "app0:", "data/ico.o2r") == 0);
    CHECK(strcmp(joined, "app0:/data/ico.o2r") == 0);
    CHECK(ico_rmdir(data) == 0);
    CHECK(ico_host_ini_path(ini, sizeof(ini)) == 0);
    {
        IcoIni settings;
        CHECK(ico_ini_load_layered(&settings, ini) == -1);
    }
    CHECK(ico_host_saves2_dir(joined, sizeof(joined)) == 0 && joined[0] == '\0');
    CHECK(ico_host_saves_dir(joined, sizeof(joined)) == 0);
    CHECK(strcmp(joined, "host_config_vita_test_dir/memcard") == 0);
    CHECK(ico_path_kind(data, NULL, NULL) < 0);
    if (failures != 0) {
        fprintf(stderr, "Vita host-config path regression: %d failures\n", failures);
        return 1;
    }
    puts("Vita host-config path regression passed");
    return 0;
}
