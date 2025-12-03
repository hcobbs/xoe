# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

XOE (X over Ethernet) is an extensible framework for encapsulation of various new and legacy protocols into Ethernet-transmissible data. Currently implements a basic TCP server that echoes received data back to clients.

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
│   ├── packet_manager/  Protocol definitions (not yet implemented)
│   │   └── h/protocol.h - xoe_packet_t, xoe_payload_t, protocol_handler_t
│   └── server/
│       ├── net/         Network server implementation
│       │   ├── c/xoe.c  - Main server/client implementation
│       │   └── h/xoe.h  - Server configuration (port, buffer size)
│       ├── security/    Security features (stub)
│       └── tests/       Test infrastructure (stub)
└── connector/
    └── serial/          Serial protocol connector (stub)
```

### Build System
- Compiled objects go to `obj/`, binaries to `bin/`
- Makefile auto-detects OS and links platform-specific libraries:
  - Linux/macOS: `-lpthread`
  - Windows: `-lws2_32`
- Currently only builds files from `src/core/server/net/c/`

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
