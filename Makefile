CC=clang

local_CFLAGS = -I./libs/include/ -I./ -I./src/
debug:   local_CFLAGS := -DDEBUG -DDIAGNOSTICS -DSLOW $(local_CFLAGS) $(CFLAGS)
release: local_CFLAGS := -O3 -march=native -DRELEASE -DFAST $(local_CFLAGS) $(CFLAGS)

local_LDFLAGS = -L./libs/lib/ -lglfw

PLATFORM = unix

ifeq ($(shell uname -s), Darwin)
	local_LDFLAGS := $(local_LDFLAGS) -framework OpenGL
else
	local_LDFLAGS := $(local_LDFLAGS) -lgl
endif

local_LDFLAGS := $(local_LDFLAGS) $(LDFLAGS)

TARGET = bench

all: debug

debug:   src
release: src

src: clean
	$(CC) src/platform/$(PLATFORM).c -I./ -I./src/ -o $(TARGET) $(local_CFLAGS) $(local_LDFLAGS)

clean:
	@rm -f $(TARGET)
.PHONY: all debug release src clean
