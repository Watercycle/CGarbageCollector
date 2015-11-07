# ------------------------------------------------
# Cache Simulator Makefile
# Date  : November 4, 2015
#
# @  => don't echo
# $  => variable/macro
# %  => a wildcard for pattern matching
# $@ => name of target (file being generated)
# $< => first prerequisite (usually source file)
# ------------------------------------------------

TARGET   = cachesim
CC       = gcc
CFLAGS   = -Wall -g -I.
LINKER   = gcc -o
LFLAGS   = -Wall -I. -lm

SRCDIR   = src
OBJDIR   = src
BINDIR   = .

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

##################
# Program
##################

# our target our default 'make'.
.DEFAULT_GOAL := $(TARGET)

$(BINDIR)/$(TARGET): $(OBJECTS)
	@$(LINKER) $@ $(LFLAGS) $(OBJECTS)
	@echo "Linking complete!"

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

##################
# Regression tests
##################

TEST_PROGRAMS = ./cachesim

all: $(TARGET) $(TEST_PROGRAMS)

test01: all
	./cachesim 8 1 1 < trace01.txt
	./cachesim 8 4 1 < trace01.txt
	./cachesim 16 1 1 < trace01.txt
	./cachesim 16 1 16 < trace01.txt
	./cachesim 16 1 32 < trace01.txt

test02: all
	./cachesim 16 1 1 < trace02.txt
	./cachesim 16 1 8 < trace02.txt
	./cachesim 16 1 16 < trace02.txt
	./cachesim 1024 1 1 < trace02.txt
	./cachesim 128 1 8 < trace02.txt

test03: all
	bunzip2 -c trace03.txt.bz2 | ./cachesim 128 1 8
	bunzip2 -c trace03.txt.bz2 | ./cachesim 128 8 16
	bunzip2 -c trace03.txt.bz2 | ./cachesim 512 8 32
	bunzip2 -c trace03.txt.bz2 | ./cachesim 1024 8 64

##################
# Clean up
##################

.PHONEY: clean
clean:
	@rm -rf $(OBJECTS) $(TEST_PROGRAMS) *.o *~ *.dSYM
	@echo "Cleanup complete!"

.PHONEY: remove
remove: clean
	@rm -f $(BINDIR)/$(TARGET)
	@echo "Executable removed!"