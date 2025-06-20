# Makefile for mtws

CC = gcc
CFLAGS = -Wall -pthread -g
TARGET = mtws
OBJS = mtws.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

mtws.o: mtws.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS)