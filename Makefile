INCLUDES = -Ilibco -Ievloop2
CFLAGS   = -Wall -Wextra -pedantic -std=c89 -pipe -g -MMD -MP $(INCLUDES)
LIBS     = -lssl -lcrypto -ljson-c

SRCS := $(shell find src/ -type f -name '*.c')
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)
SRCS_WITH_HEADERS := $(shell find src/ -type f -name '*.h') $(SRCS)

.PHONY: all pdf

all: search pdf

search: $(OBJS) libco/libco.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) libco/libco.o $(LIBS)

src/http.o: INCLUDES += -Isrc/io/
src/wikipedia.o: INCLUDES += -Isrc/io/

libco/libco.o: CFLAGS =
libco/libco.o: libco/libco.c

-include $(DEPS)

pdf: report.pdf

report.pdf: assets/source.tex assets/uml.png report.tex \
	assets/uml.png \
	assets/wikipedia-cli-request-fsm.png
	pdflatex report.tex
	pdflatex report.tex

assets/source.tex: $(SRCS_WITH_HEADERS)
	($(foreach src,$(SRCS_WITH_HEADERS), \
printf "\n\\\texttt{$(src):}\n\n\\\begin{lstlisting}[language=C]\n" | sed 's|_|\\_|g'; \
cat $(src); \
printf "\\\end{lstlisting}\n";)) > $@


%.png: %.dot
	dot -Tpng -o $@ $< || :
