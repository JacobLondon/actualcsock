#
# Build test util for Unix-based stuff
#

CC=tcc
TARGET=test
CFLAGS=\
	-std=c99 \
	-pipe \
	-Wall \
	-Wextra \
	-pedantic \
	-Iinclude/tinycthread/source

FILES=\
	src/acs.c \
	src/test.c

all: $(TARGET)

link:
	$(CC) -o $(TARGET) src/test.c $(CFLAGS) \
	-LDebug/socketekcos.lib

# just compile the whole thing...
$(TARGET): $(FILES)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf test
