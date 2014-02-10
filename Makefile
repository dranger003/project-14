cc=gcc
#CFLAGS=-g -c -Wall -m64 -Ofast -flto -march=native -funroll-loops -DLINUX
CFLAGS=-g -c -Wall -mthumb -O3 -march=armv7-a -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon -mvectorize-with-neon-quad -mfloat-abi=hard -DLINUX
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
