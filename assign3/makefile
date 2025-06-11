CC = gcc
CFLAGS = -pthread -O2
TARGET = rwlock

all: $(TARGET)

$(TARGET): rwlock.c
	$(CC) $(CFLAGS) -o $(TARGET) rwlock.c

clean:
	rm -f $(TARGET)