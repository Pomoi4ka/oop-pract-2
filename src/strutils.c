#include "strutils.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

void strip(char **line)
{
    int len;
    while (isspace(**line)) (*line)++;
    len = strlen(*line);
    while (len && isspace((*line)[len-1])) (*line)[--len] = 0;
}
