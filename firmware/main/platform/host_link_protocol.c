#include <stdio.h>
#include <string.h>
#include "host_link.h"

const char *host_link_command_name(host_link_command_t command)
{
    switch (command) {
    case HOST_LINK_CMD_PLAY_PAUSE: return "play_pause";
    case HOST_LINK_CMD_PREVIOUS:   return "previous";
    case HOST_LINK_CMD_NEXT:       return "next";
    case HOST_LINK_CMD_STOP:       return "stop";
    case HOST_LINK_CMD_SEEK_TO:    return "seek_to";
    case HOST_LINK_CMD_SET_VOLUME: return "set_volume";
    default: return NULL;
    }
}

/* Appends a JSON string literal (quoted, escaped). Returns new offset or -1. */
static int put_json_string(char *buf, size_t len, int off, const char *s)
{
    if (off < 0 || (size_t)off >= len) return -1;
    size_t o = (size_t)off;
    if (o + 1 >= len) return -1;
    buf[o++] = '"';
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        char esc[12];
        size_t n;
        if (*p == '"' || *p == '\\') { esc[0] = '\\'; esc[1] = (char)*p; n = 2; }
        else if (*p < 0x20) { n = (size_t)snprintf(esc, sizeof(esc), "\\u%04x", *p); }
        else { esc[0] = (char)*p; n = 1; }
        if (o + n + 1 >= len) return -1;
        memcpy(buf + o, esc, n);
        o += n;
    }
    if (o + 1 >= len) return -1;
    buf[o++] = '"';
    buf[o] = '\0';
    return (int)o;
}

static int put_raw(char *buf, size_t len, int off, const char *s)
{
    if (off < 0) return -1;
    size_t n = strlen(s);
    if ((size_t)off + n + 1 > len) return -1;
    memcpy(buf + off, s, n + 1);
    return off + (int)n;
}

int host_link_format_hello(char *buf, size_t len, const char *token, const char *device_id)
{
    if (!buf || len == 0 || !token) return -1;
    int o = put_raw(buf, len, 0, "{\"type\":\"hello\",\"token\":");
    o = put_json_string(buf, len, o, token);
    o = put_raw(buf, len, o, ",\"device_id\":");
    o = put_json_string(buf, len, o, device_id ? device_id : "round-remote");
    o = put_raw(buf, len, o, "}");
    return o;
}

int host_link_format_command(char *buf, size_t len, const char *request_id,
                             host_link_command_t command, float value)
{
    const char *name = host_link_command_name(command);
    if (!buf || len == 0 || !name || !request_id || !request_id[0] || strlen(request_id) > 64) return -1;
    int o = put_raw(buf, len, 0, "{\"type\":\"command\",\"id\":");
    o = put_json_string(buf, len, o, request_id);
    o = put_raw(buf, len, o, ",\"command\":");
    o = put_json_string(buf, len, o, name);
    if (command == HOST_LINK_CMD_SEEK_TO || command == HOST_LINK_CMD_SET_VOLUME) {
        if (value != value) return -1;   /* NaN */
        if (command == HOST_LINK_CMD_SEEK_TO && (value < 0.0f || value > 86400.0f)) return -1;
        if (command == HOST_LINK_CMD_SET_VOLUME && (value < 0.0f || value > 1.0f)) return -1;
        char num[32];
        snprintf(num, sizeof(num), ",\"value\":%.3f", (double)value);
        o = put_raw(buf, len, o, num);
    }
    o = put_raw(buf, len, o, "}");
    return o;
}

host_link_playback_t host_link_playback_from_string(const char *status)
{
    if (!status) return HOST_LINK_PLAYBACK_UNAVAILABLE;
    if (strcmp(status, "playing") == 0) return HOST_LINK_PLAYBACK_PLAYING;
    if (strcmp(status, "paused") == 0) return HOST_LINK_PLAYBACK_PAUSED;
    if (strcmp(status, "stopped") == 0) return HOST_LINK_PLAYBACK_STOPPED;
    return HOST_LINK_PLAYBACK_UNAVAILABLE;
}
