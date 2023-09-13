CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -g -I/  

all: writer

writer: writer.o
	$(CC) $(CFLAGS) -o writer writer.o
	chmod +x writer

writer.o: writer.c
	$(CC) $(CFLAGS) -c writer.c

clean:
	rm -f writer writer.o


