# USB Connector Quick Start Guide

**Status**: ⚠️ EXPERIMENTAL - Awaiting community hardware testing

This guide provides quick instructions for testing the XOE USB connector implementation.

---

## Prerequisites

### System Requirements

- **Linux, macOS, or BSD** (POSIX systems only)
- **libusb-1.0.20 or newer**
- **GCC or Clang** compiler
- **Physical USB device** for testing
- **Root/sudo access** (required for USB device access)

### Install libusb

**Ubuntu/Debian**:
```bash
sudo apt-get install libusb-1.0-0-dev
```

**macOS (Homebrew)**:
```bash
brew install libusb
```

**FreeBSD**:
```bash
pkg install libusb
```

---

## Build XOE with USB Support

```bash
# Clone and build
cd xoe
make clean && make

# Verify USB support is compiled
./bin/xoe -h | grep -A 5 "USB"
```

---

## Quick Test Procedure

### Step 1: Identify Your USB Device

```bash
# Linux
lsusb

# macOS
system_profiler SPUSBDataType

# Example output:
# Bus 001 Device 003: ID 046d:c077 Logitech, Inc. M105 Optical Mouse
#                      ^^^^ ^^^^
#                      VID  PID
```

Note the **VID:PID** (Vendor ID:Product ID) of the device you want to test.

### Step 2: Check Permissions

**Linux** - Create udev rule for non-root access:
```bash
# Create udev rule for your device
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="046d", ATTR{idProduct}=="c077", MODE="0666"' | \
  sudo tee /etc/udev/rules.d/99-xoe-usb.rules

# Reload udev
sudo udevadm control --reload-rules
sudo udevadm trigger

# Unplug and replug device
```

**macOS** - May require running as root:
```bash
sudo ./bin/xoe ...
```

### Step 3: Start XOE Server

```bash
# Terminal 1: Start server
./bin/xoe -p 12345
```

Server will initialize and wait for client connections.

### Step 4: Connect USB Client

```bash
# Terminal 2: Start USB client (replace VID:PID with your device)
./bin/xoe -c localhost:12345 -u 046d:c077
```

**Expected Output**:
```
Connecting to server: localhost:12345
Connected successfully
Opening USB device 046d:c077...
USB device opened successfully
Manufacturer: Logitech
Product: USB Mouse
Claiming interface 0...
Interface claimed successfully

Registering USB devices with server...
  Registering device 1: VID:PID 046d:c077 (device_id=0x046dc077)
Device 0x046dc077 registered with server successfully
All devices registered successfully

USB client running... (Press Ctrl+C to stop)
```

---

## Testing Scenarios

### Test 1: Device Enumeration

**Goal**: Verify device can be opened and identified.

```bash
./bin/xoe -c localhost:12345 -u YOUR_VID:YOUR_PID
```

**Success Indicators**:
- Device opens without errors
- Manufacturer/product strings displayed
- Interface claimed successfully
- Device registration succeeds

### Test 2: HID Device (Mouse)

**Goal**: Test interrupt endpoint with USB mouse.

```bash
# Server
./bin/xoe -p 12345

# Client with mouse (example VID:PID)
./bin/xoe -c localhost:12345 -u 046d:c077
```

**What to Look For**:
- Move mouse and observe transfer activity in logs
- Check for interrupt endpoint detection
- Verify data packets are received

### Test 3: Multiple Devices

**Goal**: Test multi-device support.

```bash
# Server
./bin/xoe -p 12345

# Client with multiple devices
./bin/xoe -c localhost:12345 -u 046d:c077 -u 046d:c31c
```

**Expected**:
- Both devices enumerate
- Each device registers separately
- No routing conflicts

---

## Troubleshooting

### "Permission denied" Error

**Linux**:
```bash
# Option 1: Run as root (not recommended)
sudo ./bin/xoe -c localhost:12345 -u 046d:c077

# Option 2: Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in

# Option 3: Create udev rule (see Step 2 above)
```

**macOS**:
```bash
# Run as root
sudo ./bin/xoe -c localhost:12345 -u 046d:c077
```

### "Device not found" Error

1. **Verify device is connected**: `lsusb`
2. **Check VID:PID format**: Must be hex (e.g., `046d:c077`)
3. **Try unplugging and replugging** device

### "Interface claim failed" Error

**Possible Causes**:
- Kernel driver already attached
- Device in use by another program

**Solutions**:
```bash
# Linux: Detach kernel driver automatically
./bin/xoe -c localhost:12345 -u 046d:c077 --detach-kernel

# macOS: Close programs using the device
# (e.g., close System Preferences if testing mouse)
```

### No Data Transfer Activity

1. **Check device type**: Isochronous devices (audio/video) are NOT supported
2. **Verify endpoints**: Some devices require explicit endpoint specification
3. **Enable debug output**: Add `-v` flag for verbose logging (future enhancement)

---

## Supported Device Classes

### ✅ Supported

- **HID (Human Interface Devices)**
  - Keyboards
  - Mice
  - Game controllers
  - Generic HID devices

- **CDC-ACM (USB Serial)**
  - USB-to-serial adapters
  - Virtual COM ports

- **Bulk Transfer Devices**
  - Custom USB devices
  - Some storage devices (basic transfers only)

### ❌ Not Supported

- **Audio devices** - Requires isochronous transfers
- **Video devices** - Requires isochronous transfers
- **Webcams** - Requires isochronous transfers
- **Timing-sensitive devices** - Network latency makes real-time impractical

---

## Reporting Test Results

When reporting test results, please include:

1. **Device Information**:
   - Device type (mouse, keyboard, etc.)
   - VID:PID
   - Manufacturer/product strings

2. **Platform**:
   - OS (Linux/macOS/BSD)
   - OS version
   - libusb version: `pkg-config --modversion libusb-1.0`

3. **Test Outcome**:
   - Device enumeration: Success/Failure
   - Registration: Success/Failure
   - Data transfer: Success/Failure
   - Any error messages

4. **Steps to Reproduce**:
   - Exact commands used
   - Any special setup required

**Submit Reports**:
- GitHub Issues (link TBD)
- Include full console output
- Attach relevant logs

---

## Known Limitations

- **Network Latency**: Not suitable for real-time or latency-sensitive devices
- **No Isochronous**: Audio/video devices will not work
- **Experimental**: Limited real-world testing
- **Platform Quirks**: macOS may have additional KEXT conflicts

---

## Advanced Usage

### Explicit Endpoint Configuration

For devices with non-standard endpoints:

```bash
./bin/xoe -c localhost:12345 -u 1234:5678 \
  --interface 0 \
  --ep-in 0x81 \
  --ep-out 0x01
```

### Multiple Interfaces

Some devices have multiple interfaces:

```bash
# Specify interface number
./bin/xoe -c localhost:12345 -u 1234:5678 --interface 1
```

---

## Next Steps

1. **Test with your devices** - Report results!
2. **Review implementation**: `src/connectors/usb/`
3. **Read full plan**: `.plan/usb-connector-implementation.md`
4. **Contribute improvements** - See CONTRIBUTING.md

---

## Getting Help

- **Documentation**: `docs/connectors/` (more docs coming after testing)
- **Issues**: GitHub Issues (link TBD)
- **Discussions**: GitHub Discussions (link TBD)

---

**Last Updated**: December 5, 2025
**Status**: Experimental - Awaiting Hardware Testing
**Feedback Welcome**: Help us validate the implementation!
