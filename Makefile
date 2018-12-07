
CC	= gcc
CFLAGS	= -g -lrt
TARGET1	= oss
TARGET2	= user
OBJS1	= oss.o header.h
OBJS2	= user.o header.h

.SUFFIXES: .c .o

all: $(TARGET1) $(TARGET2)

oss: $(OBJS1)
	$(CC) $(CFLAGS) $(OBJS1) -o $@

user: $(OBJS2)
	$(CC) $(CFLAGS) $(OBJS2) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean

clean: 
	/bin/rm -f *.o *~ *.log $(TARGET1) $(TARGET2)
