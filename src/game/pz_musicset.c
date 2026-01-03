/*
 * Tank Game - Musicset file parsing
 */

#include "pz_musicset.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"

static const char *
pz_musicset_skip_whitespace(const char *s)
{
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static const char *
pz_musicset_read_line(const char **text, char *buf, size_t buf_size)
{
    const char *p = *text;
    if (!p || !*p) {
        return NULL;
    }

    size_t i = 0;
    while (*p && *p != '\n' && *p != '\r' && i < buf_size - 1) {
        buf[i++] = *p++;
    }
    buf[i] = '\0';

    while (*p && (*p == '\n' || *p == '\r')) {
        p++;
    }

    *text = p;
    return buf;
}

static void
pz_musicset_rtrim(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        len--;
    }
    s[len] = '\0';
}

static bool
pz_musicset_is_absolute_path(const char *path)
{
    if (!path || !*path) {
        return false;
    }
    if (path[0] == '/') {
        return true;
    }
    if (strlen(path) > 1 && path[1] == ':') {
        return true;
    }
    return false;
}

static void
pz_musicset_join_path(
    char *out, size_t out_len, const char *base_dir, const char *file)
{
    if (!out || out_len == 0) {
        return;
    }
    if (!file || !*file) {
        out[0] = '\0';
        return;
    }
    if (pz_musicset_is_absolute_path(file) || !base_dir || !*base_dir) {
        snprintf(out, out_len, "%s", file);
        return;
    }
    snprintf(out, out_len, "%s/%s", base_dir, file);
}

static bool
pz_musicset_parse_role(const char *role, pz_music_role *out_role)
{
    if (!role || !out_role) {
        return false;
    }
    if (strcmp(role, "base") == 0) {
        *out_role = PZ_MUSIC_ROLE_BASE;
        return true;
    }
    if (strcmp(role, "intensity1") == 0) {
        *out_role = PZ_MUSIC_ROLE_INTENSITY1;
        return true;
    }
    if (strcmp(role, "intensity2") == 0) {
        *out_role = PZ_MUSIC_ROLE_INTENSITY2;
        return true;
    }
    return false;
}

pz_musicset *
pz_musicset_load(const char *path)
{
    if (!path) {
        return NULL;
    }

    char *file_data = pz_file_read_text(path);
    if (!file_data) {
        pz_log(
            PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "Failed to read musicset: %s", path);
        return NULL;
    }

    char base_dir[PZ_MUSICSET_PATH_LEN] = "";
    const char *slash = strrchr(path, '/');
    if (slash) {
        size_t len = (size_t)(slash - path);
        if (len >= sizeof(base_dir)) {
            len = sizeof(base_dir) - 1;
        }
        memcpy(base_dir, path, len);
        base_dir[len] = '\0';
    }

    pz_musicset *set
        = (pz_musicset *)pz_calloc_tagged(1, sizeof(pz_musicset), PZ_MEM_GAME);
    if (!set) {
        pz_free(file_data);
        return NULL;
    }

    set->bpm = 120.0f;
    set->layer_count = 0;
    set->has_victory = false;
    set->victory_channel = 0;

    const char *text = file_data;
    char line[512];
    while (pz_musicset_read_line(&text, line, sizeof(line))) {
        const char *p = pz_musicset_skip_whitespace(line);
        if (!*p || *p == '#') {
            continue;
        }

        if (strncmp(p, "name ", 5) == 0) {
            strncpy(set->name, p + 5, sizeof(set->name) - 1);
            set->name[sizeof(set->name) - 1] = '\0';
            pz_musicset_rtrim(set->name);
        } else if (strncmp(p, "bpm ", 4) == 0) {
            set->bpm = (float)atof(p + 4);
        } else if (strncmp(p, "layer ", 6) == 0) {
            if (set->layer_count >= PZ_MUSICSET_MAX_LAYERS) {
                pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO,
                    "Too many layers in musicset: %s", path);
                continue;
            }

            char role_str[32];
            char file_str[128];
            char rest[256] = "";
            int count = sscanf(
                p + 6, "%31s %127s %255[^\n]", role_str, file_str, rest);
            if (count < 2) {
                continue;
            }

            pz_music_role role;
            if (!pz_musicset_parse_role(role_str, &role)) {
                pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO,
                    "Unknown music role '%s' in %s", role_str, path);
                continue;
            }

            int channel = 0;
            float volume = 1.0f;
            if (count >= 3) {
                char *token = strtok(rest, " \t");
                while (token) {
                    if (strncmp(token, "channel=", 8) == 0) {
                        channel = atoi(token + 8);
                    } else if (strncmp(token, "volume=", 7) == 0) {
                        volume = (float)atof(token + 7);
                    }
                    token = strtok(NULL, " \t");
                }
            }

            pz_musicset_layer *layer = &set->layers[set->layer_count++];
            layer->role = role;
            layer->channel = channel;
            layer->volume = volume;
            pz_musicset_join_path(
                layer->midi_path, sizeof(layer->midi_path), base_dir, file_str);
        } else if (strncmp(p, "victory ", 8) == 0) {
            char file_str[128];
            char rest[256] = "";
            int count = sscanf(p + 8, "%127s %255[^\n]", file_str, rest);
            if (count >= 1) {
                int channel = 0;
                if (count == 2) {
                    char *token = strtok(rest, " \t");
                    while (token) {
                        if (strncmp(token, "channel=", 8) == 0) {
                            channel = atoi(token + 8);
                        }
                        token = strtok(NULL, " \t");
                    }
                }
                set->has_victory = true;
                set->victory_channel = channel;
                pz_musicset_join_path(set->victory_path,
                    sizeof(set->victory_path), base_dir, file_str);
            }
        }
    }

    pz_free(file_data);

    if (set->bpm <= 0.0f) {
        set->bpm = 120.0f;
    }

    if (set->layer_count == 0) {
        pz_log(
            PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "Musicset has no layers: %s", path);
    }

    return set;
}

void
pz_musicset_destroy(pz_musicset *set)
{
    if (!set) {
        return;
    }
    pz_free(set);
}
