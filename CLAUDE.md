# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

XOE (X over Ethernet) is an extensible framework for encapsulation of various new and legacy protocols into Ethernet-transmissible data. Implements:
- Multi-threaded TCP server with echo functionality
- Serial connector for bridging TTY devices over Ethernet
- TLS/SSL support for secure connections

This project is also a testbed for human-LLM hybrid development. All contributions should be clearly labeled by category (classic coder, LLM-assisted coder, or vibe coder).

## Build Commands

- `make` - Build the xoe server for current OS (auto-detects and links appropriate libraries)
- `make clean` - Remove all build artifacts (obj/ and bin/ directories)

The built binary will be at `bin/xoe` (or `bin/xoe.exe` on Windows).

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

**Development Process**:
1. Create a new branch from `main`: `git checkout -b feature/your-feature-name`
2. Make your changes and commit with appropriate labels ([CLASSIC], [LLM-ASSISTED], or [VIBE])
3. Push the branch: `git push -u origin feature/your-feature-name`
4. Create a pull request to merge into `main`
5. Ensure PR checklist is complete before merging

**Exception**: Documentation-only changes (*.md files only) may be committed directly to `main`.

**Pre-PR Checklist**:
- [ ] Code builds successfully: `make`
- [ ] Code follows ANSI-C (C89) standards with `-std=c89 -pedantic`
- [ ] No compiler warnings with `-Wall -Wextra`
- [ ] All functions have complete documentation (purpose, parameters, return values, errors)
- [ ] Code has complete test coverage
- [ ] Helper functions are reusable and well-documented
- [ ] Commits include contribution label ([CLASSIC], [LLM-ASSISTED], or [VIBE])
- [ ] Changes align with Clean Code principles (see Coding Standards section)

## Architecture

### Language Standard
- **ANSI-C (C89)** (`-std=c89 -pedantic`) with strict warnings (`-Wall -Wextra`)
- Cross-platform support for Linux, macOS, and Windows
- Fixed-width integer types (`uint16_t`, `uint32_t`) defined via typedefs (C89 doesn't have `stdint.h`)

### Directory Structure
```
src/
├── common/              Common definitions used across project
│   └── h/commonDefinitions.h  - Boolean defines, error codes
├── core/
│   ├── packet_manager/  Protocol definitions
│   │   └── h/protocol.h - xoe_packet_t, xoe_payload_t, protocol_handler_t
│   └── server/
│       ├── net/         Network server implementation
│       │   ├── c/xoe.c  - Main server/client implementation
│       │   └── h/xoe.h  - Server configuration (port, buffer size)
│       ├── security/    TLS/SSL support
│       └── tests/       Test infrastructure
└── connector/
    └── serial/          Serial protocol connector (IMPLEMENTED)
        ├── c/
        │   ├── serial_buffer.c   - Thread-safe circular buffer
        │   ├── serial_client.c   - Multi-threaded serial bridge
        │   ├── serial_port.c     - POSIX termios serial I/O
        │   └── serial_protocol.c - XOE packet encapsulation
        └── h/
            ├── serial_buffer.h   - Buffer API
            ├── serial_client.h   - Client API
            ├── serial_config.h   - Configuration structures
            ├── serial_port.h     - Serial I/O API
            └── serial_protocol.h - Protocol definitions
```

### Build System
- Compiled objects go to `obj/`, binaries to `bin/`
- Makefile auto-detects OS and links platform-specific libraries:
  - Linux/macOS: `-lpthread`
  - Windows: `-lws2_32`
  - TLS: `-lssl -lcrypto` (OpenSSL)
- Builds all source files from `src/core/server/net/c/`, `src/core/server/security/c/`, and `src/connector/serial/c/`

### Protocol Architecture (Planned)

The codebase defines a protocol abstraction layer (in `protocol.h`) but is not yet implemented:

- **xoe_packet_t**: Logical packet structure with protocol_id, protocol_version, payload, and checksum
- **protocol_handler_t**: Interface for pluggable protocol handlers with init_session, handle_data, and cleanup_session callbacks
- **xoe_payload_t**: Container for variable-length payload data

The current server implementation (`xoe.c`) is a simple echo server that will eventually be refactored to use this protocol handler architecture.

### Current Implementation

The server (`src/core/server/net/c/xoe.c`) currently implements:
- Multi-threaded TCP server using POSIX threads
- Per-client thread spawned via `handle_client()`
- Echo functionality: receives data and sends it back
- Simple client mode for testing

Platform-specific network code is handled via `#ifdef _WIN32` blocks (Winsock2 vs BSD sockets).

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
- **Maximum Code Widt**: 120 Characters maximum line length. Use appropriate escapes to aling variables of a function cleanly and clearly readable.
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
