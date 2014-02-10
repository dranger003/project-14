cc=gcc
CFLAGS=-g -c -Wall -m64 -Ofast -flto -march=native -funroll-loops -DLINUX
LDFLAGS=-lavformat -lavcodec -lavutil -lavdevice -lao
SRCS=main.c
OBJS=$(SRCS:.c=.o)
TARGET=project-14

all: $(SRCS) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -fr $(OBJS) $(TARGET)
