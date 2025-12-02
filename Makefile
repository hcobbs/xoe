# Makefile for XOE

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c89 -pedantic

# Directories
SRCDIR   = src
BINDIR   = bin
OBJDIR   = obj
COMMONDIR= $(SRCDIR)/common
COREDIR  = $(SRCDIR)/core
NETDIR   = $(COREDIR)/server/net
SECDIR   = $(COREDIR)/server/security
PACKETDIR= $(COREDIR)/packet_manager


# Source files
# Automatically find all .c files
SOURCES = $(wildcard $(NETDIR)/c/*.c)
SOURCES += $(wildcard $(SECDIR)/c/*.c)

# Object files
# Create a corresponding .o file in the OBJDIR for each .c file
OBJECTS = $(patsubst $(NETDIR)/c/%.c,$(OBJDIR)/%.o,$(wildcard $(NETDIR)/c/*.c))
OBJECTS += $(patsubst $(SECDIR)/c/%.c,$(OBJDIR)/%.o,$(wildcard $(SECDIR)/c/*.c))

# Include paths for headers
INCLUDES = -I$(NETDIR)/h -I$(SECDIR)/h -I$(PACKETDIR)/h -I$(COMMONDIR)/h

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
    LIBS += -lpthread -lssl -lcrypto
endif
ifeq ($(UNAME_S),Darwin) # macOS
    LIBS += -lpthread
    # Check for Homebrew OpenSSL installation
    HOMEBREW_OPENSSL := $(shell brew --prefix openssl 2>/dev/null)
    ifneq ($(HOMEBREW_OPENSSL),)
        # Use Homebrew OpenSSL
        INCLUDES += -I$(HOMEBREW_OPENSSL)/include
        LIBS += -L$(HOMEBREW_OPENSSL)/lib -lssl -lcrypto
    else
        # Fall back to system LibreSSL
        LIBS += -lssl -lcrypto
    endif
endif
ifneq (,$(findstring NT,$(UNAME_S))) # Windows (e.g., MINGW)
    TARGET := $(TARGET).exe
    LIBS += -lws2_32
    # OpenSSL on Windows (via vcpkg or manual install)
    # Adjust OPENSSL_ROOT path as needed
    OPENSSL_ROOT ?= C:/vcpkg/installed/x64-windows
    INCLUDES += -I$(OPENSSL_ROOT)/include
    LIBS += -L$(OPENSSL_ROOT)/lib -lssl -lcrypto
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

# Pattern rule to compile security source files
$(OBJDIR)/%.o: $(SECDIR)/c/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Rule to clean up build artifacts
.PHONY: clean
clean:
	@echo "Cleaning up build files..."
	-rm -rf $(OBJDIR) $(BINDIR)
	@echo "Done."