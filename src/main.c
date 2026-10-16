#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "strutils.h"
#include "wikipedia.h"
#include "spinner.h"

static int64_t stdin_cb(void *d)
{
    (void) d;
    return STDIN_FILENO;
}

void run(void) {
    struct wikipedia_resp resp;
    essl_ctx_t ssl = ev_get_data();
    char *encoded, *line;
    char buffer[256];
    ev_task *spinner;

    ev_set_data_to_transport_handle_cb(stdin_cb);
    ev_set_flags(EV_F_READ);
    ev_set_timeout(-1);

    encoded = NULL;
    for (;;) {
        json_object *walk;
        size_t i;
        printf("> ");
        fflush(stdout);
        if (ev_yield() & EV_F_EXCEPTIONS) return;
        line = fgets(buffer, sizeof buffer, stdin);
        if (!line) return;
        strip(&line);

        encoded = urlencode(line);
        spinner = spinner_start("Searching... ");
        request_wikipedia(&resp, encoded, ssl);
        spinner_stop(spinner);
        free(encoded);

        switch (resp.status) {
        case WRS_OK: break;
        case WRS_CONNECTION_FAILED: fprintf(stderr, "error: connection failed\n"); continue;
        case WRS_INVALID_RESPONSE: fprintf(stderr, "error: invalid response\n"); continue;
        case WRS_TRANSPORT_FAILURE: fprintf(stderr, "error: transport failure\n"); continue;
        }

        walk = resp.payload;
        if (!json_object_object_get_ex(walk, "query", &walk)) goto cleanup;
        if (!json_object_object_get_ex(walk, "search", &walk)) goto cleanup;

        for (i = 0; i < json_object_array_length(walk); ++i) {
            json_object *title;
            json_object *result = json_object_array_get_idx(walk, i);
            if (!json_object_object_get_ex(result, "title", &title)) continue;
            if (json_object_get_type(title) != json_type_string) continue;
            printf("%ld: %s\n", i + 1, json_object_get_string(title));
        }

        for (;;) {
            printf("select page> ");
            fflush(stdout);
            if (ev_yield() & EV_F_EXCEPTIONS) goto cleanup;
            line = fgets(buffer, sizeof buffer, stdin);
            if (!line) goto cleanup;
            strip(&line);

            i = strtol(line, NULL, 10);
            i -= 1;
            if (i < json_object_array_length(walk)) break;
            printf("entered number is not in the list\n");
        }
        walk = json_object_array_get_idx(walk, i);
        if (!json_object_object_get_ex(walk, "pageid", &walk)) {
            fprintf(stderr, "error: wikipedia api changed\n");
            continue;
        }
        if (json_object_get_type(walk) != json_type_int) {
            fprintf(stderr, "error: wikipedia api changed\n");
            continue;
        }
        snprintf(buffer, sizeof buffer, "xdg-open https://ru.wikipedia.org/w/index.php?curid=%d", json_object_get_int(walk));
        system(buffer);

    cleanup:
        json_object_put(resp.payload);
    }
}

int main(void)
{
    event_loop loop;
    essl_ctx_t ssl;
    ev_task *t;

    eloop_init(&loop);
    ssl = essl_init();

    t = eloop_add(&loop, run);
    ev_set_task_data(t, ssl);

    eloop_run(&loop);
    essl_deinit(ssl);
    return 0;
}
