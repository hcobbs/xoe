# XOE Serial Connector Design Plan

## Project Context

**Objective**: Design and implement a serial connector for the xoe (X over Ethernet) project that enables bidirectional TTY serial communication over Ethernet.

**Use Case**: Bridge two remote serial devices through Ethernet-connected xoe instances, allowing legacy serial protocols to communicate over modern networks.

## Current Architecture Understanding

### Existing XOE Framework
- **Language**: ANSI-C (C89) with strict compliance (`-std=c89 -pedantic`)
- **Platform**: POSIX-only (Linux, macOS)
- **Threading**: POSIX pthreads, per-client detached threads
- **Current Implementation**: Multi-threaded TCP echo server with optional TLS
- **Max Clients**: 32 concurrent connections
- **Buffer Size**: 1024 bytes (configurable)

### Protocol Handler Architecture (Defined but Not Implemented)
Located in `src/core/packet_manager/h/protocol.h`:

- **xoe_packet_t**: Logical packet structure
  - `uint16_t protocol_id` - Protocol identifier
  - `uint16_t protocol_version` - Version number
  - `xoe_payload_t* payload` - Variable-length payload
  - `uint32_t checksum` - Data integrity check

- **protocol_handler_t**: Pluggable protocol interface
  - `init_session()` - Session initialization callback
  - `handle_data()` - Data processing callback
  - `cleanup_session()` - Session cleanup callback

### Connector Directory Structure
Currently exists as empty stubs:
```
src/connector/serial/
├── c/
│   └── stub.c (empty)
└── h/
    └── stub.h (empty)
```

### Build System
- GNU Make with wildcard source discovery
- Compiler: `gcc -Wall -Wextra -g -std=c89 -pedantic`
- Output: Objects to `obj/`, binaries to `bin/`
- Currently only builds from `src/core/server/net/c/`
- Serial connector NOT yet integrated into build

## Design Decisions (User-Approved)

### 1. Serial Port Configuration ✓
**Decision**: Runtime configuration via command-line arguments
- Configurable parameters: baud rate, parity, data bits, stop bits, flow control
- No compile-time hardcoding
- Support standard baud rates (9600, 19200, 38400, 57600, 115200, etc.)

### 2. Operating Mode ✓
**Decision**: Client Mode Extension
- Extend existing xoe client mode (`-c` flag)
- Client bridges local serial port ↔ remote xoe server
- Serial support activated via new command-line flag (e.g., `-s /dev/ttyUSB0`)

### 3. Data Flow Topology ✓
**Decision**: Bidirectional Serial-to-Serial Bridge
```
[Local Serial Port] ↔ [Local XOE Client] ↔ [Ethernet] ↔ [Remote XOE Server] ↔ [Remote XOE Client] ↔ [Remote Serial Port]
```
- Both sides run xoe in client mode with serial connector
- Data flows bidirectionally through the network
- Each xoe instance bridges its local serial port to the network

### 4. Protocol Handler Integration ✓
**Decision**: Use existing `protocol_handler_t` interface
- Define serial protocol ID in `protocol.h`
- Implement protocol_handler_t callbacks for serial
- Leverage existing packet abstraction (xoe_packet_t, xoe_payload_t)

### 5. Buffer and Flow Control ✓
**Decision**: Request implementation suggestions
- Need buffering strategy for serial ↔ network speed mismatch
- Flow control mechanism to be designed based on best practices
- Consider both software (XON/XOFF) and hardware (RTS/CTS) options

### 6. Error Handling Strategy ✓
**Decision**: Graceful shutdown on errors (for now)
- Serial port errors → log error and gracefully terminate
- No automatic reconnection in initial implementation
- Clean resource cleanup on error conditions
- Future enhancement: error recovery and reconnection

### 7. TTY Device Specification ✓
**Decision**: Command-line parameter
- Format: `-s /dev/ttyUSB0` or `--serial /dev/ttyUSB0`
- Additional flags for serial configuration:
  - `-b` or `--baud` for baud rate
  - `--parity` for parity setting
  - `--databits` for data bits (7 or 8)
  - `--stopbits` for stop bits (1 or 2)
  - `--flow` for flow control (none, xonxoff, rtscts)
- Single serial port per xoe instance (for now)

## Implementation Architecture

### Module Design

The serial connector consists of four main components:

1. **Serial I/O Layer** - Low-level termios abstraction for serial port operations
2. **Protocol Handler** - XOE protocol integration for serial data encapsulation
3. **Client Integration** - Threading and flow control for bidirectional bridging
4. **Circular Buffer** - Flow control for network→serial speed mismatch

### Threading Model: Three-Thread Design

**Main Thread**: Initialization, argument parsing, thread spawning, shutdown coordination

**Serial→Network Thread**:
- Reads from serial port (blocking with timeout)
- Encapsulates into xoe_packet_t
- Sends to network socket
- No buffering needed (serial slower than network)

**Network→Serial Thread**:
- Receives xoe_packet_t from network
- Decapsulates to serial data
- Writes to serial port via circular buffer (16KB)
- Buffer handles speed mismatch

### Protocol Integration

**Protocol ID**: `XOE_PROTOCOL_SERIAL = 0x0001`

**Packet Structure**:
```
xoe_packet_t:
  protocol_id: 0x0001 (SERIAL)
  protocol_version: 0x0001
  payload:
    serial_header_t (4 bytes): flags + sequence number
    data: actual serial data (1-1020 bytes)
  checksum: simple sum (CRC32 future enhancement)
```

**Flags**: Parity error, framing error, overrun error, XON/XOFF

### Flow Control Strategy

- **Serial→Network**: No buffering (serial is slow, network is fast)
- **Network→Serial**: 16KB circular buffer provides ~16 seconds at 9600 baud
- **Hardware Flow Control**: Support RTS/CTS and XON/XOFF via termios
- **Backpressure**: If buffer fills, block network read (TCP handles backpressure)

## Implementation Plan

### Phase 1: Serial I/O Foundation
**Goal**: Basic serial port access working

1. Create `serial_config.h` with configuration structures
2. Implement `serial_port.c`: open, close, read, write with timeout
3. Create unit tests for serial port functions
4. Test with virtual serial ports (socat)

**Files**:
- [serial_config.h](src/connector/serial/h/serial_config.h)
- [serial_port.h](src/connector/serial/h/serial_port.h)
- [serial_port.c](src/connector/serial/c/serial_port.c)

### Phase 2: Protocol Layer
**Goal**: Serial data encapsulation working

1. Create `serial_protocol.h` with packet definitions
2. Implement encapsulation/decapsulation functions
3. Implement checksum calculation (simple sum)
4. Create unit tests for protocol functions

**Files**:
- [serial_protocol.h](src/connector/serial/h/serial_protocol.h)
- [serial_protocol.c](src/connector/serial/c/serial_protocol.c)

### Phase 3: Basic Client Integration
**Goal**: Single-threaded serial client working

1. Modify `xoe.c` to parse serial command-line flags
2. Implement basic serial client mode (single-threaded)
3. Test with virtual serial ports and xoe server
4. Verify bidirectional data flow

**Files Modified**:
- [xoe.c](src/core/server/net/c/xoe.c) - Add serial CLI parsing and basic client

### Phase 4: Threading and Flow Control
**Goal**: Multi-threaded client with proper flow control

1. Create circular buffer implementation
2. Implement `serial_client.c` with threading
3. Add shutdown coordination
4. Integration testing with stress tests

**Files**:
- [serial_buffer.h](src/connector/serial/h/serial_buffer.h)
- [serial_buffer.c](src/connector/serial/c/serial_buffer.c)
- [serial_client.h](src/connector/serial/h/serial_client.h)
- [serial_client.c](src/connector/serial/c/serial_client.c)

### Phase 5: Error Handling and Polish
**Goal**: Production-ready error handling

1. Add comprehensive error detection
2. Implement graceful error recovery
3. Add detailed logging
4. Update documentation
5. Complete test coverage

**Files Modified**:
- All serial connector files - Add error handling
- [CLAUDE.md](CLAUDE.md) - Update with serial connector docs

### Phase 6: Hardware Testing
**Goal**: Verify with real hardware

1. Test with USB-to-serial adapters
2. Test various baud rates and configurations
3. Stress test with large data transfers
4. Long-duration stability testing

## Critical Files

### Files to Create

1. **[serial_config.h](src/connector/serial/h/serial_config.h)** - Configuration structures, constants, defaults (~150 lines)

2. **[serial_port.h/c](src/connector/serial/h/serial_port.h)** - Core serial I/O abstraction (~500 lines total)
   - `serial_port_open()` - Open and configure serial port
   - `serial_port_close()` - Close serial port
   - `serial_port_read()` - Read with timeout
   - `serial_port_write()` - Write data
   - `serial_port_get_status()` - Get error flags

3. **[serial_protocol.h/c](src/connector/serial/h/serial_protocol.h)** - XOE protocol integration (~420 lines total)
   - `serial_protocol_encapsulate()` - Wrap serial data in xoe_packet_t
   - `serial_protocol_decapsulate()` - Extract serial data from packet
   - `serial_protocol_checksum()` - Calculate packet checksum

4. **[serial_buffer.h/c](src/connector/serial/h/serial_buffer.h)** - Circular buffer for flow control (~330 lines total)
   - Thread-safe read/write operations
   - Condition variables for blocking on full/empty

5. **[serial_client.h/c](src/connector/serial/h/serial_client.h)** - Threading and coordination (~600 lines total)
   - `serial_client_init()` - Initialize session
   - `serial_client_start()` - Spawn I/O threads
   - `serial_client_stop()` - Graceful shutdown
   - Thread entry points for serial→network and network→serial

### Files to Modify

6. **[xoe.c](src/core/server/net/c/xoe.c)** - Main integration point (~150 lines added)
   - Parse serial command-line flags: `-s`, `-b`, `--parity`, `--databits`, `--stopbits`, `--flow`
   - Conditional logic in client mode to invoke serial client
   - Update `print_usage()` with serial options

7. **[Makefile](Makefile)** - Build system integration
   - Add `SERIALDIR` variable
   - Include serial source files in `SOURCES`
   - Add pattern rule for serial object files
   - Add `-I$(SERIALDIR)/h` to include paths

## Command-Line Interface

### Usage Examples

**Start XOE server** (on remote machine):
```bash
./bin/xoe -p 12345
```

**Start XOE serial client** (on local machine with serial device):
```bash
./bin/xoe -c 192.168.1.100:12345 -s /dev/ttyUSB0 -b 9600
```

**With full configuration**:
```bash
./bin/xoe -c 192.168.1.100:12345 \
  -s /dev/ttyUSB0 \
  -b 115200 \
  --parity none \
  --databits 8 \
  --stopbits 1 \
  --flow rtscts
```

### New Command-Line Flags

- `-s <device>` - Serial device path (e.g., `/dev/ttyUSB0`)
- `-b <baud>` - Baud rate (default: 9600)
  - Common: 9600, 19200, 38400, 57600, 115200
- `--parity <mode>` - Parity (default: none)
  - Options: none, even, odd
- `--databits <bits>` - Data bits (default: 8)
  - Options: 7, 8
- `--stopbits <bits>` - Stop bits (default: 1)
  - Options: 1, 2
- `--flow <mode>` - Flow control (default: none)
  - Options: none, xonxoff, rtscts

## Testing Strategy

### Unit Tests

Create in `src/core/server/tests/`:
- **test_serial_port.c** - Serial port configuration and I/O
- **test_serial_protocol.c** - Packet encapsulation/decapsulation
- **test_serial_buffer.c** - Circular buffer operations

### Integration Tests

**Virtual Serial Ports with socat**:
```bash
# Create virtual serial port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Returns: /dev/ttys002 <-> /dev/ttys003

# Test bidirectional communication
./bin/xoe -c localhost:12345 -s /dev/ttys002 -b 9600 &
./bin/xoe -s /dev/ttys003 -b 9600
```

**Integration Script** (`scripts/test_serial_integration.sh`):
1. Start socat to create virtual serial pair
2. Start xoe server
3. Start xoe serial clients on both virtual ports
4. Send test data through one port
5. Verify received on other port
6. Test bidirectional flow
7. Cleanup

### End-to-End Tests

```
[Virtual Serial A] ↔ [XOE Client -s A] ↔ [Network] ↔ [XOE Server] ↔ [XOE Client -s B] ↔ [Virtual Serial B]
```

**Test Cases**:
- Bidirectional echo test
- Large data transfer (1MB+)
- Various baud rates (9600, 115200)
- Flow control stress test
- Error injection (disconnect, invalid data)
- Graceful shutdown

### Hardware Tests

- USB-to-serial adapter with loopback (TX→RX)
- Two adapters connected to each other
- Real serial devices (GPS, sensors, etc.)

## Build System Changes

### Makefile Additions

```makefile
# Add serial connector directory
SERIALDIR = $(SRCDIR)/connector/serial

# Add serial sources to build
SOURCES += $(wildcard $(SERIALDIR)/c/*.c)

# Generate object files for serial sources
OBJECTS += $(patsubst $(SERIALDIR)/c/%.c,$(OBJDIR)/%.o,$(wildcard $(SERIALDIR)/c/*.c))

# Add serial include path
INCLUDES = -I$(NETDIR)/h -I$(SECDIR)/h -I$(PACKETDIR)/h -I$(COMMONDIR)/h -I$(SERIALDIR)/h

# Pattern rule for serial connector objects
$(OBJDIR)/%.o: $(SERIALDIR)/c/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
```

**Dependencies**: No additional libraries needed (termios is part of libc, pthread already linked)

## Error Handling

### Serial Port Errors
- **Configuration errors** (invalid baud, device not found): Fail fast, return `E_INVALID_ARGUMENT`
- **Hardware errors** (framing, parity, overrun): Log warning, set flags in packet, continue
- **I/O errors** (read/write failures, disconnect): Set shutdown flag, cleanup, exit gracefully

### Network Errors
- **Connection errors**: Fail fast, return `E_NETWORK_ERROR`
- **Transfer errors**: Set shutdown flag, cleanup resources
- **Protocol errors**: Log error, discard packet, continue (don't crash)

### Resource Cleanup
Always cleanup in reverse order of allocation:
1. Set shutdown flag
2. Wait for threads (pthread_join)
3. Close connections (network socket, serial port)
4. Destroy synchronization objects (mutexes)

## Future Enhancements (Out of Scope)

- Auto-reconnection on connection failures
- Hot-plug detection for serial devices
- Multiple concurrent serial ports
- Dynamic baud rate/config negotiation
- Serial server mode (xoe server accepting serial connections)
- Built-in data logging/capture
- Higher-level protocol parsing (Modbus, etc.)

## Coding Standards Compliance

All implementation will follow xoe project standards:
- ✓ ANSI-C (C89) strict compliance (`-std=c89 -pedantic`)
- ✓ 4-space indentation (no tabs)
- ✓ 120-character maximum line length
- ✓ Complete function documentation (purpose, params, returns, errors)
- ✓ Full test coverage (unit + integration tests)
- ✓ Reusable helper functions
- ✓ Clean Code principles
- ✓ Contribution labels ([LLM-ASSISTED] for AI-generated code)

## Status

**Current Phase**: Plan complete, ready for implementation

**Resume Point**: Begin Phase 1 (Serial I/O Foundation) when ready to implement
