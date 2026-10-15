#define _XOPEN_SOURCE 500
#include "bufio.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

static void bio_fetch(struct bufio *bio)
{
    int avail = sizeof bio->readbuf - bio->rb_avail - bio->rb_pos;
    int n = bio->read(bio->handler, &bio->readbuf[bio->rb_pos + bio->rb_avail], avail);
    bio->rb_avail += n;
}

BUFIODEF void bio_read(struct bufio *bio, void *buf, int size)
{
    int pos = 0;
    while (pos < size) {
        int n;
        if (bio->rb_avail == 0) bio_fetch(bio);

        n = size - pos;
        if (n >= bio->rb_avail) n = bio->rb_avail;
        assert(n >= 0);
        memcpy((char*)buf + pos, &bio->readbuf[bio->rb_pos], n);
        pos += n;
        bio->rb_pos = (bio->rb_pos + n) % sizeof bio->readbuf;
        bio->rb_avail -= n;
    }
}

BUFIODEF void bio_send(struct bufio *bio, const void *buf, int size)
{
    int pos = 0;
    while (pos < size) {
        int n, free;
        if (bio->wb_pos + bio->wb_avail == sizeof bio->writebuf)
            bio_flush(bio);

        n = size - pos;
        free = (int) sizeof bio->writebuf - bio->wb_pos - bio->wb_avail;
        if (n >= free) n = free;
        assert(n >= 0);
        memcpy(&bio->writebuf[bio->wb_pos + bio->wb_avail], (char*)buf + pos, n);
        pos += n;
        bio->wb_avail += n;
    }
}

BUFIODEF void bio_flush(struct bufio *bio)
{
    bio->write(bio->handler, &bio->writebuf[bio->wb_pos], bio->wb_avail);
    bio->wb_pos = (bio->wb_pos + bio->wb_avail) % sizeof bio->writebuf;
    bio->wb_avail = 0;
}

BUFIODEF void bio_init(struct bufio *bio, void *hnd,
        int (*read)(void *, void *, int),
        void (*write)(void *, const void *, int))
{
    memset(bio, 0, sizeof(*bio));
    bio->handler = hnd;
    bio->read = read;
    bio->write = write;
}

static int buf_ends_with(const char *buf, size_t size,
        const char *pat, size_t pat_len)
{
    if (size < pat_len) return 0;
    if (memcmp(&buf[size - pat_len], pat, pat_len) != 0) return 0;
    return 1;
}

BUFIODEF int bio_read_until(struct bufio *bio, void *_buf,
        const char *pattern, size_t limit)
{
    char *buf = _buf;
    size_t size = 0, pat_len = strlen(pattern);

    while (size < limit && !buf_ends_with(buf, size, pattern, pat_len))
        bio_read(bio, &buf[size++], 1);

    if (size < limit) return size;
    return -1;
}

static int bio_printf_rv(struct bufio *bio, char *buf, size_t n, const char *fmt, va_list va)
{
    int ret = vsnprintf(buf, n, fmt, va);
    if (ret < 0 || (size_t) ret >= n) return -1;
    bio_send(bio, buf, n);
    return ret;
}

BUFIODEF int bio_printf_r(struct bufio *bio, char *buf, size_t n, const char *fmt, ...)
{
    va_list va;
    int ret;

    va_start(va, fmt);
    ret = bio_printf_rv(bio, buf, n, fmt, va);
    va_end(va);
    return ret;
}

BUFIODEF void bio_printf(struct bufio *bio, const char *fmt, ...)
{
    static char buf[4096];
    va_list va;
    int ret;

    va_start(va, fmt);
    ret = bio_printf_rv(bio, buf, sizeof buf, fmt, va);
    assert(ret >= 0);
    va_end(va);
}
