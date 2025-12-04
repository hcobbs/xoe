# XOE - X over Ethernet

Meet Xoe... An extensible framework for encapsulation of various new and legacy protocols into Ethernet-transmissible data.

**Current Status**: Echo server with TLS 1.2/1.3 support
**Version**: 1.0
**License**: (To be determined)

---

## Table of Contents

1. [Features](#features)
2. [Quick Start](#quick-start)
3. [Installation](#installation)
4. [Usage](#usage)
5. [Documentation](#documentation)
6. [Development](#development)
7. [Contributing](#contributing)
8. [Project Goals](#project-goals)

---

## Features

- ✅ **Multi-threaded TCP Server** - Handles up to 32 concurrent connections
- ✅ **Runtime Encryption Selection** - Choose between plain TCP, TLS 1.2, or TLS 1.3
- ✅ **Strong Security** - TLS 1.3 with modern cipher suites (AES-GCM, ChaCha20-Poly1305)
- ✅ **Cross-Platform** - Supports Linux, macOS, BSD (Unix/POSIX only)
- ✅ **C89 Compliant** - Strict ANSI C for maximum compatibility
- ✅ **Echo Protocol** - Simple echo server for testing and development
- 🚧 **Extensible Architecture** - Protocol handler framework (in development)

---

## Quick Start

### Prerequisites

- GCC or Clang compiler
- OpenSSL 1.1.1+ or 3.x
- Make

### Build and Run

```bash
# Clone repository
git clone <repository-url> xoe
cd xoe

# Build
make

# Generate test certificates
./scripts/generate_test_certs.sh

# Run server with TLS 1.3
./bin/xoe -e tls13

# In another terminal, test with OpenSSL client
echo "Hello World" | openssl s_client -connect localhost:12345 -tls1_3 -CAfile ./certs/server.crt -quiet
```

---

## Installation

### Build from Source

```bash
make clean && make
```

**Supported Platforms**:
- Linux (any modern distribution)
- macOS 10.15+
- FreeBSD / OpenBSD / NetBSD

**Windows is NOT supported** (POSIX only)

### Install System-Wide

```bash
sudo install -m 755 bin/xoe /usr/local/bin/
```

See [DEPLOYMENT.md](DEPLOYMENT.md) for production installation instructions.

---

## Usage

### Server Mode

**Plain TCP** (no encryption):
```bash
./bin/xoe -e none
./bin/xoe -e none -i 127.0.0.1 -p 8080
```

**TLS 1.3** (recommended):
```bash
./bin/xoe -e tls13
./bin/xoe -e tls13 -p 8443 -cert /path/to/cert.pem -key /path/to/key.pem
```

**TLS 1.2** (legacy compatibility):
```bash
./bin/xoe -e tls12
```

### Client Mode

```bash
./bin/xoe -c 127.0.0.1:12345
./bin/xoe -c 192.168.1.100:8080
```

### All Command-Line Options

```
Usage: xoe [OPTIONS]

Server Mode:
  -i <interface>    Network interface to bind (default: 0.0.0.0)
  -p <port>         Port to listen on (default: 12345)
  -e <mode>         Encryption: none, tls12, tls13 (default: none)
  -cert <path>      Server certificate (default: ./certs/server.crt)
  -key <path>       Private key (default: ./certs/server.key)

Client Mode:
  -c <ip>:<port>    Connect as client

General:
  -h                Show help message
```

### Testing with Standard Tools

**OpenSSL**:
```bash
# TLS 1.3 connection
openssl s_client -connect localhost:12345 -tls1_3 -CAfile ./certs/server.crt

# TLS 1.2 connection
openssl s_client -connect localhost:12345 -tls1_2 -CAfile ./certs/server.crt
```

**Telnet** (plain TCP only):
```bash
telnet localhost 12345
```

**Netcat** (plain TCP only):
```bash
nc localhost 12345
```

---

## Documentation

- **[DEPLOYMENT.md](DEPLOYMENT.md)** - Installation, configuration, and production deployment
- **[SECURITY.md](SECURITY.md)** - TLS configuration, certificate management, and security best practices
- **[CODE_REVIEW.md](CODE_REVIEW.md)** - Technical code review and architecture analysis
- **[CLAUDE.md](CLAUDE.md)** - Project instructions for Claude Code (AI assistant)

---

## Development

### Project Structure

```
xoe/
├── src/
│   ├── common/
│   │   └── h/commonDefinitions.h    # Error codes, constants
│   ├── core/
│   │   ├── server/
│   │   │   ├── net/                 # Network server
│   │   │   │   ├── c/xoe.c          # Main server implementation
│   │   │   │   └── h/xoe.h          # Server configuration
│   │   │   ├── security/            # TLS implementation
│   │   │   │   ├── h/               # TLS headers
│   │   │   │   └── c/               # TLS implementation
│   │   │   └── tests/               # Unit tests (in development)
│   │   └── packet_manager/          # Protocol handlers (planned)
├── scripts/
│   └── generate_test_certs.sh       # Certificate generation
├── Makefile                         # Build system
└── README.md                        # This file
```

### Build System

**Standard targets**:
```bash
make          # Build project
make clean    # Remove build artifacts
make all      # Same as make
```

**Compiler flags**:
- `-Wall -Wextra` - All warnings enabled
- `-std=c89 -pedantic` - Strict C89 compliance
- `-g` - Debug symbols

### Testing

**Quick Start**:
```bash
# Run all tests (recommended)
make test

# Comprehensive validation (build + test)
make check
```

**Available Test Targets**:
```bash
make test-build        # Compile tests without running
make test-unit         # Run unit tests only (68 tests)
make test-integration  # Run integration tests only (8 tests)
make test-verbose      # Run all tests with detailed output
```

**Current Test Coverage**:
- **Unit Tests**: 68 tests across 6 modules
  - TLS Security: 22 tests (context, error, I/O, session)
  - Serial Buffer: 21 tests (init, read/write, wrap-around, state)
  - Serial Protocol: 25 tests (encapsulation, decapsulation, checksums)
- **Integration Tests**: 8 end-to-end scenarios (TCP/TLS modes, concurrent connections)

**Test Requirements**:
- TLS tests require certificates: `./scripts/generate_test_certs.sh`
- Integration tests require `bin/xoe` binary: `make`

**Detailed Documentation**: See [docs/TESTING.md](docs/TESTING.md) for:
- Test framework reference (all assertion macros)
- Writing unit tests (templates, best practices)
- Debugging test failures
- Contributing tests

**Memory Leak Testing**:
```bash
# Linux
valgrind --leak-check=full ./bin/xoe -e none -p 12345

# macOS
leaks --atExit -- ./bin/xoe -e none -p 12345
```

### Code Style

- **Language**: ANSI C (C89) with `-std=c89 -pedantic`
- **Naming**:
  - Functions: `snake_case`
  - Types: `snake_case_t`
  - Macros: `UPPER_CASE`
  - Constants: `UPPER_CASE`
- **Documentation**: Every function must have header comment
- **Error Handling**: Check all return values
- **Thread Safety**: Document thread-safety for all functions

### Architecture

**Current Implementation**:
- Thread-per-client model (max 32 clients)
- Blocking I/O
- Fixed-size client pool (no dynamic allocation)
- Global SSL_CTX (read-only, thread-safe)
- Per-client SSL objects (thread-isolated)

**Future Plans**:
- Protocol handler abstraction
- Non-blocking I/O with event loop
- Dynamic client pool
- Plugin architecture for protocols

---

## Contributing

### Human-LLM Hybrid Development

This project is a testbed for human-LLM collaboration. Contributions are accepted from:

1. **Classic Coders** - Traditional hand-written code
2. **LLM-Assisted Coders** - Human developers using LLMs as tools
3. **Vibe Coders** - Developers primarily directing LLMs

**IMPORTANT**: All contributions must be clearly labeled with the development method used.

### Contribution Categories

**Label your contributions** in commit messages:

```
[CLASSIC] Implemented thread pool manager
[LLM-ASSISTED] Refactored TLS error handling with Claude's suggestions
[VIBE] Added certificate rotation feature via Claude Code
```

**Labeling Requirements**:
- First-time contributors must declare their category
- Each commit should indicate the development method
- Continuous improper labeling will lead to revocation of merge permissions

### Contribution Process

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Label** your commits with development method
4. **Test** thoroughly (unit tests + integration tests)
5. **Document** all new functions and APIs
6. **Submit** a pull request with clear description

### Code Quality Requirements

- ✅ C89 compliance (`-std=c89 -pedantic`)
- ✅ No compiler warnings
- ✅ All functions documented
- ✅ Error handling for all edge cases
- ✅ Memory leak free (verified with valgrind)
- ✅ Unit tests for new functionality
- ✅ Integration tests for user-facing features

### What We're Looking For

**High Priority**:
- Protocol handler implementations
- Non-blocking I/O support
- Client TLS support
- Certificate revocation checking (OCSP/CRL)
- Comprehensive unit test coverage

**Medium Priority**:
- Performance optimizations
- Additional platforms (Solaris, AIX, etc.)
- Documentation improvements
- Example client applications

**Low Priority**:
- GUI tools
- Configuration file support
- Logging framework
- Monitoring/metrics

---

## Project Goals

### 1. Extensible Protocol Framework

**Primary Goal**: Create a generic framework for encapsulating various protocols over Ethernet.

**Current State**: Basic echo server with TLS support
**Future Vision**: Plugin architecture for protocol handlers (HTTP, FTP, serial, custom protocols)

### 2. Human-LLM Hybrid Development Testbed

**Research Goal**: Study collaboration patterns between humans and LLMs in software development.

**Questions We're Exploring**:
- How do different development methods (classic, LLM-assisted, vibe) affect code quality?
- What are the strengths and weaknesses of each approach?
- How can humans and LLMs collaborate most effectively?

**Data Collection**:
- Commit labels track development method
- Code review compares quality across methods
- Issue resolution time varies by development method

### 3. Production-Ready Security

**Goal**: Demonstrate that TLS can be implemented correctly in C89.

**Achievements**:
- Modern TLS 1.3 with strong cipher suites
- Thread-safe design
- Clean abstraction layer
- Comprehensive security documentation

---

## Security

**Reporting Vulnerabilities**: Do NOT open public issues for security bugs.

Contact: (security contact email here)

See [SECURITY.md](SECURITY.md) for detailed security guidelines.

---

## License

(To be determined - please add license information)

---

## Acknowledgments

- OpenSSL project for TLS implementation
- Contributors (to be listed)
- Claude Code (Anthropic) for LLM-assisted development

---

## Contact

- **Issues**: GitHub Issues (link TBD)
- **Discussions**: GitHub Discussions (link TBD)
- **Email**: (contact email TBD)

---

## Changelog

### v1.0 (2025-12-02)
- Initial release with TLS 1.2/1.3 support
- Runtime encryption selection
- Multi-threaded echo server
- Cross-platform POSIX support
- Comprehensive documentation

---

**Built with ❤️ using Human-LLM collaboration**
