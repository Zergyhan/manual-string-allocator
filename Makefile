
CFLAGS = -Wall

OBJS = tests.o stralloc.o

all: tests

.c.o:
	$(CC) $(CFLAGS) -c $<

tests: $(OBJS)
	$(CC) -o $@ $(OBJS)

$(OBJS): stralloc.h
