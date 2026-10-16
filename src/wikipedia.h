#ifndef WIKIPEDIA_H_
#define WIKIPEDIA_H_

#include <json-c/json.h>

#include "eloop_ssl.h"

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

void request_wikipedia(struct wikipedia_resp *resp, const char *encoded, essl_ctx_t ssl);

#endif
