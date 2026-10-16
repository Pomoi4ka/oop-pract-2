#include "spinner.h"

#include <stdio.h>
#include <unistd.h>

static void spinner(void)
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
