#define ELOOP_HTTP_IMPLEMENTATION
#define ELOOP_SSL_IMPLEMENTATION
#define ELOOP_NET_IMPLEMENTATION
#define EVENT_LOOP2_IMPLEMENTATION
#include "eloop_http.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <json-c/json.h>

#define HOST "ru.wikipedia.org"
#define PORT 443

int64_t stdin_cb(void *d)
{
    (void) d;
    return STDIN_FILENO;
}

char *urlencode(const char *p)
{
    char *e = malloc(strlen(p)*3 + 1);
    char *c = e;
    for (; *p; ++p) {
        unsigned char q = *p;
        if (isalnum(q)) {
            *c++ = q;
            continue;
        }
        *c++ = '%';
        *c++ = (q>>4) < 10 ? (q>>4) + '0' : (q>>4) + 'A' - 10;
        *c++ = (q&0xf) < 10 ? (q&0xf) + '0' : (q&0xf) + 'A' - 10;
    }
    *c = 0;
    return e;
}

void spinner(void)
{
    int i;
    ev_set_timeout(100);

    printf(" ");
    for (i = 0;;) {
        printf("\b%c", "-\\|/"[i++%4]);
        fflush(stdout);
        if (ev_recv_message()) break;
    }
    printf("\r\x1b[K");
}

ev_task *spinner_start(const char *message)
{
    event_loop *loop = ev_get_loop();
    printf("%s", message);
    return eloop_add(loop, spinner);
}

void spinner_stop(ev_task *sp)
{
    ev_send_message(sp, (void *)1);
}

enum wiki_resp_status {
    WRS_OK,
    WRS_CONNECTION_FAILED,
    WRS_INVALID_RESPONSE,
    WRS_TRANSPORT_FAILURE
};

struct wikipedia_resp {
    enum wiki_resp_status status;
    json_object *payload;
};

struct wikipedia_req_closure {
    struct wikipedia_resp *resp;
    const char *encoded_query;
    ev_task *parent;
    essl_ctx_t ssl;
};

void wikipedia_request(enet_socket *sock)
{
    struct wikipedia_req_closure *reqc;
    struct http_resp resp;
    jmp_buf err;
    http_transport http;
    json_object *jobj = NULL;
    json_tokener *jtk = NULL;
    char *buf = NULL;
    size_t read;
    int len;
    reqc = esock_get_data(sock);

    if (https_connect(&http, reqc->ssl, sock, &err) < 0) {
        reqc->resp->status = WRS_CONNECTION_FAILED;
        goto fini;
    }

    switch (setjmp(err)) {
    case HTTP_ERR_NONE: break;
    case HTTP_ERR_TRANSPORT:
    case HTTP_ERR_PARSE:
    case HTTP_ERR_PARSE_LIMIT:
        reqc->resp->status = WRS_TRANSPORT_FAILURE;
        goto fini;
    }

    len = snprintf(NULL, 0, "/w/api.php?action=query&list=search&utf8=&format=json&srsearch=%%22%s%%22", reqc->encoded_query);
    buf = malloc(len + 1);
    snprintf(buf, len + 1, "/w/api.php?action=query&list=search&utf8=&format=json&srsearch=%%22%s%%22", reqc->encoded_query);
    http_send_req(&http, "GET", buf);
    free(buf);
    buf = NULL;
    http_send_header(&http, "Host", HOST);
    http_send_header(&http, "User-Agent", "curl/8.0");
    http_send_header(&http, "Connection", "closed");
    http_end_request(&http);

    http_recv_resp(&http, &resp, NULL);

    if (resp.status_code != 200) {
        reqc->resp->status = WRS_INVALID_RESPONSE;
        goto fini;
    }

    read = 0;
    if (resp.content_length > 0) {
        len = 4096;
        buf = malloc(len);
    }
    jtk = json_tokener_new();
    do {
        char chunk[16] = {0};
        if (resp.content_length > 0) {
            if ((size_t)resp.content_length < read + len)
                len = resp.content_length - read;
        } else {
            /* this is hackish, thank you http, very cool */
            unsigned short term = '\r'|('\n'<<8);
            char *c = chunk;
            do {
                http_recv(&http, c++, 1);
                if ((size_t)(c - chunk) > sizeof chunk) {
                    reqc->resp->status = WRS_TRANSPORT_FAILURE;
                    goto fini;
                }
            } while (c - chunk < 2 || ((unsigned short*)c)[-1] != term);
            len = strtol(chunk, NULL, 16);
            free(buf);
            buf = malloc(len);
        }
        http_recv(&http, buf, len);
        read += len;
        if (resp.content_length < 0) http_recv(&http, chunk, 2); /* extra \r\n */
        jobj = json_tokener_parse_ex(jtk, buf, len);
    } while (!jobj);

    reqc->resp->status = WRS_OK;
    reqc->resp->payload = jobj;

 fini:
    free(buf);
    if (jtk) json_tokener_free(jtk);
    ev_send_message(reqc->parent, NULL);
}

void request_wikipedia(struct wikipedia_resp *resp, const char *encoded, essl_ctx_t ssl)
{
    event_loop *loop = ev_get_loop();
    /* since this request function is blocking for the caller, the
       request closure can be safely located on the current stack
       frame */
    struct wikipedia_req_closure reqc;

    reqc.resp = resp;
    reqc.encoded_query = encoded;
    reqc.parent = ev_current();
    reqc.ssl = ssl;

    enet_connect_tcp(loop, HOST, PORT, wikipedia_request, &reqc);
    ev_recv_message();
}

void strip(char **line)
{
    int len;
    while (isspace(**line)) (*line)++;
    len = strlen(*line);
    while (len && isspace((*line)[len-1])) (*line)[--len] = 0;
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
            if (ev_yield() & EV_F_EXCEPTIONS) return;
            line = fgets(buffer, sizeof buffer, stdin);
            if (!line) return;
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
