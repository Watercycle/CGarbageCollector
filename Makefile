# ------------------------------------------------
# Cache C Garbage Collector Makefile
# Date  : November 4, 2015
#
# .PHONY => target is not a physical file
# .DEFAULT_GOAL => explicit, default 'make' target
# @  => don't echo
# $  => variable/macro
# %  => 'wildcard' for pattern matching
# $@ => name of target (file being generated)
# $< => first prerequisite (usually source file)
# ------------------------------------------------

TARGET   = c-garbage-collector
CC       = gcc
CFLAGS   = -Wall -O0 -g -I.
LINKER   = gcc -o
LFLAGS   = -Wall -I. -lm

SRCDIR   = src
OBJDIR   = src
BINDIR   = .

##################
# Program
##################

.DEFAULT_GOAL := $(TARGET)

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

$(BINDIR)/$(TARGET): $(OBJECTS)
	@$(LINKER) $@ $(LFLAGS) $(OBJECTS)
	@echo "Linking complete!"

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

##################
# Regression tests
##################

all: $(TARGET)

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