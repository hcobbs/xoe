# Makefile for XOE

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99

# Directories
SRCDIR   = src
BINDIR   = bin
OBJDIR   = obj
COMMONDIR= $(SRCDIR)/common
COREDIR  = $(SRCDIR)/core
NETDIR   = $(COREDIR)/server/net
PACKETDIR= $(COREDIR)/packet_manager


# Source files
# Automatically find all .c files
SOURCES = $(wildcard $(NETDIR)/c/*.c)

# Object files
# Create a corresponding .o file in the OBJDIR for each .c file
OBJECTS = $(patsubst $(NETDIR)/c/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Include paths for headers
INCLUDES = -I$(NETDIR)/h -I$(PACKETDIR)/h -I$(COMMONDIR)/h

# Target executable
TARGET_NAME = xoe
TARGET = $(BINDIR)/$(TARGET_NAME)

# Default libraries
LIBS =

# OS detection and specific settings
# This uses `uname` which is common on POSIX systems (Linux, macOS) and
# developer environments on Windows like Git Bash or MSYS2.
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    LIBS += -lpthread
endif
ifeq ($(UNAME_S),Darwin) # macOS
    LIBS += -lpthread
endif
ifneq (,$(findstring NT,$(UNAME_S))) # Windows (e.g., MINGW)
    TARGET := $(TARGET).exe
    LIBS += -lws2_32
endif

# Default target executed when you just run `make`
.PHONY: all
all: $(TARGET)

# Rule to link the final executable
$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	@echo "Successfully built: $@"

# Pattern rule to compile .c source files into .o object files
$(OBJDIR)/%.o: $(NETDIR)/c/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Rule to clean up build artifacts
.PHONY: clean
clean:
	@echo "Cleaning up build files..."
	-rm -rf $(OBJDIR) $(BINDIR)
	@echo "Done."