# XOE Directory Reorganization Plan

## Executive Summary

This plan restructures the XOE codebase into a clean, modular, and extensible architecture that:
- Separates concerns clearly (core, connectors, common)
- Eliminates deeply nested paths and complex relative includes
- Follows industry-standard conventions for C projects
- Makes it easy to add new connectors and protocols
- Improves build system maintainability

## Current Structure Issues

### Problems Identified

1. **Inconsistent Naming**: Mix of `server`, `net`, and `core` creates confusion about boundaries
2. **Deep Nesting**: Paths like `src/core/server/net/fsm/c/` require `../../../../../` includes
3. **Misplaced Components**:
   - `packet_manager` is under `core/` but isn't server-specific
   - `security` is under `server/` but could be used by clients
   - FSM states are under `net/` but handle serial and other modes
4. **Split c/h Directories**: Creates artificial separation; modern C projects often colocate
5. **Ambiguous Naming**: "server" directory contains both server and client code
6. **Poor Extensibility**: Adding new connectors (CAN bus, USB, etc.) isn't obvious

### Current Directory Tree

```
src/
├── common/h/                    # Common definitions (types, constants)
├── core/
│   ├── packet_manager/h/        # Protocol definitions (not server-specific!)
│   └── server/                  # Contains BOTH server and client code
│       ├── net/
│       │   ├── c/xoe.c          # Main application + client pool
│       │   ├── h/xoe.h          # Config structures
│       │   └── fsm/c/           # FSM states (8 files)
│       ├── security/            # TLS support (could be used by clients)
│       │   ├── c/               # 4 implementation files
│       │   └── h/               # 6 header files
│       └── tests/               # Unit tests
└── connector/
    └── serial/                  # Serial connector (well-organized!)
        ├── c/                   # 4 implementation files
        └── h/                   # 5 header files
```

**Include Path Complexity Examples**:
- `state_init.c` includes: `"../../../../../connector/serial/h/serial_config.h"`
- `state_client_serial.c` includes: `"../../../../../common/h/commonDefinitions.h"`
- Build system needs 6 different `-I` paths

## Proposed Structure

### Design Principles

1. **Flat is better than nested**: Max 3 levels deep
2. **Colocate related files**: Headers and implementations together
3. **Clear module boundaries**: Each top-level directory is a cohesive unit
4. **Extensibility first**: Easy to add new connectors/protocols
5. **Standard conventions**: Follow common C project layouts

### New Directory Tree

```
src/
├── lib/                         # Core library code (reusable components)
│   ├── protocol/                # Protocol definitions and handlers
│   │   ├── protocol.h           # xoe_packet_t, xoe_payload_t definitions
│   │   └── protocol.c           # Common protocol utilities (future)
│   ├── security/                # TLS/SSL support (usable by all components)
│   │   ├── tls_config.h
│   │   ├── tls_context.c/h
│   │   ├── tls_error.c/h
│   │   ├── tls_io.c/h
│   │   ├── tls_session.c/h
│   │   └── tls_types.h
│   └── common/                  # Common definitions and utilities
│       ├── types.h              # Fixed-width types
│       └── definitions.h        # TRUE/FALSE, error codes, etc.
│
├── core/                        # Core XOE application
│   ├── main.c                   # Entry point (FSM loop only)
│   ├── config.h                 # xoe_config_t, xoe_state_t, xoe_mode_t
│   ├── server.c/h               # Server implementation + client pool
│   └── fsm/                     # Finite state machine handlers
│       ├── state_init.c/h
│       ├── state_parse_args.c/h
│       ├── state_validate_config.c/h
│       ├── state_mode_select.c/h
│       ├── state_server_mode.c/h
│       ├── state_client_std.c/h
│       ├── state_client_serial.c/h
│       └── state_cleanup.c/h
│
├── connectors/                  # Protocol connectors (extensible)
│   ├── serial/                  # Serial TTY connector
│   │   ├── serial_buffer.c/h
│   │   ├── serial_client.c/h
│   │   ├── serial_config.h
│   │   ├── serial_port.c/h
│   │   └── serial_protocol.c/h
│   ├── can/                     # CAN bus connector (future)
│   ├── usb/                     # USB connector (future)
│   └── modbus/                  # Modbus connector (future)
│
└── tests/                       # All test code
    ├── framework/               # Test framework
    │   ├── test_framework.c/h
    │   └── test_helpers.c/h
    └── unit/                    # Unit tests
        ├── test_tls_context.c
        ├── test_tls_error.c
        ├── test_tls_io.c
        └── test_tls_session.c
```

### Key Improvements

1. **Simplified Includes**: All includes are now 2 levels max
   - Before: `"../../../../../common/h/commonDefinitions.h"`
   - After: `"lib/common/definitions.h"`

2. **Clear Boundaries**:
   - `src/lib/` = Reusable library components
   - `src/core/` = Main application logic
   - `src/connectors/` = Pluggable protocol connectors
   - `src/tests/` = All test-related code

3. **Extensibility**: Adding a new connector is now obvious:
   ```bash
   mkdir src/connectors/can
   # Add can_*.c/h files
   # Update Makefile CONNECTORS list
   ```

4. **No c/h Split**: Headers and implementations colocated (industry standard)

5. **Logical Grouping**:
   - Security code moved from `server/security` to `lib/security` (reusable)
   - Protocol definitions moved from `core/packet_manager` to `lib/protocol`
   - FSM states remain together under `core/fsm/`
   - Server-specific code consolidated in `core/server.c`

## Detailed File Mapping

### Phase 1: Restructure Library Components

**src/lib/common/** (from src/common/h/)
- `types.h` ← `src/common/h/types.h` (no change)
- `definitions.h` ← `src/common/h/commonDefinitions.h` (rename)

**src/lib/protocol/** (from src/core/packet_manager/h/)
- `protocol.h` ← `src/core/packet_manager/h/protocol.h`

**src/lib/security/** (from src/core/server/security/)
- `tls_config.h` ← `src/core/server/security/h/tls_config.h`
- `tls_context.c` ← `src/core/server/security/c/tls_context.c`
- `tls_context.h` ← `src/core/server/security/h/tls_context.h`
- `tls_error.c` ← `src/core/server/security/c/tls_error.c`
- `tls_error.h` ← `src/core/server/security/h/tls_error.h`
- `tls_io.c` ← `src/core/server/security/c/tls_io.c`
- `tls_io.h` ← `src/core/server/security/h/tls_io.h`
- `tls_session.c` ← `src/core/server/security/c/tls_session.c`
- `tls_session.h` ← `src/core/server/security/h/tls_session.h`
- `tls_types.h` ← `src/core/server/security/h/tls_types.h`

### Phase 2: Restructure Core Application

**src/core/** (from src/core/server/net/)
- `main.c` ← Extract `main()` from `src/core/server/net/c/xoe.c`
- `config.h` ← Extract types from `src/core/server/net/h/xoe.h`
- `server.c` ← Extract server functions from `src/core/server/net/c/xoe.c`
- `server.h` ← Extract server declarations from `src/core/server/net/h/xoe.h`

**src/core/fsm/** (from src/core/server/net/fsm/c/)
- `state_init.c` ← `src/core/server/net/fsm/c/state_init.c`
- `state_init.h` ← NEW (extract prototypes)
- `state_parse_args.c` ← `src/core/server/net/fsm/c/state_parse_args.c`
- `state_parse_args.h` ← NEW (extract prototypes)
- `state_validate_config.c` ← `src/core/server/net/fsm/c/state_validate_config.c`
- `state_validate_config.h` ← NEW (extract prototypes)
- `state_mode_select.c` ← `src/core/server/net/fsm/c/state_mode_select.c`
- `state_mode_select.h` ← NEW (extract prototypes)
- `state_server_mode.c` ← `src/core/server/net/fsm/c/state_server_mode.c`
- `state_server_mode.h` ← NEW (extract prototypes)
- `state_client_std.c` ← `src/core/server/net/fsm/c/state_client_std.c`
- `state_client_std.h` ← NEW (extract prototypes)
- `state_client_serial.c` ← `src/core/server/net/fsm/c/state_client_serial.c`
- `state_client_serial.h` ← NEW (extract prototypes)
- `state_cleanup.c` ← `src/core/server/net/fsm/c/state_cleanup.c`
- `state_cleanup.h` ← NEW (extract prototypes)

### Phase 3: Restructure Connectors

**src/connectors/serial/** (from src/connector/serial/)
- `serial_buffer.c` ← `src/connector/serial/c/serial_buffer.c`
- `serial_buffer.h` ← `src/connector/serial/h/serial_buffer.h`
- `serial_client.c` ← `src/connector/serial/c/serial_client.c`
- `serial_client.h` ← `src/connector/serial/h/serial_client.h`
- `serial_config.h` ← `src/connector/serial/h/serial_config.h`
- `serial_port.c` ← `src/connector/serial/c/serial_port.c`
- `serial_port.h` ← `src/connector/serial/h/serial_port.h`
- `serial_protocol.c` ← `src/connector/serial/c/serial_protocol.c`
- `serial_protocol.h` ← `src/connector/serial/h/serial_protocol.h`

### Phase 4: Restructure Tests

**src/tests/framework/** (from src/core/server/tests/)
- `test_framework.c` ← `src/core/server/tests/test_framework.c`
- `test_framework.h` ← `src/core/server/tests/test_framework.h`

**src/tests/unit/** (from src/core/server/tests/)
- `test_tls_context.c` ← `src/core/server/tests/test_tls_context.c`
- `test_tls_error.c` ← `src/core/server/tests/test_tls_error.c`
- `test_tls_io.c` ← `src/core/server/tests/test_tls_io.c`
- `test_tls_session.c` ← `src/core/server/tests/test_tls_session.c`

## Implementation Plan

### Prerequisites
- Create new branch: `refactor/directory-reorganization`
- Ensure all tests pass on current main branch
- Full backup of repository

### Phase 1: Create New Directory Structure (Empty)
```bash
mkdir -p src/lib/{common,protocol,security}
mkdir -p src/core/fsm
mkdir -p src/connectors/serial
mkdir -p src/tests/{framework,unit}
```

### Phase 2: Move Library Components
1. Move `src/common/h/` → `src/lib/common/`
2. Rename `commonDefinitions.h` → `definitions.h`
3. Move `src/core/packet_manager/h/protocol.h` → `src/lib/protocol/protocol.h`
4. Move all `src/core/server/security/` → `src/lib/security/`
5. Update all `#include` statements in moved files

### Phase 3: Split and Move Core Application
1. Extract `main()` from `xoe.c` → `src/core/main.c`
2. Extract config types from `xoe.h` → `src/core/config.h`
3. Move remaining `xoe.c` → `src/core/server.c`
4. Move remaining `xoe.h` → `src/core/server.h`
5. Move all FSM files from `src/core/server/net/fsm/c/` → `src/core/fsm/`
6. Create corresponding `.h` files for each FSM state
7. Update all `#include` statements

### Phase 4: Move Connectors
1. Move `src/connector/serial/` → `src/connectors/serial/`
2. Flatten c/h split (move all files to `src/connectors/serial/`)
3. Update all `#include` statements

### Phase 5: Move Tests
1. Move `src/core/server/tests/test_framework.*` → `src/tests/framework/`
2. Move `src/core/server/tests/test_*.c` → `src/tests/unit/`
3. Update all `#include` statements

### Phase 6: Update Makefile
1. Rewrite directory variables
2. Simplify include paths (should only need 4-5 `-I` flags)
3. Update source file wildcards
4. Update object file patterns
5. Test build

### Phase 7: Update Documentation
1. Update CLAUDE.md with new structure
2. Update README.md build instructions
3. Update CODE_REVIEW.md references
4. Add architecture diagram to docs

### Phase 8: Cleanup
1. Remove old empty directories
2. Verify no broken includes
3. Run full test suite
4. Build clean with zero warnings

## Include Path Strategy

### Before (Complex)
```c
// From state_init.c
#include "../../h/xoe.h"
#include "../../../../../common/h/commonDefinitions.h"
#include "../../../../../connector/serial/h/serial_config.h"
#include "../../security/h/tls_context.h"
```

### After (Simple)
```c
// From core/fsm/state_init.c
#include "core/config.h"
#include "lib/common/definitions.h"
#include "connectors/serial/serial_config.h"
#include "lib/security/tls_context.h"
```

### Makefile Include Paths
```makefile
# Before: 6 include paths
INCLUDES = -I$(NETDIR)/h -I$(FSMDIR)/h -I$(SECDIR)/h -I$(PACKETDIR)/h -I$(COMMONDIR)/h -I$(SERIALDIR)/h

# After: 4 include paths (from src/ root)
INCLUDES = -Isrc -Isrc/lib -Isrc/core -Isrc/connectors
```

## Makefile Reorganization

### Before (Complex)
```makefile
NETDIR   = $(COREDIR)/server/net
FSMDIR   = $(NETDIR)/fsm
SECDIR   = $(COREDIR)/server/security
TESTDIR  = $(COREDIR)/server/tests
PACKETDIR= $(COREDIR)/packet_manager
SERIALDIR= $(SRCDIR)/connector/serial

SOURCES = $(wildcard $(NETDIR)/c/*.c)
SOURCES += $(wildcard $(FSMDIR)/c/*.c)
SOURCES += $(wildcard $(SECDIR)/c/*.c)
SOURCES += $(wildcard $(SERIALDIR)/c/*.c)

# 4 different pattern rules for different source directories
```

### After (Simple)
```makefile
LIBDIR      = $(SRCDIR)/lib
COREDIR     = $(SRCDIR)/core
CONNECTORDIR= $(SRCDIR)/connectors
TESTDIR     = $(SRCDIR)/tests

# Automatic discovery of all source files
SOURCES = $(wildcard $(COREDIR)/*.c)
SOURCES += $(wildcard $(COREDIR)/fsm/*.c)
SOURCES += $(wildcard $(LIBDIR)/*/*.c)
SOURCES += $(wildcard $(CONNECTORDIR)/*/*.c)

# Single pattern rule (or auto-dependency generation)
$(OBJDIR)/%.o: $(SRCDIR)/%.c
    $(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
```

## Benefits Summary

### For Developers
1. **Easier Navigation**: Logical structure, clear boundaries
2. **Simpler Includes**: No more `../../../../../` paths
3. **Better IDE Support**: Flat structure works better with autocomplete
4. **Clearer Dependencies**: Import paths show module relationships

### For Extensibility
1. **Add Connectors**: Just create `src/connectors/newtype/`
2. **Add Protocols**: Add to `src/lib/protocol/`
3. **Add Tests**: Drop into `src/tests/unit/`
4. **No Makefile Surgery**: Wildcard patterns auto-discover

### For Maintenance
1. **Fewer Build Rules**: Single pattern rule vs. 4 separate rules
2. **Shorter Makefiles**: ~80 lines vs. 138 lines
3. **Clear Module Boundaries**: Each directory is independent
4. **Standard Layout**: Matches industry conventions

## Risk Mitigation

### Potential Issues
1. **Breaking Include Paths**: Every file must be updated
2. **Build System Complexity**: Makefile needs careful rewrite
3. **Merge Conflicts**: Large structural changes
4. **Testing Gap**: Must verify all modes still work

### Mitigation Strategies
1. **Scripted Migration**: Use shell scripts for bulk file moves
2. **Automated Include Updates**: Use `sed` to update include paths
3. **Incremental Testing**: Test build after each phase
4. **Comprehensive Test Suite**: Run full integration tests
5. **Git Branch**: All work on `refactor/directory-reorganization`
6. **Atomic Commits**: Each phase is a separate commit

## Testing Strategy

After each phase:
1. **Build Test**: `make clean && make` must succeed
2. **Unit Tests**: `make test` must pass
3. **Integration Tests**: Run all 5 operational modes
4. **Warning Check**: Zero compiler warnings (except OpenSSL)

Final verification:
1. Server mode: `./bin/xoe -p 12345`
2. Standard client: `./bin/xoe -c localhost:12345`
3. Serial client: `./bin/xoe -c localhost:12345 -s /dev/ttyUSB0`
4. TLS mode: `./bin/xoe -e tls13 -p 12345`
5. Help output: `./bin/xoe -h`

## Timeline Estimate

- **Phase 1-2** (Library restructure): ~2 hours
- **Phase 3** (Core split): ~3 hours (most complex)
- **Phase 4** (Connectors): ~1 hour
- **Phase 5** (Tests): ~1 hour
- **Phase 6** (Makefile): ~2 hours
- **Phase 7** (Documentation): ~1 hour
- **Phase 8** (Cleanup/Testing): ~2 hours

**Total**: ~12 hours of focused work

## Success Criteria

1. ✅ Build succeeds with zero code warnings
2. ✅ All unit tests pass
3. ✅ All integration tests pass
4. ✅ All 5 operational modes work correctly
5. ✅ Include paths are simplified (no `../../../..`)
6. ✅ Makefile reduced to ~80 lines
7. ✅ Clear separation: lib/, core/, connectors/, tests/
8. ✅ Documentation updated
9. ✅ No files left in old directory structure

## Future Extensibility Examples

### Adding a CAN Bus Connector
```bash
# 1. Create directory
mkdir src/connectors/can

# 2. Add implementation files
touch src/connectors/can/{can_config.h,can_port.c,can_port.h,can_protocol.c,can_protocol.h}

# 3. Implement CAN-specific logic
# (can_port.c handles CAN I/O, can_protocol.c handles XOE packet encapsulation)

# 4. Build system auto-discovers (no Makefile changes needed)
make

# 5. Add new state handler if needed
touch src/core/fsm/state_client_can.{c,h}
```

### Adding a New Protocol Type
```bash
# 1. Define protocol in lib/protocol/protocol.h
#define XOE_PROTOCOL_MODBUS 0x0002

# 2. Create connector
mkdir src/connectors/modbus
# Add modbus_*.{c,h} files

# 3. Build automatically includes new code
make
```

## Conclusion

This reorganization transforms XOE from a deeply nested, server-centric structure into a clean, modular architecture that clearly separates:
- **Reusable libraries** (`lib/`)
- **Core application logic** (`core/`)
- **Extensible connectors** (`connectors/`)
- **Comprehensive tests** (`tests/`)

The result is a codebase that is easier to understand, maintain, and extend while following modern C project conventions.
