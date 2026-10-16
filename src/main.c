#include "eloop_ssl.h"

#include <unistd.h>

void userloop(void);

static int64_t stdin_cb(void *d)
{
    (void) d;
    return STDIN_FILENO;
}

static void run(void) {
    ev_set_data_to_transport_handle_cb(stdin_cb);
    ev_set_flags(EV_F_READ);
    ev_set_timeout(-1);
    for (;;) userloop();
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
