PROG := fvncd

bindir := /usr/local/sbin
srcdir := src

CC := gcc
CCOPT := -O2
DEFS :=
INCLS :=
LIBS :=

syslog_facility := daemon

CFLAGS = $(CCOPT) $(DEFS) $(INCLS)
LDFLAGS = $(LIBS)

PREPROCESSING := usage.h

CSRC := daemon.c \
        fvncd.c \
        hcs.c \
        listener.c \
        rfb.c \
        usage.c \
        util.c

OBJ := $(CSRC:.c=.o)
OBJ_DEBUG := $(CSRC:.c=_debug.o)

CLEANFILES := $(PROG) $(OBJ:%=src/%)

all: $(PROG)

$(PROG): $(PREPROCESSING) $(OBJ)
	@rm -f $(PROG)
	$(CC) -o $(PROG) $(OBJ:%=src/%) $(LDFLAGS)

daemon.o:
	@rm -f $(srcdir)/$@
	$(CC) $(CFLAGS) -o $(srcdir)/$@ -c $(srcdir)/daemon.c
fvncd.o:
	@rm -f $(srcdir)/$@
	$(CC) $(CFLAGS) -o $(srcdir)/$@ -c $(srcdir)/fvncd.c
hcs.o:
	@rm -f $(srcdir)/$@
	$(CC) $(CFLAGS) -o $(srcdir)/$@ -c $(srcdir)/hcs.c
listener.o:
	@rm -f $(srcdir)/$@
	$(CC) $(CFLAGS) -o $(srcdir)/$@ -c $(srcdir)/listener.c
rfb.o:
	@rm -f $(srcdir)/$@
	$(CC) $(CFLAGS) -o $(srcdir)/$@ -c $(srcdir)/rfb.c
usage.o:
	@rm -f $(srcdir)/$@
	$(CC) $(CFLAGS) -o $(srcdir)/$@ -c $(srcdir)/usage.c
util.o:
	@rm -f $(srcdir)/$@
	$(CC) $(CFLAGS) -o $(srcdir)/$@ -c $(srcdir)/util.c

debug: $(PREPROCESSING) $(OBJ_DEBUG)
	@rm -f $(PROG)
	$(CC) -o $(PROG) $(OBJ_DEBUG:%_debug.o=src/%.o) $(LDFLAGS)

daemon_debug.o:
	@rm -f $(srcdir)/$(@:_debug.o=.o)
	$(CC) $(CFLAGS) -DDEBUG -o $(srcdir)/$(@:_debug.o=.o) -c $(srcdir)/daemon.c
fvncd_debug.o:
	@rm -f $(srcdir)/$(@:_debug.o=.o)
	$(CC) $(CFLAGS) -DDEBUG -o $(srcdir)/$(@:_debug.o=.o) -c $(srcdir)/fvncd.c
hcs_debug.o:
	@rm -f $(srcdir)/$(@:_debug.o=.o)
	$(CC) $(CFLAGS) -DDEBUG -o $(srcdir)/$(@:_debug.o=.o) -c $(srcdir)/hcs.c
listener_debug.o:
	@rm -f $(srcdir)/$(@:_debug.o=.o)
	$(CC) $(CFLAGS) -DDEBUG -o $(srcdir)/$(@:_debug.o=.o) -c $(srcdir)/listener.c
rfb_debug.o:
	@rm -f $(srcdir)/$(@:_debug.o=.o)
	$(CC) $(CFLAGS) -DDEBUG -o $(srcdir)/$(@:_debug.o=.o) -c $(srcdir)/rfb.c
usage_debug.o:
	@rm -f $(srcdir)/$(@:_debug.o=.o)
	$(CC) $(CFLAGS) -DDEBUG -o $(srcdir)/$(@:_debug.o=.o) -c $(srcdir)/usage.c
util_debug.o:
	@rm -f $(srcdir)/$(@:_debug.o=.o)
	$(CC) $(CFLAGS) -DDEBUG -o $(srcdir)/$(@:_debug.o=.o) -c $(srcdir)/util.c

usage.h: $(srcdir)/VERSION
	@sed -e 's/.*/#define VERSION "&"/' $(srcdir)/VERSION > $(srcdir)/$@

install:
	cp -f $(PROG) $(bindir)/

clean:
	rm -f $(CLEANFILES)
