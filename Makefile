CC = gcc
CFLAGS = -I -g
OBJECT1 = oss.o
OBJECT2 = child.o
TARGET1 = oss
TARGET2 = child
CONFIG = config.h

PROCESS1 = oss.c
PROCESS2 = child.c

all: $(PROCESS1) $(TARGET2) $(CONFIG)
	$(CC) $(CFLAGS) $(PROCESS1) -o $(TARGET1) 

oss: $(PROCESS2) $(CONFIG)
	$(CC) $(CFLAGS) $(PROCESS2) -o $(TARGET2)

%.o: %.c $(CONFIG)
	$(CC) $(CFLAGS) -c $@ $<

clean: 
	@rm -f *.o oss child logfile.*
	rm *.txt
