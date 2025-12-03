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
FSMDIR   = $(NETDIR)/fsm
SECDIR   = $(COREDIR)/server/security
TESTDIR  = $(COREDIR)/server/tests
PACKETDIR= $(COREDIR)/packet_manager
SERIALDIR= $(SRCDIR)/connector/serial


# Source files
# Automatically find all .c files
SOURCES = $(wildcard $(NETDIR)/c/*.c)
SOURCES += $(wildcard $(FSMDIR)/c/*.c)
SOURCES += $(wildcard $(SECDIR)/c/*.c)
SOURCES += $(wildcard $(SERIALDIR)/c/*.c)

# Object files
# Create a corresponding .o file in the OBJDIR for each .c file
OBJECTS = $(patsubst $(NETDIR)/c/%.c,$(OBJDIR)/%.o,$(wildcard $(NETDIR)/c/*.c))
OBJECTS += $(patsubst $(FSMDIR)/c/%.c,$(OBJDIR)/%.o,$(wildcard $(FSMDIR)/c/*.c))
OBJECTS += $(patsubst $(SECDIR)/c/%.c,$(OBJDIR)/%.o,$(wildcard $(SECDIR)/c/*.c))
OBJECTS += $(patsubst $(SERIALDIR)/c/%.c,$(OBJDIR)/%.o,$(wildcard $(SERIALDIR)/c/*.c))

# Include paths for headers
INCLUDES = -I$(NETDIR)/h -I$(FSMDIR)/h -I$(SECDIR)/h -I$(PACKETDIR)/h -I$(COMMONDIR)/h -I$(SERIALDIR)/h

# Target executable
TARGET_NAME = xoe
TARGET = $(BINDIR)/$(TARGET_NAME)

# Default libraries
LIBS =

# OS detection and specific settings
# POSIX systems only (Linux, macOS, BSD)
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

# Pattern rule to compile FSM state handler source files
$(OBJDIR)/%.o: $(FSMDIR)/c/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Pattern rule to compile security source files
$(OBJDIR)/%.o: $(SECDIR)/c/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Pattern rule to compile serial connector source files
$(OBJDIR)/%.o: $(SERIALDIR)/c/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test configuration
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_FRAMEWORK = $(TESTDIR)/test_framework.c
TEST_BINARIES = $(filter-out $(BINDIR)/test_framework,$(patsubst $(TESTDIR)/test_%.c,$(BINDIR)/test_%,$(TEST_SOURCES)))

# Separate object files for test framework
TEST_FRAMEWORK_OBJ = $(OBJDIR)/test_framework.o

# Security objects (without xoe.o which contains main())
SEC_TEST_OBJECTS = $(filter-out $(OBJDIR)/xoe.o,$(OBJECTS))

# Test target - run all unit tests
.PHONY: test
test: $(TEST_BINARIES)
	@echo ""
	@echo "=== Running Unit Tests ==="
	@echo ""
	@for test in $(TEST_BINARIES); do \
		$$test || exit 1; \
		echo ""; \
	done
	@echo "=== Running Integration Tests ==="
	@echo ""
	@./scripts/test_integration.sh

# Pattern rule for test binaries
# Each test links with test framework, security modules, but NOT xoe.o (which has main())
$(BINDIR)/test_%: $(TESTDIR)/test_%.c $(TEST_FRAMEWORK_OBJ) $(SEC_TEST_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(TESTDIR) $< $(TEST_FRAMEWORK_OBJ) $(SEC_TEST_OBJECTS) -o $@ $(LIBS)
	@echo "Built test: $@"

# Compile test framework object
$(TEST_FRAMEWORK_OBJ): $(TEST_FRAMEWORK)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(TESTDIR) -c $< -o $@

# Rule to clean up build artifacts
.PHONY: clean
clean:
	@echo "Cleaning up build files..."
	-rm -rf $(OBJDIR) $(BINDIR)
	@echo "Done."