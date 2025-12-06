# Makefile for XOE

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c89 -pedantic -DTLS_ENABLED=1

# Address Sanitizer flags (enabled with make asan or make test-asan)
ASAN_FLAGS = -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
ASAN_CFLAGS = $(CFLAGS) $(ASAN_FLAGS)

# Directories
SRCDIR   = src
BINDIR   = bin
OBJDIR   = obj
LIBDIR   = $(SRCDIR)/lib
COREDIR  = $(SRCDIR)/core
CONNECTORDIR = $(SRCDIR)/connectors
TESTDIR  = $(SRCDIR)/tests

# Source files (auto-discover all .c files in new structure)
SOURCES = $(wildcard $(COREDIR)/*.c)
SOURCES += $(wildcard $(COREDIR)/fsm/*.c)
SOURCES += $(wildcard $(COREDIR)/mgmt/*.c)
SOURCES += $(wildcard $(LIBDIR)/*/*.c)
SOURCES += $(wildcard $(CONNECTORDIR)/*/*.c)

# Object files (flatten all to obj/)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Include paths (simplified to single -Isrc)
INCLUDES = -I$(SRCDIR)

# Target executable
TARGET_NAME = xoe
TARGET = $(BINDIR)/$(TARGET_NAME)

# Default libraries
LIBS =

# OS detection and specific settings
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    LIBS += -lpthread -lssl -lcrypto -lusb-1.0
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
    # Add libusb-1.0 (usually from Homebrew)
    HOMEBREW_LIBUSB := $(shell brew --prefix libusb 2>/dev/null)
    ifneq ($(HOMEBREW_LIBUSB),)
        INCLUDES += -I$(HOMEBREW_LIBUSB)/include/libusb-1.0
        LIBS += -L$(HOMEBREW_LIBUSB)/lib -lusb-1.0
    else
        LIBS += -lusb-1.0
    endif
endif
ifeq ($(UNAME_S),FreeBSD)
    LIBS += -lpthread -lssl -lcrypto -lusb
    INCLUDES += -I/usr/local/include
    LIBS += -L/usr/local/lib
endif
ifeq ($(UNAME_S),OpenBSD)
    LIBS += -lpthread -lssl -lcrypto -lusb-1.0
    INCLUDES += -I/usr/local/include
    LIBS += -L/usr/local/lib
endif
ifeq ($(UNAME_S),NetBSD)
    LIBS += -lpthread -lssl -lcrypto -lusb-1.0
    INCLUDES += -I/usr/pkg/include
    LIBS += -L/usr/pkg/lib
endif

# Default target
.PHONY: all
all: $(TARGET)

# Rule to link the final executable
$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	@echo "Successfully built: $@"

# Pattern rule to compile all source files
# Object files maintain directory structure under obj/
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test configuration
TEST_FRAMEWORK = $(TESTDIR)/framework/test_framework.c
TEST_SOURCES = $(wildcard $(TESTDIR)/unit/test_*.c)
TEST_BINARIES = $(patsubst $(TESTDIR)/unit/test_%.c,$(BINDIR)/test_%,$(TEST_SOURCES))

# Separate object file for test framework
TEST_FRAMEWORK_OBJ = $(OBJDIR)/test_framework.o

# Application objects (without main.o which contains main())
APP_TEST_OBJECTS = $(filter-out $(OBJDIR)/core/main.o,$(OBJECTS))

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

# Test build target - compile tests without running them
.PHONY: test-build
test-build: $(TEST_BINARIES)
	@echo "All tests compiled successfully"

# Unit tests only
.PHONY: test-unit
test-unit: $(TEST_BINARIES)
	@echo "=== Running Unit Tests ==="
	@echo ""
	@for test in $(TEST_BINARIES); do \
		$$test || exit 1; \
		echo ""; \
	done

# Integration tests only
.PHONY: test-integration
test-integration: $(TARGET)
	@echo "=== Running Integration Tests ==="
	@echo ""
	@./scripts/test_integration.sh

# Test with verbose output
.PHONY: test-verbose
test-verbose: $(TEST_BINARIES)
	@echo ""
	@echo "========================================"
	@echo " XOE Test Suite"
	@echo "========================================"
	@echo ""
	@echo "--- Unit Tests ---"
	@for test in $(TEST_BINARIES); do \
		echo ""; \
		$$test || exit 1; \
	done
	@echo ""
	@echo "--- Integration Tests ---"
	@./scripts/test_integration.sh
	@echo ""
	@echo "========================================"
	@echo " All Tests Passed ✓"
	@echo "========================================"

# Check target - build and test, fail on any error (GNU standard)
.PHONY: check
check: all test-build
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
	@echo ""
	@echo "✓ All checks passed"

# Pattern rule for test binaries
$(BINDIR)/test_%: $(TESTDIR)/unit/test_%.c $(TEST_FRAMEWORK_OBJ) $(APP_TEST_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(TESTDIR) $< $(TEST_FRAMEWORK_OBJ) $(APP_TEST_OBJECTS) -o $@ $(LIBS)
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

# Address Sanitizer build targets
.PHONY: asan
asan: clean
	@echo "Building with Address Sanitizer..."
	@$(MAKE) all CFLAGS="$(ASAN_CFLAGS)" LIBS="$(LIBS) $(ASAN_FLAGS)"

.PHONY: test-asan
test-asan: clean
	@echo "Building and testing with Address Sanitizer..."
	@$(MAKE) test CFLAGS="$(ASAN_CFLAGS)" LIBS="$(LIBS) $(ASAN_FLAGS)"

.PHONY: test-leaks
test-leaks: test-asan
	@echo ""
	@echo "=== Memory Leak Detection Report ==="
	@echo "Tests completed with Address Sanitizer enabled"
	@echo "Check output above for any memory leaks or errors"
