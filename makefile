# Change these two paths if necessary
GCC_INC := $(shell gcc -print-file-name=plugin)/include
GCC_VER := 4.8

PLUGIN  := array_checker.so
SRCS    := array_checker.c

CFLAGS  := -Wall -Wextra -O0 -shared -fPIC -std=c++0x \
           -I$(GCC_INC) -I$(GCC_INC)/..         # tree.h lives one dir up

all: $(PLUGIN)

$(PLUGIN): $(SRCS)
	g++ $(CFLAGS) $^ -o $@

clean:
	rm -f $(PLUGIN) *.o
