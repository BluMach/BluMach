/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Engine selection reads a machine configuration without changing it. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <86box/coremach_engine.h>

FILE *
plat_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

static const char *path = "coremach-engine-test.cfg";

static void
write_config(const char *text)
{
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    assert(fwrite(text, 1, strlen(text), fp) == strlen(text));
    assert(fclose(fp) == 0);
}

static char *
read_config(size_t *size)
{
    FILE *fp = fopen(path, "rb");
    char *data;
    long  length;

    assert(fp != NULL);
    assert(fseek(fp, 0, SEEK_END) == 0);
    length = ftell(fp);
    assert(length >= 0);
    assert(fseek(fp, 0, SEEK_SET) == 0);
    data = calloc(1, (size_t) length + 1);
    assert(data != NULL);
    assert(fread(data, 1, (size_t) length, fp) == (size_t) length);
    assert(fclose(fp) == 0);
    *size = (size_t) length;
    return data;
}

static blumach_engine_t
detect(const char *text, char *system, size_t system_size)
{
    blumach_engine_t engine;
    size_t           size;
    char            *after;

    write_config(text);
    engine = blumach_detect_engine(path, system, system_size);
    after  = read_config(&size);
    assert(size == strlen(text));
    assert(!memcmp(after, text, size));
    free(after);
    return engine;
}

int
main(void)
{
    char system[BLUMACH_COREMACH_SYSTEM_MAX];

    /* 86Box configurations keep the 86Box core. */
    assert(detect("[Machine]\nmachine = olivetti_m15\n", system, sizeof(system)) == BLUMACH_ENGINE_86BOX);
    assert(system[0] == '\0');
    assert(detect("", system, sizeof(system)) == BLUMACH_ENGINE_86BOX);
    assert(detect("# [CoreMach]\n; [CoreMach]\n[CoreMachine]\nsystem = x\n", system, sizeof(system))
           == BLUMACH_ENGINE_86BOX);
    assert(detect("[coremach]\nsystem = olivetti-m10\n", system, sizeof(system)) == BLUMACH_ENGINE_86BOX);

    /* A [CoreMach] section selects CoreMach wherever it appears. */
    assert(detect("[CoreMach]\nsystem = olivetti-m10\nram_kib = 32\n", system, sizeof(system))
           == BLUMACH_ENGINE_COREMACH);
    assert(!strcmp(system, "olivetti-m10"));
    assert(detect("\xef\xbb\xbf[General]\nvid_renderer = qt_software\r\n\r\n  [CoreMach]\r\n\tsystem=olivetti-m99  \r\n",
                  system, sizeof(system))
           == BLUMACH_ENGINE_COREMACH);
    assert(!strcmp(system, "olivetti-m99"));

    /* Without a system it is still CoreMach, never the 86Box core. */
    assert(detect("[CoreMach]\nram_kib = 32\n[Machine]\nsystem = olivetti-m10\n", system, sizeof(system))
           == BLUMACH_ENGINE_COREMACH);
    assert(system[0] == '\0');
    assert(detect("[CoreMach]\n", NULL, 0) == BLUMACH_ENGINE_COREMACH);

    /* Long names are truncated, keys only match exactly. */
    assert(detect("[CoreMach]\nsystems = a\nsystem = olivetti-m10-with-a-long-name\n", system, 9)
           == BLUMACH_ENGINE_COREMACH);
    assert(!strcmp(system, "olivetti"));

    /* A missing file keeps the 86Box behaviour of config_load(). */
    assert(remove(path) == 0);
    assert(blumach_detect_engine(path, system, sizeof(system)) == BLUMACH_ENGINE_86BOX);
    assert(blumach_detect_engine(NULL, system, sizeof(system)) == BLUMACH_ENGINE_86BOX);
    return 0;
}
