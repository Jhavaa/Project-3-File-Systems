# this is the Makefile to compile test cases

CC     = gcc
OPTS   = -O -Wall 
INCS   = 
LIBS   = -L. -lFS -lDisk
SHLIBS = libDisk.so libFS.so

SRCS   = main.c \
	simple-test.c \
	slow-ls.c slow-mkdir.c slow-rmdir.c \
	slow-touch.c slow-rm.c \
	slow-cat.c slow-import.c slow-export.c \
        slow-export-not-open-neg.c \
        slow-import_offset_size.c \
        slow-import_offset_large_size.c

OBJS   = $(SRCS:.c=.o)
TARGETS = $(SRCS:.c=.exe)

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJS) *~

reset:	clean
	make -f Makefile.LibDisk clean
	make -f Makefile.LibFS clean

%.o: %.c
	$(CC) $(INCS) $(OPTS) -c $< -o $@

%.exe: %.o $(SHLIBS)
	$(CC) -o $@ $< $(LIBS)

libDisk.so:	LibDisk.h LibDisk.c
	make -f Makefile.LibDisk

libFS.so:	LibFS.h LibFS.c
	make -f Makefile.LibFS
