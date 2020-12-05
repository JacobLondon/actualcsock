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
	-Iinclude/tinycthread/source \
	-lpthread \
	-lm

FILES=\
	include/tinycthread/source/tinycthread.c \
	src/acs.c \
	src/acs_sync.c \
	src/list.c \
	src/test.c

all: $(TARGET)

# just compile the whole thing...
$(TARGET): $(FILES)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf $(TARGET)
