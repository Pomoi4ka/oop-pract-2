#define _XOPEN_SOURCE 500
#include <stdio.h>

#include "strutils.h"
#include "wikipedia.h"
#include "spinner.h"

static char *prompt(const char *prompt, char *buf, size_t len)
{
    char *line;
    printf("%s> ", prompt);
    fflush(stdout);
    if (ev_yield() & EV_F_EXCEPTIONS) return NULL;
    line = fgets(buf, len, stdin);
    if (!line) return NULL;
    strip(&line);
    return line;
}

static json_object *search(const char *line)
{
    struct wikipedia_resp resp;
    ev_task *spinner;
    essl_ctx_t ssl = ev_get_data();
    char *encoded = urlencode(line);
    spinner = spinner_start("Searching... ");
    request_wikipedia(&resp, encoded, ssl);
    spinner_stop(spinner);
    free(encoded);

    switch (resp.status) {
    case WRS_OK: return resp.payload;
    case WRS_CONNECTION_FAILED: fprintf(stderr, "error: connection failed\n"); break;
    case WRS_INVALID_RESPONSE: fprintf(stderr, "error: invalid response\n"); break;
    case WRS_TRANSPORT_FAILURE: fprintf(stderr, "error: transport failure\n"); break;
    }
    return NULL;
}

static json_object *select_variant(json_object *payload)
{
    size_t i;
    const char *line;
    json_object *walk;
    char buffer[256];
    walk = payload;
    if (!json_object_object_get_ex(walk, "query", &walk)) return NULL;
    if (!json_object_object_get_ex(walk, "search", &walk)) return NULL;

    for (;;) {
        line = prompt("select page", buffer, sizeof buffer);
        if (!line) return NULL;

        if (*line == 'q') return NULL;
        i = strtol(line, NULL, 10);
        i -= 1;
        if (i < json_object_array_length(walk)) break;
        printf("entered number is not in the list\n");
    }

    walk = json_object_array_get_idx(walk, i);
    if (!json_object_object_get_ex(walk, "pageid", &walk)) {
        fprintf(stderr, "error: wikipedia api changed\n");
        return NULL;
    }
    if (json_object_get_type(walk) != json_type_int) {
        fprintf(stderr, "error: wikipedia api changed\n");
        return NULL;
    }
    return walk;
}

static int display_variants(json_object *payload)
{
    size_t i;
    json_object *walk;
    walk = payload;
    if (!json_object_object_get_ex(walk, "query", &walk)) return 0;
    if (!json_object_object_get_ex(walk, "search", &walk)) return 0;

    for (i = 0; i < json_object_array_length(walk); ++i) {
        json_object *title;
        json_object *result = json_object_array_get_idx(walk, i);
        if (!json_object_object_get_ex(result, "title", &title)) continue;
        if (json_object_get_type(title) != json_type_string) continue;
        printf("%ld: %s\n", i + 1, json_object_get_string(title));
    }
    return 1;
}

static void open_in_browser(json_object *obj)
{
    char buffer[256];
    snprintf(buffer, sizeof buffer, "xdg-open https://ru.wikipedia.org/w/index.php?curid=%d", json_object_get_int(obj));
    system(buffer);
}

void userloop(void)
{
    json_object *selected, *payload;
    char *line;
    char buffer[256];

    line = prompt("", buffer, sizeof buffer);
    if (!line) ev_terminate();

    payload = search(line);
    if (!payload) return;
    if (!display_variants(payload)) goto cleanup;
    selected = select_variant(payload);
    if (!selected) goto cleanup;
    open_in_browser(selected);

 cleanup:
    json_object_put(payload);
}
