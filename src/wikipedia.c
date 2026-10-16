#define _XOPEN_SOURCE 500

#include "wikipedia.h"
#include "eloop_http.h"

#include <stdio.h>

#define HOST "ru.wikipedia.org"
#define PORT 443

struct wikipedia_req_closure {
    struct wikipedia_resp *resp;
    const char *encoded_query;
    ev_task *parent;
    essl_ctx_t ssl;
};

static void wikipedia_request(enet_socket *sock)
{
    struct wikipedia_req_closure *reqc;
    struct http_resp resp;
    jmp_buf err;
    http_transport http;
    json_object *jobj = NULL;
    json_tokener *volatile jtk = NULL;
    char *volatile buf = NULL;
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
