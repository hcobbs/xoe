# USB Connector Implementation Plan

**Project**: XOE USB Connector
**Feature Branch**: `feature/usb-connector`
**Estimated Complexity**: High (significantly more complex than serial connector)
**Implementation Model**: [LLM-ARCH]

---

## ⚠️ EXPERIMENTAL FEATURE WARNING ⚠️

**THIS FEATURE IS HIGHLY EXPERIMENTAL**

This USB connector implementation is in the experimental phase. Users and contributors should be aware:

- ❌ **Test coverage is NOT guaranteed** - Testing may be incomplete or missing for some functionality
- ⚠️ **Unexpected results may occur** - Testing or using this connector can cause unexpected behavior, system instability, or device issues
- 🔬 **Use at your own risk** - This feature is provided as-is for research and development purposes
- 💡 **Domain expertise welcome** - People with USB protocol knowledge are strongly encouraged to review, critique, and contribute improvements!

**Recommended Use Cases During Experimental Phase**:
- Development and testing environments only
- Research and educational purposes
- Proof-of-concept implementations

**NOT Recommended**:
- Production environments
- Critical systems
- Situations where data loss or device damage would be unacceptable

---

## Known Limitations

This USB connector implementation has the following known limitations:

**Unsupported Transfer Types**:
- ❌ **Isochronous transfers** - Audio and video devices will NOT work
  - Network latency makes real-time streaming impractical
  - Timing guarantees cannot be met over TCP/IP

**Device Class Focus**:
- ✅ **HID devices** (keyboards, mice, game controllers) - Primary focus
- ✅ **Generic bulk devices** - Supported
- ✅ **CDC-ACM devices** (USB-serial adapters) - Supported for testing
- ⚠️ **Mass storage** - Basic bulk transfers work, but filesystem access is out of scope
- ❌ **Audio/Video devices** - Not supported (require isochronous)
- ❌ **Webcams** - Not supported (require isochronous)

**Performance Considerations**:
- Network latency will add delay to all USB operations
- Not suitable for latency-sensitive applications
- Best used on LAN or low-latency networks

---

## Executive Summary

Implement a comprehensive USB connector for the XOE (X over Ethernet) framework that enables bridging USB devices over network connections. This feature will follow the architectural patterns established by the existing serial connector while accommodating the significantly greater complexity of the USB protocol.

**Key Challenges**:
- USB timing constraints (sub-microsecond ACK/NAK) require Virtual HCI approach
- Multiple transfer types (Control, Bulk, Interrupt) - isochronous excluded
- Multi-device support with correct packet routing
- HID device handling (interrupt endpoints, input reports)
- Complex device enumeration and hot-plug handling
- Platform-specific kernel driver detachment requirements
- C89 compatibility with libusb-1.0 (modern C library)

---

## Architecture Overview

### Protocol Definition

**Protocol ID**: `XOE_PROTOCOL_USB = 0x0002`
**Protocol Version**: `0x0001`

The USB connector will encapsulate USB Request Blocks (URBs) into XOE packets, similar to how USB/IP operates but simplified for the XOE architecture.

### Threading Model

Building on the serial connector's three-thread design, the USB connector will use:

```
Main Thread:
  - Initialize libusb context
  - Enumerate/open USB device
  - Claim interface, detach kernel drivers
  - Connect to XOE server
  - Install signal handlers
  - Coordinate shutdown

USB→Network Thread (for each active endpoint):
  - Async USB transfers (libusb events)
  - URB encapsulation
  - Send to network socket

Network→USB Thread:
  - Receive from network socket
  - URB decapsulation
  - Submit to libusb
```

### Directory Structure

```
src/connectors/usb/
├── usb_client.c           # Multi-threaded client coordinator
├── usb_client.h           # Client session structure and API
├── usb_config.h           # Configuration structures and constants
├── usb_device.c           # Single device operations (open, close, claim)
├── usb_device.h           # Device interface declarations
├── usb_device_manager.c   # Multi-device manager (registry, routing)
├── usb_device_manager.h   # Device manager interface
├── usb_protocol.c         # URB encapsulation/decapsulation
├── usb_protocol.h         # Protocol definitions (XOE_PROTOCOL_USB)
├── usb_transfer.c         # Transfer handling (Control, Bulk, Interrupt)
├── usb_transfer.h         # Transfer interface declarations
└── usb_hotplug.c          # Hotplug detection (future enhancement)
```

---

## Implementation Phases

### Phase 1: Foundation and Infrastructure

**Goal**: Establish core USB infrastructure with device enumeration and control transfers.

#### 1.1 Library Integration and C89 Compatibility

**Files to Create**:
- `src/lib/usb_compat.h` - C89 compatibility wrapper for libusb-1.0

**Implementation**:
```c
/* src/lib/usb_compat.h */
#ifndef USB_COMPAT_H
#define USB_COMPAT_H

/* C89 compatibility for libusb-1.0 */
#if defined(__STDC__) && !defined(__STDC_VERSION__)
#define inline
#endif

#include <libusb-1.0/libusb.h>

#endif /* USB_COMPAT_H */
```

**Makefile Updates**:
- Add `-lusb-1.0` to `LIBS`
- Add libusb version check to ensure >= 1.0.20
- Add `connectors/usb/*.c` to source discovery

**Acceptance Criteria**:
- [ ] Code compiles with `-std=c89 -pedantic` and `-Wall -Wextra`
- [ ] No warnings from libusb header inclusion
- [ ] libusb functions callable from C89 code

---

#### 1.2 Protocol Definitions

**Files to Modify**:
- `src/lib/protocol/protocol.h`

**Files to Create**:
- `src/connectors/usb/usb_protocol.h`
- `src/connectors/usb/usb_protocol.c`

**Protocol Structure** (`usb_protocol.h`):

```c
#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H

#include "lib/common/types.h"
#include "lib/protocol/protocol.h"

/* Protocol identifier */
#define XOE_PROTOCOL_USB         0x0002
#define XOE_PROTOCOL_USB_VERSION 0x0001

/* USB command types (inspired by USB/IP) */
#define USB_CMD_SUBMIT      0x0001  /* Submit URB */
#define USB_CMD_UNLINK      0x0002  /* Cancel URB */
#define USB_RET_SUBMIT      0x0003  /* URB completion */
#define USB_RET_UNLINK      0x0004  /* Unlink response */
#define USB_CMD_ENUM        0x0010  /* Device enumeration */
#define USB_RET_ENUM        0x0011  /* Enumeration response */

/* USB transfer types (subset of libusb - isochronous not supported) */
#define USB_TRANSFER_CONTROL        0
#define USB_TRANSFER_BULK           2
#define USB_TRANSFER_INTERRUPT      3

/* USB transfer flags */
#define USB_FLAG_NO_DEVICE      0x0001  /* Device disconnected */
#define USB_FLAG_TIMEOUT        0x0002  /* Transfer timeout */
#define USB_FLAG_STALL          0x0004  /* Endpoint stalled */
#define USB_FLAG_OVERFLOW       0x0008  /* Buffer overflow */
#define USB_FLAG_BABBLE         0x0010  /* Babble detected */
#define USB_FLAG_CRC_ERROR      0x0020  /* CRC/protocol error */

/* Maximum payload sizes */
#define USB_MAX_PAYLOAD_SIZE    4096    /* URB header + data */
#define USB_MAX_DATA_SIZE       4048    /* Max transfer data */

/* USB URB header structure */
typedef struct {
    uint16_t command;           /* Command type (USB_CMD_*) */
    uint16_t flags;             /* Protocol flags */
    uint32_t seqnum;            /* Sequence number */
    uint32_t device_id;         /* Device identifier (VID:PID) */
    uint8_t  endpoint;          /* Target endpoint */
    uint8_t  transfer_type;     /* Control/Bulk/Interrupt/ISO */
    uint16_t reserved;          /* Alignment padding */
    uint32_t transfer_length;   /* Expected data length */
    uint32_t actual_length;     /* Actual transferred (response) */
    int32_t  status;            /* Transfer status (libusb codes) */
    uint8_t  setup[8];          /* Control setup packet */
    /* Followed by transfer data */
} usb_urb_header_t;

/* Function prototypes */
int usb_protocol_encapsulate(
    const usb_urb_header_t* urb_header,
    const void* transfer_data,
    uint32_t data_len,
    xoe_packet_t* packet
);

int usb_protocol_decapsulate(
    const xoe_packet_t* packet,
    usb_urb_header_t* urb_header,
    void* transfer_data,
    uint32_t* data_len
);

uint32_t usb_protocol_checksum(
    const usb_urb_header_t* urb_header,
    const void* data,
    uint32_t data_len
);

#endif /* USB_PROTOCOL_H */
```

**Implementation** (`usb_protocol.c`):

Core functions:
1. `usb_protocol_encapsulate()` - Pack URB header + data into XOE packet
2. `usb_protocol_decapsulate()` - Unpack XOE packet into URB header + data
3. `usb_protocol_checksum()` - Calculate checksum over URB and data

**Acceptance Criteria**:
- [ ] Protocol constants defined in `protocol.h`
- [ ] URB header structure matches libusb transfer semantics
- [ ] Encapsulation/decapsulation handles all transfer types
- [ ] Checksum function implemented (simple sum initially)
- [ ] Unit tests for pack/unpack round-trip

---

#### 1.3 Configuration Structures

**Files to Create**:
- `src/connectors/usb/usb_config.h`

**Configuration Structure**:

```c
#ifndef USB_CONFIG_H
#define USB_CONFIG_H

#include "lib/common/types.h"

/* Maximum path/string lengths */
#define USB_DEVICE_DESC_MAX     256
#define USB_MAX_ENDPOINTS       16

/* USB device configuration */
typedef struct {
    /* Device identification */
    uint16_t vendor_id;         /* USB Vendor ID (0 = any) */
    uint16_t product_id;        /* USB Product ID (0 = any) */
    int      interface_number;  /* Interface to claim */
    int      configuration;     /* Configuration value (0 = active) */

    /* Endpoint configuration */
    uint8_t  bulk_in_endpoint;      /* Bulk IN endpoint (0x81, etc.) */
    uint8_t  bulk_out_endpoint;     /* Bulk OUT endpoint (0x01, etc.) */
    uint8_t  interrupt_in_endpoint; /* Interrupt IN (-1 if none) */

    /* Transfer parameters */
    int      transfer_timeout_ms;   /* Default timeout */
    int      max_packet_size;       /* Maximum packet size */

    /* Flags */
    int      detach_kernel_driver;  /* Auto-detach kernel driver */
    int      enable_hotplug;        /* Enable hotplug detection */
} usb_config_t;

/* Default values */
#define USB_DEFAULT_TIMEOUT_MS      5000
#define USB_DEFAULT_MAX_PACKET      512
#define USB_NO_ENDPOINT             0xFF

/* Validation functions */
int usb_config_validate(const usb_config_t* config);
void usb_config_init_defaults(usb_config_t* config);

#endif /* USB_CONFIG_H */
```

**Acceptance Criteria**:
- [ ] Configuration structure supports VID/PID device selection
- [ ] Endpoint addresses properly defined (IN vs OUT)
- [ ] Validation function checks for required fields
- [ ] Default initialization function provided

---

#### 1.4 Device Enumeration and Management

**Files to Create**:
- `src/connectors/usb/usb_device.h`
- `src/connectors/usb/usb_device.c`

**Key Functions**:

```c
/* usb_device.h */

/* Device context */
typedef struct {
    struct libusb_context* ctx;
    struct libusb_device_handle* handle;
    usb_config_t config;
    int interface_claimed;
    int kernel_driver_detached;
} usb_device_t;

/* Initialization */
int usb_device_init_library(struct libusb_context** ctx);
void usb_device_cleanup_library(struct libusb_context* ctx);

/* Device enumeration */
int usb_device_enumerate(struct libusb_context* ctx);
int usb_device_find(struct libusb_context* ctx,
                    uint16_t vid,
                    uint16_t pid,
                    struct libusb_device** device);

/* Device lifecycle */
int usb_device_open(usb_device_t* dev,
                    struct libusb_context* ctx,
                    const usb_config_t* config);
int usb_device_close(usb_device_t* dev);

/* Interface management */
int usb_device_claim_interface(usb_device_t* dev);
int usb_device_release_interface(usb_device_t* dev);

/* Information */
int usb_device_get_endpoints(usb_device_t* dev,
                              uint8_t* endpoints,
                              int max_endpoints);
```

**Implementation Requirements**:

1. **Library Initialization**:
   - `libusb_init()` with error handling
   - Optional debug level setting
   - Context cleanup on shutdown

2. **Device Enumeration**:
   - `libusb_get_device_list()` for discovery
   - Filter by VID/PID if specified
   - Print device info for debugging

3. **Device Opening**:
   - `libusb_open_device_with_vid_pid()`
   - Check if kernel driver active
   - `libusb_detach_kernel_driver()` if needed (Linux)
   - `libusb_claim_interface()`

4. **Graceful Cleanup**:
   - `libusb_release_interface()`
   - `libusb_attach_kernel_driver()` (restore original state)
   - `libusb_close()`

**Platform-Specific Considerations**:

- **Linux**: Requires root or udev rules, kernel driver detachment
- **macOS**: KEXT conflicts (especially CDC devices), may need codeless KEXT
- **BSD**: Similar to Linux, check permissions

**Acceptance Criteria**:
- [ ] Library initializes successfully on all platforms
- [ ] Device enumeration lists connected USB devices
- [ ] Can open device by VID/PID
- [ ] Interface claiming succeeds
- [ ] Kernel driver detachment works on Linux
- [ ] Cleanup restores system state
- [ ] Error codes properly mapped

---

#### 1.5 Core Configuration Integration

**Files to Modify**:
- `src/core/config.h`
- `src/core/fsm/state_init.c`
- `src/core/fsm/state_parse_args.c`

**Core Config Updates** (`config.h`):

```c
/* In xoe_mode_t enum */
MODE_CLIENT_USB,

/* In xoe_state_t enum */
STATE_CLIENT_USB,

/* In xoe_config_t struct */
int use_usb;                /* USB mode flag */
void* usb_config;           /* Opaque pointer to usb_config_t */
char usb_device_vid_pid[32];/* VID:PID from command line */
```

**Command-Line Arguments** (`state_parse_args.c`):

**New Options**:
- `-u <vid:pid>` - Specify USB device by vendor:product ID
- `--interface <num>` - Interface number to claim
- `--ep-in <addr>` - Bulk IN endpoint address (hex)
- `--ep-out <addr>` - Bulk OUT endpoint address (hex)

**Example Usage**:
```bash
# Connect to server with specific USB device
./bin/xoe -c 192.168.1.100:12345 -u 1234:5678 --interface 0

# With explicit endpoints
./bin/xoe -c 192.168.1.100:12345 -u 1234:5678 \
  --interface 0 --ep-in 0x81 --ep-out 0x01

# Multiple devices simultaneously
./bin/xoe -c 192.168.1.100:12345 -u 1234:5678 -u 046d:c077
```

**Parsing Logic**:

```c
/* Parse VID:PID format */
if (strcmp(arg, "-u") == 0) {
    unsigned int vid, pid;
    if (sscanf(argv[++i], "%x:%x", &vid, &pid) != 2) {
        fprintf(stderr, "Error: Invalid VID:PID format. Use: -u VVVV:PPPP\n");
        return STATE_CLEANUP;
    }
    /* Store in config */
    config->use_usb = TRUE;
    sprintf(config->usb_device_vid_pid, "%04x:%04x", vid, pid);
}
```

**Acceptance Criteria**:
- [ ] USB mode flag integrated into config
- [ ] VID:PID parsing handles hex format (e.g., `04d8:000a`)
- [ ] Interface and endpoint options parsed correctly
- [ ] Help text updated with USB options
- [ ] Validation rejects invalid combinations

---

### Phase 2: Transfer Implementation

**Goal**: Implement USB transfer handling for Control and Bulk transfers.

#### 2.1 Transfer Abstraction Layer

**Files to Create**:
- `src/connectors/usb/usb_transfer.h`
- `src/connectors/usb/usb_transfer.c`

**Transfer Context Structure**:

```c
typedef struct {
    struct libusb_transfer* transfer;  /* libusb transfer object */
    usb_urb_header_t urb_header;       /* URB header for protocol */
    void* user_data;                   /* Callback context */
    int completed;                     /* Completion flag */
    pthread_mutex_t lock;              /* Thread safety */
    pthread_cond_t cond;               /* Completion notification */
} usb_transfer_ctx_t;
```

**Key Functions**:

```c
/* Synchronous transfers (Phase 2.2) */
int usb_transfer_control(usb_device_t* dev,
                         uint8_t bmRequestType,
                         uint8_t bRequest,
                         uint16_t wValue,
                         uint16_t wIndex,
                         unsigned char* data,
                         uint16_t wLength,
                         unsigned int timeout);

int usb_transfer_bulk_read(usb_device_t* dev,
                           uint8_t endpoint,
                           unsigned char* buffer,
                           int length,
                           int* transferred,
                           unsigned int timeout);

int usb_transfer_bulk_write(usb_device_t* dev,
                            uint8_t endpoint,
                            const unsigned char* buffer,
                            int length,
                            int* transferred,
                            unsigned int timeout);

/* Asynchronous transfers (Phase 2.3) */
usb_transfer_ctx_t* usb_transfer_alloc(void);
void usb_transfer_free(usb_transfer_ctx_t* ctx);

int usb_transfer_async_submit(usb_device_t* dev,
                               usb_transfer_ctx_t* ctx,
                               uint8_t endpoint,
                               unsigned char* buffer,
                               int length);

int usb_transfer_async_wait(usb_transfer_ctx_t* ctx,
                             unsigned int timeout_ms);
```

**Acceptance Criteria**:
- [ ] Transfer context properly initialized/freed
- [ ] Thread-safe completion notification
- [ ] libusb transfer object lifecycle managed
- [ ] Error codes mapped from libusb to XOE error codes

---

#### 2.2 Control Transfers

**Implementation**:

Control transfers use `libusb_control_transfer()` for synchronous API:

```c
int usb_transfer_control(usb_device_t* dev,
                         uint8_t bmRequestType,
                         uint8_t bRequest,
                         uint16_t wValue,
                         uint16_t wIndex,
                         unsigned char* data,
                         uint16_t wLength,
                         unsigned int timeout) {
    int result;

    if (dev == NULL || dev->handle == NULL) {
        return E_INVALID_ARGUMENT;
    }

    result = libusb_control_transfer(
        dev->handle,
        bmRequestType,
        bRequest,
        wValue,
        wIndex,
        data,
        wLength,
        timeout
    );

    if (result < 0) {
        /* Map libusb error to XOE error code */
        return map_libusb_error(result);
    }

    return result;  /* Bytes transferred */
}
```

**Common Control Requests**:

1. **GET_DESCRIPTOR** (device/config/string):
   ```c
   unsigned char desc[18];
   usb_transfer_control(dev,
       LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD,
       LIBUSB_REQUEST_GET_DESCRIPTOR,
       (LIBUSB_DT_DEVICE << 8) | 0,
       0,
       desc,
       18,
       1000);
   ```

2. **SET_CONFIGURATION**:
   ```c
   usb_transfer_control(dev,
       LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_STANDARD,
       LIBUSB_REQUEST_SET_CONFIGURATION,
       1,  /* Configuration value */
       0,
       NULL,
       0,
       1000);
   ```

**Acceptance Criteria**:
- [ ] Control transfers execute successfully
- [ ] Setup packet properly formatted
- [ ] Both IN and OUT directions supported
- [ ] Timeouts handled correctly
- [ ] Error codes meaningful

---

#### 2.3 Bulk Transfers (Synchronous)

**Implementation**:

```c
int usb_transfer_bulk_read(usb_device_t* dev,
                           uint8_t endpoint,
                           unsigned char* buffer,
                           int length,
                           int* transferred,
                           unsigned int timeout) {
    int result;

    if (dev == NULL || dev->handle == NULL || buffer == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Ensure endpoint is IN direction */
    if (!(endpoint & LIBUSB_ENDPOINT_IN)) {
        return E_INVALID_ARGUMENT;
    }

    result = libusb_bulk_transfer(
        dev->handle,
        endpoint,
        buffer,
        length,
        transferred,
        timeout
    );

    if (result != 0) {
        return map_libusb_error(result);
    }

    return 0;  /* Success */
}

int usb_transfer_bulk_write(usb_device_t* dev,
                            uint8_t endpoint,
                            const unsigned char* buffer,
                            int length,
                            int* transferred,
                            unsigned int timeout) {
    int result;

    if (dev == NULL || dev->handle == NULL || buffer == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Ensure endpoint is OUT direction */
    if (endpoint & LIBUSB_ENDPOINT_IN) {
        return E_INVALID_ARGUMENT;
    }

    result = libusb_bulk_transfer(
        dev->handle,
        endpoint,
        (unsigned char*)buffer,  /* Cast away const for libusb */
        length,
        transferred,
        timeout
    );

    if (result != 0) {
        return map_libusb_error(result);
    }

    return 0;  /* Success */
}
```

**Error Mapping**:

```c
static int map_libusb_error(int libusb_code) {
    switch (libusb_code) {
        case LIBUSB_ERROR_TIMEOUT:
            return E_TIMEOUT;
        case LIBUSB_ERROR_NO_DEVICE:
            return E_NOT_FOUND;
        case LIBUSB_ERROR_ACCESS:
            return E_PERMISSION_DENIED;
        case LIBUSB_ERROR_NO_MEM:
            return E_OUT_OF_MEMORY;
        case LIBUSB_ERROR_INVALID_PARAM:
            return E_INVALID_ARGUMENT;
        default:
            return E_UNKNOWN_ERROR;
    }
}
```

**Acceptance Criteria**:
- [ ] Bulk read transfers data from device
- [ ] Bulk write transfers data to device
- [ ] Timeout parameter respected
- [ ] Actual transferred bytes reported
- [ ] Endpoint direction validated
- [ ] Errors properly mapped

---

#### 2.4 Asynchronous Transfer API

**Purpose**: Enable non-blocking I/O for better throughput and responsiveness.

**Implementation**:

```c
/* Callback for async completion */
static void transfer_callback(struct libusb_transfer* transfer) {
    usb_transfer_ctx_t* ctx = (usb_transfer_ctx_t*)transfer->user_data;

    pthread_mutex_lock(&ctx->lock);

    /* Update URB header with results */
    ctx->urb_header.actual_length = transfer->actual_length;
    ctx->urb_header.status = transfer->status;

    /* Set flags based on status */
    switch (transfer->status) {
        case LIBUSB_TRANSFER_COMPLETED:
            /* Success */
            break;
        case LIBUSB_TRANSFER_ERROR:
            ctx->urb_header.flags |= USB_FLAG_CRC_ERROR;
            break;
        case LIBUSB_TRANSFER_TIMED_OUT:
            ctx->urb_header.flags |= USB_FLAG_TIMEOUT;
            break;
        case LIBUSB_TRANSFER_STALL:
            ctx->urb_header.flags |= USB_FLAG_STALL;
            break;
        case LIBUSB_TRANSFER_NO_DEVICE:
            ctx->urb_header.flags |= USB_FLAG_NO_DEVICE;
            break;
        case LIBUSB_TRANSFER_OVERFLOW:
            ctx->urb_header.flags |= USB_FLAG_OVERFLOW;
            break;
        default:
            break;
    }

    ctx->completed = TRUE;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
}

usb_transfer_ctx_t* usb_transfer_alloc(void) {
    usb_transfer_ctx_t* ctx;

    ctx = (usb_transfer_ctx_t*)malloc(sizeof(usb_transfer_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    memset(ctx, 0, sizeof(usb_transfer_ctx_t));

    ctx->transfer = libusb_alloc_transfer(0);
    if (ctx->transfer == NULL) {
        free(ctx);
        return NULL;
    }

    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cond, NULL);
    ctx->completed = FALSE;

    return ctx;
}

void usb_transfer_free(usb_transfer_ctx_t* ctx) {
    if (ctx == NULL) {
        return;
    }

    if (ctx->transfer != NULL) {
        libusb_free_transfer(ctx->transfer);
    }

    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->cond);
    free(ctx);
}

int usb_transfer_async_submit(usb_device_t* dev,
                               usb_transfer_ctx_t* ctx,
                               uint8_t endpoint,
                               unsigned char* buffer,
                               int length) {
    struct libusb_transfer* transfer = ctx->transfer;

    libusb_fill_bulk_transfer(
        transfer,
        dev->handle,
        endpoint,
        buffer,
        length,
        transfer_callback,
        ctx,
        dev->config.transfer_timeout_ms
    );

    ctx->completed = FALSE;

    return libusb_submit_transfer(transfer);
}

int usb_transfer_async_wait(usb_transfer_ctx_t* ctx,
                             unsigned int timeout_ms) {
    struct timespec ts;
    int result;

    pthread_mutex_lock(&ctx->lock);

    if (!ctx->completed) {
        if (timeout_ms > 0) {
            /* Calculate absolute timeout */
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout_ms / 1000;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }

            result = pthread_cond_timedwait(&ctx->cond, &ctx->lock, &ts);
            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&ctx->lock);
                return E_TIMEOUT;
            }
        } else {
            pthread_cond_wait(&ctx->cond, &ctx->lock);
        }
    }

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}
```

**Event Loop Integration**:

```c
/* Event handling thread */
void* event_thread_func(void* arg) {
    struct libusb_context* ctx = (struct libusb_context*)arg;

    while (!g_shutdown_requested) {
        /* Handle libusb events (calls callbacks) */
        libusb_handle_events(ctx);
    }

    return NULL;
}
```

**Acceptance Criteria**:
- [ ] Async transfers submit successfully
- [ ] Callbacks execute on completion
- [ ] URB status properly updated
- [ ] Completion notification works
- [ ] Event loop thread handles callbacks
- [ ] Multiple in-flight transfers supported

---

### Phase 3: Client Integration

**Goal**: Integrate USB transfers with XOE network protocol.

#### 3.1 USB Client Session

**Files to Create**:
- `src/connectors/usb/usb_client.h`
- `src/connectors/usb/usb_client.c`

**Client Structure**:

```c
typedef struct {
    /* Configuration */
    usb_config_t config;

    /* USB device */
    usb_device_t device;

    /* Network connection */
    int network_fd;

    /* Threading */
    pthread_t usb_to_net_thread;
    pthread_t net_to_usb_thread;
    pthread_t event_thread;

    /* Buffers (reuse serial_buffer from serial connector) */
    struct serial_buffer* rx_buffer;  /* Network → USB */

    /* Sequence tracking */
    uint32_t tx_sequence;
    uint32_t rx_sequence;

    /* Shutdown coordination */
    pthread_mutex_t shutdown_mutex;
    int shutdown_flag;
} usb_client_t;
```

**Key Functions**:

```c
/* Lifecycle */
int usb_client_init(usb_client_t* client,
                    const usb_config_t* config,
                    int network_fd);
int usb_client_start(usb_client_t* client);
int usb_client_stop(usb_client_t* client);
void usb_client_cleanup(usb_client_t* client);

/* Entry point (called from FSM) */
int usb_client_run(const usb_config_t* config, int network_fd);
```

**Thread Functions**:

```c
/* USB → Network thread */
static void* usb_to_net_thread_func(void* arg) {
    usb_client_t* client = (usb_client_t*)arg;
    unsigned char buffer[USB_MAX_DATA_SIZE];
    usb_transfer_ctx_t* ctx;
    int result;

    ctx = usb_transfer_alloc();
    if (ctx == NULL) {
        return NULL;
    }

    while (!client->shutdown_flag) {
        /* Submit async bulk IN transfer */
        result = usb_transfer_async_submit(
            &client->device,
            ctx,
            client->config.bulk_in_endpoint,
            buffer,
            sizeof(buffer)
        );

        if (result != 0) {
            /* Handle error */
            break;
        }

        /* Wait for completion (with timeout for shutdown check) */
        result = usb_transfer_async_wait(ctx, 1000);

        if (result == E_TIMEOUT) {
            continue;  /* Check shutdown flag */
        }

        if (result != 0) {
            /* Handle error */
            break;
        }

        /* Encapsulate into XOE packet */
        xoe_packet_t packet;
        usb_urb_header_t* urb = &ctx->urb_header;

        urb->command = USB_RET_SUBMIT;
        urb->seqnum = client->tx_sequence++;
        urb->endpoint = client->config.bulk_in_endpoint;
        urb->transfer_type = USB_TRANSFER_BULK;

        result = usb_protocol_encapsulate(urb, buffer,
                                          urb->actual_length,
                                          &packet);
        if (result != 0) {
            /* Handle error */
            break;
        }

        /* Send to network */
        result = send_xoe_packet(client->network_fd, &packet);
        if (result != 0) {
            /* Handle network error */
            break;
        }

        /* Free packet payload */
        if (packet.payload && packet.payload->owns_data) {
            free(packet.payload->data);
            free(packet.payload);
        }
    }

    usb_transfer_free(ctx);
    return NULL;
}

/* Network → USB thread */
static void* net_to_usb_thread_func(void* arg) {
    usb_client_t* client = (usb_client_t*)arg;
    unsigned char buffer[USB_MAX_DATA_SIZE];
    xoe_packet_t packet;
    usb_urb_header_t urb_header;
    uint32_t data_len;
    int result;

    while (!client->shutdown_flag) {
        /* Receive XOE packet from network */
        result = recv_xoe_packet(client->network_fd, &packet);
        if (result != 0) {
            /* Handle network error or timeout */
            break;
        }

        /* Decapsulate */
        data_len = sizeof(buffer);
        result = usb_protocol_decapsulate(&packet, &urb_header,
                                          buffer, &data_len);

        /* Free packet */
        if (packet.payload && packet.payload->owns_data) {
            free(packet.payload->data);
            free(packet.payload);
        }

        if (result != 0) {
            /* Handle protocol error */
            continue;
        }

        /* Process command */
        if (urb_header.command == USB_CMD_SUBMIT) {
            /* Submit USB transfer */
            int transferred;
            result = usb_transfer_bulk_write(
                &client->device,
                urb_header.endpoint,
                buffer,
                urb_header.transfer_length,
                &transferred,
                client->config.transfer_timeout_ms
            );

            /* Send response */
            urb_header.command = USB_RET_SUBMIT;
            urb_header.actual_length = transferred;
            urb_header.status = result;

            /* Encapsulate and send response... */
        }
    }

    return NULL;
}
```

**Acceptance Criteria**:
- [ ] Client structure properly initialized
- [ ] Threads start and coordinate correctly
- [ ] USB data encapsulated into XOE packets
- [ ] Network data decapsulated and sent to USB
- [ ] Shutdown coordination works cleanly
- [ ] No resource leaks

---

#### 3.2 FSM State Handler

**Files to Create**:
- `src/core/fsm/state_client_usb.c`

**Implementation** (follows `state_client_serial.c` pattern):

```c
#include "core/config.h"
#include "connectors/usb/usb_client.h"
#include "connectors/usb/usb_config.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/* Signal handler */
static volatile sig_atomic_t g_interrupted = 0;

static void signal_handler(int signum) {
    (void)signum;
    g_interrupted = 1;
}

xoe_state_t state_client_usb(xoe_config_t* config) {
    usb_config_t* usb_cfg;
    int result;
    struct sigaction sa;

    if (config == NULL || config->usb_config == NULL) {
        fprintf(stderr, "Error: Invalid USB configuration\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    usb_cfg = (usb_config_t*)config->usb_config;

    /* Install signal handlers */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("Starting USB client mode...\n");
    printf("  Device: %04x:%04x\n",
           usb_cfg->vendor_id, usb_cfg->product_id);
    printf("  Interface: %d\n", usb_cfg->interface_number);
    printf("  Bulk IN: 0x%02x\n", usb_cfg->bulk_in_endpoint);
    printf("  Bulk OUT: 0x%02x\n", usb_cfg->bulk_out_endpoint);
    printf("  Server: %s:%d\n", config->server_address, config->port);

    /* Run USB client */
    result = usb_client_run(usb_cfg, config->server_fd);

    if (result != 0) {
        fprintf(stderr, "USB client error: %d\n", result);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    config->exit_code = EXIT_SUCCESS;
    return STATE_CLEANUP;
}
```

**Acceptance Criteria**:
- [ ] State handler integrated into FSM
- [ ] Signal handlers installed for graceful shutdown
- [ ] Configuration validated before starting
- [ ] Proper error reporting
- [ ] Exit codes set correctly

---

#### 3.3 Mode Selection Integration

**Files to Modify**:
- `src/core/fsm/state_mode_select.c`

**Changes**:

```c
xoe_state_t state_mode_select(xoe_config_t* config) {
    /* ... existing code ... */

    if (config->use_serial) {
        return STATE_CLIENT_SERIAL;
    }

    if (config->use_usb) {
        return STATE_CLIENT_USB;  /* New state */
    }

    /* ... rest of existing code ... */
}
```

**Acceptance Criteria**:
- [ ] USB mode properly routed from mode selection
- [ ] Mutually exclusive with other client modes
- [ ] Validation ensures required config present

---

### Phase 4: Testing and Validation

**Goal**: Ensure USB connector works reliably with real devices.

#### 4.1 Unit Tests

**Files to Create**:
- `src/tests/unit/test_usb_protocol.c`
- `src/tests/unit/test_usb_config.c`
- `src/tests/unit/test_usb_transfer.c`

**Test Coverage**:

1. **Protocol Tests** (`test_usb_protocol.c`):
   - [ ] URB encapsulation/decapsulation round-trip
   - [ ] Checksum calculation correctness
   - [ ] All transfer types handled
   - [ ] Error flags properly set
   - [ ] Edge cases (zero-length transfers, max size)

2. **Configuration Tests** (`test_usb_config.c`):
   - [ ] Default initialization
   - [ ] VID/PID validation
   - [ ] Endpoint address validation
   - [ ] Invalid configurations rejected

3. **Transfer Tests** (`test_usb_transfer.c`):
   - [ ] Transfer context allocation/free
   - [ ] Callback mechanism
   - [ ] Error mapping
   - [ ] Timeout handling

**Test Framework Integration**:

```bash
make test-unit     # Run all unit tests
make test-usb      # Run USB-specific tests
```

**Acceptance Criteria**:
- [ ] All unit tests pass
- [ ] Code coverage > 80% for USB modules
- [ ] No memory leaks (valgrind clean)

---

#### 4.2 Integration Tests

**Test Scenarios**:

1. **Device Enumeration**:
   ```bash
   # List all connected USB devices
   ./bin/xoe --list-usb
   ```
   - [ ] Displays VID:PID, manufacturer, product
   - [ ] Shows interfaces and endpoints

2. **Echo Test** (loopback):
   ```bash
   # Server
   ./bin/xoe -p 12345

   # Client (with USB device that echoes)
   ./bin/xoe -c localhost:12345 -u 1234:5678 --interface 0
   ```
   - [ ] Data sent to bulk OUT
   - [ ] Same data received from bulk IN
   - [ ] No data corruption

3. **CDC-ACM Device** (USB serial adapter):
   ```bash
   ./bin/xoe -c localhost:12345 -u 067b:2303 --interface 0
   ```
   - [ ] Device opens successfully
   - [ ] Can send/receive serial data
   - [ ] Comparable to native serial connection

4. **HID Device - USB Mouse** (Primary Focus):
   ```bash
   ./bin/xoe -c localhost:12345 -u 046d:c077 --interface 0
   ```
   - [ ] Device opens successfully
   - [ ] Interrupt IN endpoint detected
   - [ ] Mouse movement reports received
   - [ ] Button press events captured

5. **HID Device - USB Keyboard** (Primary Focus):
   ```bash
   ./bin/xoe -c localhost:12345 -u 046d:c31c --interface 0
   ```
   - [ ] Device opens successfully
   - [ ] Key press reports received via interrupt endpoint
   - [ ] Modifier keys (Shift, Ctrl, Alt) handled correctly
   - [ ] LED output reports work (Caps Lock, Num Lock)

6. **Multiple Device Test**:
   ```bash
   ./bin/xoe -c localhost:12345 -u 1234:5678 -u 046d:c077
   ```
   - [ ] Both devices open simultaneously
   - [ ] Device routing works correctly
   - [ ] No crosstalk between device streams

**Acceptance Criteria**:
- [ ] HID devices (mouse, keyboard) tested and working
- [ ] Multiple devices simultaneously supported
- [ ] At least 3 different device types tested (HID + CDC-ACM or others)
- [ ] No crashes or hangs
- [ ] Proper error messages on failures
- [ ] Graceful handling of device removal
- [ ] Device-to-packet routing works correctly

---

#### 4.3 Performance Testing

**Metrics to Measure**:

1. **Throughput**:
   - Bulk transfer speed (MB/s)
   - Compare local vs. over network
   - Impact of network latency

2. **Latency**:
   - Round-trip time for small transfers
   - Interrupt endpoint response time

3. **CPU Usage**:
   - Idle vs. active transfer
   - Threading efficiency

**Test Tools**:

```bash
# Throughput test
./tests/usb_throughput_test -u 1234:5678 -s 1048576

# Latency test
./tests/usb_latency_test -u 1234:5678 -n 1000
```

**Acceptance Criteria**:
- [ ] Bulk throughput > 10 MB/s on LAN
- [ ] Latency < 10ms for small transfers on LAN
- [ ] CPU usage < 50% during active transfers
- [ ] No memory leaks over extended runs

---

### Phase 5: Documentation and Advanced Features

**Goal**: Complete documentation and add advanced capabilities.

#### 5.1 Documentation

**Files to Create/Update**:

1. **README Updates** (`README.md`):
   - [ ] USB connector overview
   - [ ] Installation requirements (libusb-1.0)
   - [ ] Usage examples
   - [ ] Supported device classes

2. **USB Connector Guide** (`docs/connectors/USB.md`):
   - [ ] Architecture overview
   - [ ] Protocol specification
   - [ ] Threading model
   - [ ] Command-line reference
   - [ ] Troubleshooting guide

3. **Developer Guide** (`docs/development/USB_DEVELOPMENT.md`):
   - [ ] Adding new device class support
   - [ ] Extending protocol
   - [ ] Debugging techniques

**Acceptance Criteria**:
- [ ] All public APIs documented
- [ ] Usage examples provided
- [ ] Architecture diagrams included
- [ ] Troubleshooting section comprehensive

---

#### 5.2 Hotplug Support (Optional)

**Implementation**:

1. **Hotplug Callback Registration**:
   ```c
   int usb_hotplug_register(struct libusb_context* ctx,
                            usb_hotplug_callback_fn callback);
   ```

2. **Dynamic Device Management**:
   - Add device to active list on arrival
   - Remove and cleanup on departure
   - Notify network peers of topology changes

**Acceptance Criteria**:
- [ ] Devices detected when plugged in
- [ ] Sessions gracefully closed on removal
- [ ] No crashes on hot-unplug
- [ ] Clients notified of device status

---

#### 5.3 Interrupt Transfers

**Implementation**:

Similar to bulk transfers but using `libusb_interrupt_transfer()`:

```c
int usb_transfer_interrupt_read(usb_device_t* dev,
                                uint8_t endpoint,
                                unsigned char* buffer,
                                int length,
                                int* transferred,
                                unsigned int timeout);
```

**Use Cases**:
- HID device reports
- Device status notifications
- Low-latency events

**Acceptance Criteria**:
- [ ] Interrupt IN transfers work
- [ ] Polling interval respected
- [ ] Integration with URB protocol
- [ ] HID device tested (e.g., USB mouse)

---

## Testing Strategy

### Unit Testing
- Protocol encapsulation/decapsulation
- Configuration validation
- Transfer abstraction layer
- Error handling paths

### Integration Testing
- End-to-end device communication
- Multiple device types (CDC-ACM, Mass Storage, HID)
- Network reliability (packet loss, latency)
- Device hot-plug/removal

### Platform Testing
- Linux (Ubuntu, Debian, Fedora)
- macOS (multiple versions)
- BSD (FreeBSD, OpenBSD)

### Performance Testing
- Throughput benchmarks
- Latency measurements
- CPU/memory profiling
- Long-duration stability tests

---

## Risk Mitigation

### Technical Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **C89 incompatibility with libusb** | High | Create compatibility wrapper, test early |
| **Timing constraints over network** | High | Use async API, document limitations |
| **Kernel driver conflicts** | Medium | Auto-detach, provide troubleshooting docs |
| **Platform-specific issues** | Medium | Test on all platforms, CI/CD integration |
| **Device class complexity** | Medium | Start with generic bulk, add classes incrementally |

### Implementation Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **Scope creep** | High | Strict phase boundaries, defer advanced features |
| **Insufficient testing** | High | Comprehensive test plan, multiple device types |
| **Documentation lag** | Medium | Write docs during implementation, not after |
| **Performance issues** | Medium | Early benchmarking, profiling tools |

---

## Success Criteria

### Phase 1 (Foundation)
- [ ] libusb-1.0 integrated with C89 compatibility
- [ ] Protocol definitions complete and tested
- [ ] Device enumeration works on all platforms
- [ ] Configuration structures validated

### Phase 2 (Transfers)
- [ ] Control transfers functional
- [ ] Bulk transfers (sync and async) working
- [ ] Error handling comprehensive
- [ ] Transfer abstraction layer complete

### Phase 3 (Integration)
- [ ] USB client sessions run successfully
- [ ] FSM integration complete
- [ ] Network encapsulation works
- [ ] Graceful shutdown implemented

### Phase 4 (Validation)
- [ ] All unit tests pass
- [ ] Integration tests cover 3+ device types
- [ ] Performance meets targets
- [ ] No memory leaks or crashes

### Phase 5 (Polish)
- [ ] Documentation complete
- [ ] Optional features (hotplug, interrupt) implemented
- [ ] User-facing guide published
- [ ] Developer guide available

---

## Timeline and Effort Estimate

### Phase 1: Foundation (Implementation)
- Library integration: Implementation task
- Protocol definitions: Implementation task
- Configuration structures: Implementation task
- Device enumeration: Implementation task

### Phase 2: Transfers (Implementation)
- Transfer abstraction: Implementation task
- Control transfers: Implementation task
- Bulk transfers: Implementation task
- Async API: Implementation task

### Phase 3: Integration (Implementation)
- Client session: Implementation task
- FSM integration: Implementation task
- Network protocol: Implementation task

### Phase 4: Testing (Implementation)
- Unit tests: Implementation task
- Integration tests: Implementation task
- Performance tests: Implementation task

### Phase 5: Documentation (Implementation)
- User guide: Implementation task
- Developer guide: Implementation task
- Advanced features: Implementation task

**Note**: Timeline estimates removed per project guidelines. Implementation proceeds phase by phase.

---

## Architect Decisions

1. **Device Class Priority**: **HID (Human Interface Device)**
   - Decision: Focus on HID devices first (keyboards, mice, game controllers, etc.)
   - Rationale: Common use case, well-defined protocol, good for testing

2. **Isochronous Support**: **Deferred indefinitely**
   - Decision: Do not implement isochronous transfers
   - Rationale: Network latency makes audio/video impractical; focus on control/bulk/interrupt

3. **Multi-Device Support**: **Multiple devices per client**
   - Decision: Design for multiple simultaneous USB devices from the start
   - Rationale: Enables more flexible deployments, avoid future refactoring

## Implementation Notes Based on Decisions

### HID Device Focus
- **Phase 1**: Ensure HID device enumeration works correctly
- **Phase 2**: Prioritize interrupt transfers (required for HID input reports)
- **Phase 3**: Test with common HID devices (mouse, keyboard, game controller)
- **Configuration**: Add HID-specific endpoint detection and configuration helpers

### Multi-Device Architecture
- **Device Manager**: Create `usb_device_manager.c` to track multiple active devices
- **Device Registry**: Maintain list of open devices with unique device_id per session
- **Client Structure**: Modify `usb_client_t` to support device array instead of single device
- **Threading**: One USB→Network thread per device, shared Network→USB thread with device routing
- **Protocol**: Use `device_id` field in URB header to route packets to correct device
- **Command-Line**: Support multiple `-u` flags or device list file

### Removed from Scope
- **Isochronous Transfers**: Remove all ISO-related code from plan
- **Audio/Video Devices**: Document as unsupported in limitations section
- **Phase 5.4**: Remove isochronous implementation section entirely

### Open Questions Remaining

4. **Protocol Versioning**: Should we plan for protocol evolution (v2, v3)?
   - Recommendation: Yes, include version field in all packets

5. **Server-Side Changes**: Does the server need USB-specific handling, or is generic packet forwarding sufficient?
   - Recommendation: Generic forwarding initially, add USB-aware routing later if needed

6. **Security**: Should we add USB-specific authentication/authorization?
   - Recommendation: Defer to future enhancement, rely on network-level security initially

7. **Compression**: Would protocol-level compression benefit bulk transfers?
   - Recommendation: Measure performance first, add if needed

---

## References

### USB Specifications
- [USB in a NutShell](https://www.beyondlogic.org/usbnutshell/)
- [USB 2.0 Specification](https://www.usb.org/document-library/usb-20-specification)
- [USB Class Codes](https://www.usb.org/defined-class-codes)

### libusb Documentation
- [libusb-1.0 API Reference](https://libusb.sourceforge.io/api-1.0/)
- [libusb Documentation](https://libusb.readthedocs.io/)
- [libusb GitHub](https://github.com/libusb/libusb)

### Related Projects
- [USB/IP](http://usbip.sourceforge.net/)
- [VirtualHere](https://www.virtualhere.com/) (commercial reference)

### XOE Project Documentation
- [Serial Connector Implementation](src/connectors/serial/)
- [Protocol Abstraction Layer](src/lib/protocol/protocol.h)
- [Coding Standards](CLAUDE.md)

---

## Appendix A: File Checklist

### New Files to Create

**Library Components** (`src/lib/`):
- [ ] `src/lib/usb_compat.h` - C89 compatibility wrapper

**USB Connector** (`src/connectors/usb/`):
- [ ] `usb_client.c` - Client coordinator
- [ ] `usb_client.h` - Client interface
- [ ] `usb_config.h` - Configuration structures
- [ ] `usb_device.c` - Device management (single device operations)
- [ ] `usb_device.h` - Device interface
- [ ] `usb_device_manager.c` - Multi-device manager (tracks active devices)
- [ ] `usb_device_manager.h` - Device manager interface
- [ ] `usb_protocol.c` - Protocol implementation
- [ ] `usb_protocol.h` - Protocol definitions
- [ ] `usb_transfer.c` - Transfer handling
- [ ] `usb_transfer.h` - Transfer interface
- [ ] `usb_hotplug.c` - Hotplug support (Phase 5)

**FSM State Handler** (`src/core/fsm/`):
- [ ] `state_client_usb.c` - USB client state

**Tests** (`src/tests/`):
- [ ] `tests/unit/test_usb_protocol.c`
- [ ] `tests/unit/test_usb_config.c`
- [ ] `tests/unit/test_usb_transfer.c`
- [ ] `tests/integration/test_usb_device.c`

**Documentation** (`docs/`):
- [ ] `docs/connectors/USB.md` - User guide
- [ ] `docs/development/USB_DEVELOPMENT.md` - Developer guide

### Files to Modify

**Core Configuration**:
- [ ] `src/core/config.h` - Add USB mode and config
- [ ] `src/core/fsm/state_init.c` - Initialize USB config
- [ ] `src/core/fsm/state_parse_args.c` - Parse USB arguments
- [ ] `src/core/fsm/state_mode_select.c` - Route to USB state
- [ ] `src/core/fsm/state_cleanup.c` - Cleanup USB resources

**Protocol Definitions**:
- [ ] `src/lib/protocol/protocol.h` - Add XOE_PROTOCOL_USB

**Common Definitions**:
- [ ] `src/lib/common/definitions.h` - Add USB error codes

**Build System**:
- [ ] `Makefile` - Add USB sources, `-lusb-1.0` flag

**Documentation**:
- [ ] `README.md` - Add USB connector overview
- [ ] `CLAUDE.md` - Update with USB-specific guidelines

---

## Appendix B: Command-Line Interface

### Proposed USB Options

```bash
Usage: xoe [OPTIONS]

USB Connector Options:
  -u, --usb <vid:pid>           USB device (vendor:product ID in hex)
      --interface <num>         USB interface number (default: 0)
      --ep-in <addr>            Bulk IN endpoint address (hex, e.g., 0x81)
      --ep-out <addr>           Bulk OUT endpoint address (hex, e.g., 0x01)
      --ep-int <addr>           Interrupt IN endpoint address (optional)
      --usb-timeout <ms>        USB transfer timeout in ms (default: 5000)
      --list-usb                List all connected USB devices and exit
      --detach-kernel           Auto-detach kernel driver if active

Examples:
  # List USB devices
  xoe --list-usb

  # Connect to server with USB device (auto-detect endpoints)
  xoe -c 192.168.1.100:12345 -u 1234:5678

  # Multiple devices simultaneously
  xoe -c 192.168.1.100:12345 -u 1234:5678 -u 046d:c077 -u 067b:2303

  # HID device (mouse)
  xoe -c 192.168.1.100:12345 -u 046d:c077 --interface 0

  # HID device (keyboard)
  xoe -c 192.168.1.100:12345 -u 046d:c31c --interface 0

  # Explicit endpoint configuration
  xoe -c 192.168.1.100:12345 -u 04d8:000a --interface 0 \
      --ep-in 0x81 --ep-out 0x01

  # USB serial adapter (CDC-ACM)
  xoe -c 192.168.1.100:12345 -u 067b:2303 --interface 0
```

---

## Appendix C: Error Codes

### USB-Specific Error Codes

Add to `src/lib/common/definitions.h`:

```c
/* USB-specific errors (start at -100) */
#define E_USB_NOT_FOUND        -100  /* USB device not found */
#define E_USB_ACCESS_DENIED    -101  /* Permission denied */
#define E_USB_BUSY             -102  /* Device or resource busy */
#define E_USB_NO_DEVICE        -103  /* Device disconnected */
#define E_USB_NOT_SUPPORTED    -104  /* Operation not supported */
#define E_USB_PIPE_ERROR       -105  /* Pipe/endpoint error */
#define E_USB_OVERFLOW         -106  /* Buffer overflow */
#define E_USB_TIMEOUT          -107  /* Transfer timeout */
#define E_USB_CANCELLED        -108  /* Transfer cancelled */
#define E_USB_STALL            -109  /* Endpoint stalled */
#define E_USB_PROTOCOL_ERROR   -110  /* Protocol error */
```

---

## End of Plan

This implementation plan provides a comprehensive roadmap for adding USB connector support to the XOE framework. The plan follows the established architectural patterns from the serial connector while accommodating the additional complexity of the USB protocol.

**Next Steps**:
1. Review and approve this plan
2. Create feature branch: `git checkout -b feature/usb-connector`
3. Begin Phase 1 implementation
4. Iterate through phases with testing and validation at each stage
5. Submit pull request upon completion

**Contribution Label**: `[LLM-ARCH]` - Software architect leveraging LLM for code generation while reviewing, adjusting, and confirming all plans.
