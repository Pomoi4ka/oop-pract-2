INCLUDES = -Ilibco -Ievloop2
CFLAGS   = -Wall -Wextra -pedantic -std=c89 -pipe -g -MMD -MP $(INCLUDES)
LIBS     = -lssl -lcrypto -ljson-c

SRCS := $(shell find src/ -type f -name '*.c')
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)

.PHONY: all

all: search

search: $(OBJS) libco/libco.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) libco/libco.o $(LIBS)

src/http.o: INCLUDES += -Isrc/io/
src/wikipedia.o: INCLUDES += -Isrc/io/

libco/libco.o: CFLAGS =
libco/libco.o: libco/libco.c

-include $(DEPS)
