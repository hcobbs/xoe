# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Model Selection Strategy

This project uses different Claude models optimized for specific task types:

**Claude Opus 4.5** (`model="opus"`):
- Planning and architectural design (Plan agent)
- Code reviews and PR analysis
- Complex debugging and root cause analysis
- Critical design decisions requiring maximum reasoning capability

**Claude Sonnet 4.5** (`model="sonnet"`):
- General code development and implementation
- Feature implementation based on approved plans
- Code refactoring and improvements
- Most day-to-day coding tasks (default for implementation)

**Claude Haiku 4** (`model="haiku"`):
- Git operations (commits, branches, PRs)
- Simple file operations and quick fixes
- Trivial edits and repetitive tasks
- Fast turnaround tasks where speed matters

When spawning agents via the Task tool, Claude Code should select the appropriate model based on task complexity and type. This strategy optimizes for capability where it matters most while maintaining cost-effectiveness for routine work.

## Project Overview

XOE (X over Ethernet) is an extensible framework for encapsulation of various new and legacy protocols into Ethernet-transmissible data. Implements:
- Multi-threaded TCP server with echo functionality
- Serial connector for bridging TTY devices over Ethernet
- TLS/SSL support for secure connections

This project is also a testbed for human-LLM hybrid development. All contributions must be clearly labeled by category (see Git Workflow section for commit label types).

## Build Commands

- `make` - Build the xoe server for current OS (auto-detects and links appropriate libraries)
- `make clean` - Remove all build artifacts (obj/ and bin/ directories)

The built binary will be at `bin/xoe`.

## Running the Server

Server mode (default):
```bash
./bin/xoe                    # Listen on 0.0.0.0:12345
./bin/xoe -p 8080            # Listen on 0.0.0.0:8080
./bin/xoe -i 127.0.0.1 -p 8080  # Listen on specific interface
```

Client mode:
```bash
./bin/xoe -c 127.0.0.1:12345  # Connect to server at 127.0.0.1:12345
```

Serial client mode (bridge serial port to network):
```bash
# Basic serial bridge (9600 baud, 8N1, no flow control)
./bin/xoe -c 192.168.1.100:12345 -s /dev/ttyUSB0

# Full configuration
./bin/xoe -c 192.168.1.100:12345 \
  -s /dev/ttyUSB0 \
  -b 115200 \
  --parity none \
  --databits 8 \
  --stopbits 1 \
  --flow rtscts
```

Serial bridge topology:
```
[Local Serial] ↔ [XOE Client] ↔ [Network] ↔ [XOE Server] ↔ [XOE Client] ↔ [Remote Serial]
```

Testing with telnet:
```bash
telnet localhost 12345
```

## Git Workflow

**IMPORTANT**: All code changes must be developed on feature branches and submitted via pull requests. Do not commit code changes directly to `main`.

### Branch-Based Development

**Branch Naming Conventions**:
- `feature/<description>` - New features or enhancements
- `bugfix/<description>` - Bug fixes
- `refactor/<description>` - Code refactoring
- `test/<description>` - Test additions or improvements

**Commit Label Types**:
- `[CLASSIC]` - Traditional hand-coded implementation without AI assistance
- `[CLASSIC-REVIEW]` - Traditional human-run code review
- `[LLM-ASSISTED]` - Code written with LLM assistance (pair programming style)
- `[LLM-ARCH]` - Software architect leveraging LLM for code generation while reviewing, adjusting, and confirming all plans
- `[LLM-REVIEW]` - LLM-powered code review and resulting fixes
- `[VIBE]` - Experimental or exploratory coding

**Development Process**:
1. Create a new branch from `main`: `git checkout -b feature/your-feature-name`
2. Make your changes and commit with appropriate labels (see Commit Label Types above)
3. Push the branch: `git push -u origin feature/your-feature-name`
4. Create a pull request to merge into `main`
5. Ensure PR checklist is complete before merging

**Note**: Repository rules require all changes (including documentation) to go through pull requests. Direct commits to `main` are not allowed.

**Pre-PR Checklist**:
- [ ] Code builds successfully: `make`
- [ ] Code follows ANSI-C (C89) standards with `-std=c89 -pedantic`
- [ ] No compiler warnings with `-Wall -Wextra`
- [ ] All functions have complete documentation (purpose, parameters, return values, errors)
- [ ] Code has complete test coverage
- [ ] Helper functions are reusable and well-documented
- [ ] Commits include contribution label (see Commit Label Types)
- [ ] Changes align with Clean Code principles (see Coding Standards section)

## Architecture

### Language Standard
- **ANSI-C (C89)** (`-std=c89 -pedantic`) with strict warnings (`-Wall -Wextra`)
- Cross-platform support for Linux, macOS, and BSD (Unix/POSIX only)
- Fixed-width integer types (`uint16_t`, `uint32_t`) defined via typedefs (C89 doesn't have `stdint.h`)

### Directory Structure
```
src/
├── lib/                 Reusable library components
│   ├── common/          Common definitions and types
│   │   ├── types.h      - Fixed-width integer typedefs
│   │   └── definitions.h - Boolean defines, error codes
│   ├── protocol/        Protocol definitions
│   │   └── protocol.h   - xoe_packet_t, xoe_payload_t, protocol_handler_t
│   └── security/        TLS/SSL support (10 files)
│       ├── tls_config.h, tls_context.{c,h}
│       ├── tls_error.{c,h}, tls_io.{c,h}
│       ├── tls_session.{c,h}, tls_types.h
├── core/                Core XOE application
│   ├── main.c           Entry point (FSM loop)
│   ├── config.h         Configuration structures (xoe_config_t, xoe_state_t)
│   ├── server.{c,h}     Server implementation + client pool
│   └── fsm/             Finite state machine handlers (8 files)
│       ├── state_init.c, state_parse_args.c
│       ├── state_validate_config.c, state_mode_select.c
│       ├── state_server_mode.c, state_client_std.c
│       ├── state_client_serial.c, state_cleanup.c
├── connectors/          Pluggable protocol connectors
│   └── serial/          Serial TTY connector (9 files)
│       ├── serial_buffer.{c,h}   - Thread-safe circular buffer
│       ├── serial_client.{c,h}   - Multi-threaded serial bridge
│       ├── serial_config.h       - Configuration structures
│       ├── serial_port.{c,h}     - POSIX termios serial I/O
│       └── serial_protocol.{c,h} - XOE packet encapsulation
└── tests/               Test infrastructure
    ├── framework/       Test framework (2 files)
    └── unit/            Unit tests (4 TLS tests)
```

**Key Improvements** (Dec 2025 reorganization):
- Flat structure (max 3 levels deep) vs. previous 6-level nesting
- Single include path (`-Isrc`) vs. previous 6 paths
- Colocated headers and implementations (no c/h split)
- Clear module boundaries: lib/, core/, connectors/, tests/

### Build System
- Compiled objects go to `obj/`, binaries to `bin/`
- Makefile auto-detects OS and links platform-specific libraries:
  - Linux/macOS: `-lpthread`
  - TLS: `-lssl -lcrypto` (OpenSSL)
- Simplified build: 112 lines (down from 138), single `-Isrc` include path
- Auto-discovers all `.c` files via wildcards: `core/*.c`, `core/fsm/*.c`, `lib/*/*.c`, `connectors/*/*.c`

### Protocol Architecture (Planned)

The codebase defines a protocol abstraction layer (in `protocol.h`) but is not yet implemented:

- **xoe_packet_t**: Logical packet structure with protocol_id, protocol_version, payload, and checksum
- **protocol_handler_t**: Interface for pluggable protocol handlers with init_session, handle_data, and cleanup_session callbacks
- **xoe_payload_t**: Container for variable-length payload data

The current server implementation (`xoe.c`) is a simple echo server that will eventually be refactored to use this protocol handler architecture.

### Current Implementation

The application uses a finite state machine architecture:
- **Entry point**: `src/core/main.c` - Simple FSM loop (~54 lines)
- **Server logic**: `src/core/server.c` - Multi-threaded TCP/TLS server with client pool
- **State handlers**: `src/core/fsm/state_*.c` - 8 modular state handlers
  - `state_init` - Initialize defaults
  - `state_parse_args` - Command-line parsing
  - `state_validate_config` - Configuration validation
  - `state_mode_select` - Mode routing (server/client/serial)
  - `state_server_mode` - TCP/TLS server implementation
  - `state_client_std` - Standard interactive client
  - `state_client_serial` - Serial bridge client
  - `state_cleanup` - Resource cleanup

Server features:
- Multi-threaded with fixed-size client pool (MAX_CLIENTS concurrent connections)
- Per-client thread spawned via `handle_client()`
- Echo functionality: receives data and sends it back
- Optional TLS encryption (TLS 1.2/1.3)

### Serial Connector

The serial connector enables bidirectional TTY serial communication over Ethernet.

**Architecture**:
- **Three-thread design**:
  - Main thread: Initialization and shutdown coordination
  - Serial→Network thread: Reads from serial port, encapsulates into XOE packets, sends to network
  - Network→Serial thread: Receives from network, decapsulates packets, writes to serial via circular buffer

- **Flow Control**: 16KB circular buffer handles network→serial speed mismatch (~16 seconds at 9600 baud)

- **Protocol**: XOE_PROTOCOL_SERIAL (0x0001)
  - Packet format: protocol_id + version + payload (header + data) + checksum
  - Header: 4 bytes (flags + sequence number)
  - Max payload: 1020 bytes per packet
  - Simple sum checksum (upgradable to CRC32)

**Key Files**:
- `serial_config.h`: Configuration structures and constants
- `serial_port.c/h`: POSIX termios-based serial I/O (open, close, read, write, status, flush)
- `serial_protocol.c/h`: XOE packet encapsulation/decapsulation
- `serial_buffer.c/h`: Thread-safe circular buffer with condition variables
- `serial_client.c/h`: Multi-threaded serial bridge coordinator

**Features**:
- Runtime configuration via command-line arguments
- Support for standard baud rates (9600-230400)
- Configurable parity, data bits, stop bits, flow control (XON/XOFF, RTS/CTS)
- Comprehensive error logging with errno details
- Graceful shutdown on errors
- Detects and logs serial errors (parity, framing, overrun)

**Command-Line Options**:
- `-s <device>`: Serial device path (e.g., `/dev/ttyUSB0`)
- `-b <baud>`: Baud rate (default: 9600)
- `--parity <mode>`: none, even, odd (default: none)
- `--databits <bits>`: 7 or 8 (default: 8)
- `--stopbits <bits>`: 1 or 2 (default: 1)
- `--flow <mode>`: none, xonxoff, rtscts (default: none)

**Testing**:
Use `socat` to create virtual serial port pairs for integration testing:
```bash
# Create virtual serial port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Returns: /dev/ttys002 <-> /dev/ttys003

# Start XOE server
./bin/xoe -p 12345

# Start serial client on first virtual port
./bin/xoe -c localhost:12345 -s /dev/ttys002 -b 9600 &

# Start serial client on second virtual port
./bin/xoe -c localhost:12345 -s /dev/ttys003 -b 9600
```

## Code Formatting

**Indentation**:
- Use **4 spaces** for indentation (no tabs)
- Consistent indentation for all code blocks

**Line Length**:
- Preferred: 80-100 characters (soft limit)
- Maximum: 120 characters (hard limit)
- Break long lines at logical points (after commas, operators, etc.)

**General Style**:
- Opening braces on same line for functions and control structures
- Consistent spacing around operators and after keywords
- Clear, readable code structure preferred over compact formatting

## Coding Standards and Preferences

### Clean Code Principles
This project follows Clean Code principles with emphasis on:

- **Spaces for Alignment**: tabs are banned from use for alignment purposes.  Anything other than spaces used for alignment fail the code review and will not be accepted.
- **Maximum Code Width**: 120 Characters maximum line length. Use appropriate escapes to align variables of a function cleanly and clearly readable.
- **Reusable Helper Functions**: Prefer creating helper functions that can be reused multiple times rather than duplicating code
- **Complete Documentation**: All code must be thoroughly documented with clear comments explaining purpose, parameters, return values, and any side effects
- **Full Test Coverage**: All code should have complete test coverage to ensure reliability and maintainability

### Documentation Requirements
- Function headers should document purpose, parameters, return values, and error conditions
- Complex algorithms or business logic should include inline comments
- Non-obvious code sections should be explained

### Testing Requirements
- All functions should have corresponding unit tests
- Edge cases and error conditions must be tested
- Test coverage should be comprehensive and maintainable

## Testing Guidelines

### Running Tests

Before submitting a PR, ensure all tests pass:

```bash
make check    # Comprehensive validation (recommended)
```

Individual test targets:

```bash
make test-build        # Compile tests only
make test-unit         # Run unit tests
make test-integration  # Run integration tests
make test              # Run all tests
make test-verbose      # Detailed output
```

### Test Requirements for PRs

Pre-PR checklist requires:
- [ ] All tests compile (`make test-build`)
- [ ] All tests pass (`make test`)
- [ ] New code has corresponding unit tests
- [ ] Test coverage is comprehensive

### Common Test Issues

**Issue:** Tests fail to compile after refactoring
**Solution:** Check all test files have updated header includes to match new directory structure

**Issue:** Integration tests fail
**Solution:** Ensure `bin/xoe` is built and certificates exist in `./certs/` (run `./scripts/generate_test_certs.sh`)

**Issue:** TLS tests fail
**Solution:** Run `./scripts/generate_test_certs.sh` to create test certificates

### Test Documentation

For comprehensive testing documentation, see [docs/development/TESTING.md](docs/development/TESTING.md):
- Test framework reference (all assertion macros)
- Writing unit tests (templates, best practices)
- Integration tests (requirements, adding new tests)
- Debugging test failures
- Test coverage guidelines
