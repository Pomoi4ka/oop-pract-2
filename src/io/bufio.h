#ifndef BUFIO_H_
#define BUFIO_H_

#ifndef BUFIODEF
#define BUFIODEF
#endif

#include <stddef.h>

struct bufio {
    void *handler;
    int (*read)(void *, void *, int);
    void (*write)(void *, const void *, int);
    int rb_avail, rb_pos;
    int wb_avail, wb_pos;
    char readbuf[4096];
    char writebuf[4096];
};

BUFIODEF void bio_init(struct bufio *, void *, int (*)(void *, void *, int), void (*)(void *, const void *, int));
BUFIODEF void bio_read(struct bufio *, void *, int);
BUFIODEF void bio_send(struct bufio *, const void *, int);
BUFIODEF int  bio_pending(struct bufio *);
BUFIODEF void bio_printf(struct bufio *, const char *, ...);
BUFIODEF int  bio_printf_r(struct bufio *, char *, size_t, const char *, ...);
BUFIODEF void bio_flush(struct bufio *);
BUFIODEF int  bio_read_until(struct bufio *, void *, const char*, size_t);

#endif /* BUFIO_H_ */
