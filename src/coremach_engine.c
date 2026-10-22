/*
 * BluMach engine selection for a machine configuration.
 *
 * The scan follows the line rules of the 86Box INI reader (leading blanks,
 * '#' and ';' comments, exact section names, keys ending at '=' or a blank)
 * but only looks for the [CoreMach] section, so it never builds or writes an
 * 86Box configuration.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>
#include <string.h>
#include <86box/coremach_engine.h>
#include <86box/plat.h>

blumach_engine_t blumach_engine = BLUMACH_ENGINE_86BOX;
char             blumach_coremach_system[BLUMACH_COREMACH_SYSTEM_MAX];

static char *
skip_blanks(char *text)
{
    while ((*text == ' ') || (*text == '\t'))
        text++;
    return text;
}

static void
strip_line_end(char *line)
{
    size_t length = strlen(line);

    while ((length > 0) && ((line[length - 1] == '\n') || (line[length - 1] == '\r')))
        line[--length] = '\0';
}

blumach_engine_t
blumach_detect_engine(const char *cfg_path, char *system, size_t system_size)
{
    blumach_engine_t engine     = BLUMACH_ENGINE_86BOX;
    int              in_section = 0;
    int              first_line = 1;
    char             line[1024];
    FILE            *fp;

    if ((system != NULL) && (system_size > 0))
        system[0] = '\0';
    if ((cfg_path == NULL) || (cfg_path[0] == '\0'))
        return engine;
    fp = plat_fopen(cfg_path, "rb");
    if (fp == NULL)
        return engine;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *text = line;

        /* Skip a UTF-8 byte order mark on the first line. */
        if (first_line && !memcmp(text, "\xef\xbb\xbf", 3))
            text += 3;
        first_line = 0;
        strip_line_end(text);
        text = skip_blanks(text);
        if ((*text == '\0') || (*text == '#') || (*text == ';'))
            continue;

        if (*text == '[') {
            char *end = strchr(text + 1, ']');
            if (end == NULL)
                continue;
            *end       = '\0';
            in_section = !strcmp(text + 1, BLUMACH_COREMACH_SECTION);
            if (in_section)
                engine = BLUMACH_ENGINE_COREMACH;
            continue;
        }

        if (in_section && (system != NULL) && (system_size > 0)) {
            size_t key_length = strcspn(text, "= ");
            if ((key_length == 6) && !strncmp(text, "system", 6) && (text[key_length] != '\0')) {
                char  *value = text + key_length;
                size_t value_length;

                while ((*value == '=') || (*value == ' '))
                    value++;
                value_length = strlen(value);
                while ((value_length > 0) && ((value[value_length - 1] == ' ') || (value[value_length - 1] == '\t')))
                    value[--value_length] = '\0';
                snprintf(system, system_size, "%s", value);
            }
        }
    }

    fclose(fp);
    return engine;
}
