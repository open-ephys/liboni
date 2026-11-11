# Test ONI Translation Layer
This is a test ONI translation layer that creates a simple device table and
generates data that is useful for testing ONI-compliant APIs. This is a minimal
implementation has the following limitations:

- Fixed size device table
- Writing frames is implemented without a visible effect: data is just ignored 
- Data generation takes place on the read thread.

Like hardware, a host index can be held by one context at a time. Opening a host
index that another context of the process holds fails with `ONI_ERETRIG`, as the
riffa driver does, until that context is destroyed. A negative host index opens
host index 0.

Like hardware, the context starts with acquisition stopped. Data stream reads
block until acquisition is started by setting `ONI_OPT_RUNNING` to 1 or
`ONI_OPT_RESETACQCOUNTER` to 2. Stopping acquisition flushes the data stream,
discarding the frame that was partially sent, and acquisition always starts on a
frame boundary.

## Features for testing host software
- **Hub firmware version**: hubs report firmware version 2.0 (`0x0200`). Opening
  the context with host index 128 makes them report `0xFFFF` instead, a version
  that will never be valid, for testing how host software handles unsupported
  firmware.
- **Fault injection registers**: registers 4 to 7 can be accessed through any
  test device, but act on the whole context.
- **Read fault injection**: writing N > 0 to register 4 of any test device lets
  N more data stream reads succeed, after which every data stream read fails
  with `ONI_EREADFAILURE` until the context is reset (`ONI_OPT_RESET`) or
  destroyed. This emulates a hardware failure during acquisition, such as a
  buffer overflow. Writing 0 before the fault is triggered disarms it. Reading
  the register returns the number of reads remaining before the fault, or 0 if
  no fault is armed.
- **Write fault injection**: register 5 does the same for data stream writes,
  which then fail with `ONI_EWRITEFAILURE`.
- **Read stall injection**: writing N > 0 to register 6 of any test device makes
  the next data stream read block for N ms before it proceeds, once. This
  emulates hardware that stops producing frames for a while. Reading the
  register returns the stall that is still pending, or 0 if none is.
- **Frame corruption injection**: writing N > 0 to register 7 of any test device
  lets N more data stream reads produce valid frames, after which every frame
  header is corrupt until acquisition stops. This emulates a hardware buffer
  overflow. Corrupt headers alternate between a device index that is not in the
  device table and a data size that differs from the device's read size.
  Writing 0 before corruption starts disarms it. Reading the register returns
  the number of reads remaining before corruption starts, or 0 if none is armed.
- **Initialization failure**: opening the context with host index 129 fails
  with `ONI_EINIT`, without holding the host index.
- **Device table failure**: with host index 130, the driver opens and holds the
  host index, but the device table it reports repeats a device index, so
  initializing the context fails with `ONI_EDEVIDXREPEAT`. The host index stays
  held until the context is destroyed, which tests that host software destroys
  a context whose initialization failed.

| Register | Name | Access | Description |
|---|---|---|---|
| 0 | Enable | R/W | Enables the device's data stream |
| 1 | Message | R/W | Value of the message word in each frame |
| 2 | Dummy words | R | Number of counter words following the message |
| 3 | Frame rate | R | Frame rate in Hz, always 0 (undefined) |
| 4 | Read fault | R/W | Data stream reads remaining before an injected read fault |
| 5 | Write fault | R/W | Data stream writes remaining before an injected write fault |
| 6 | Read stall | R/W | Duration in ms of a stall injected into the next data stream read |
| 7 | Frame corruption | R/W | Data stream reads remaining before frame headers are corrupted |

## Building the library
### Linux
```
make                # Build without debug symbols
sudo make install   # Install in /usr/local and run ldconfig to update library cache
make help           # list all make options
```

### Windows
Run the project in Visual Studio. It can be included as a dependency in other
projects.
