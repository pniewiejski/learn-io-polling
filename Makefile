CC = gcc
CCVERSION = $(shell $(CC) -dumpversion)
CFLAGS = -Wall -Werror

# Define all C source files using Make wildcard funciton
# Wildcards are expanded by the shell.
# NOTE: 
#    wildcard expansion does not happen when a veriable is defined
#      SOURCES = *.c 
#    Then the value of SOURCES variable is *.c
#    When SRC will be used in target or prerequisite, expansion will take place.
#    If you want to set SOURCES to its expansion use
#      SOURCES := $(wildcard *.c)
SOURCES := $(wildcard *_socket.c)
COMMON_SOURCES = common.c

# Use Substiturtion Reference to get rid to the .c suffix.
EXEC := $(SOURCES:.c=)

all: $(EXEC) $(COMMON_SOURCES)
	$(info Build all execs: $(EXEC))

%: %.c
	$(info Using $(CC) version: $(CCVERSION))
	$(info Building $@ exec)
	$(CC) $(CFLAGS) -o $@ $(COMMON_SOURCES) $<

.PHONY: clean
clean:
	rm -f $(EXEC)