#ifndef SPINNER_H_
#define SPINNER_H_

#include "event_loop2.h"

ev_task *spinner_start(const char *message);
void spinner_stop(ev_task *sp);

#endif
