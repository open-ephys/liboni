# Test ONI Translation Layer
This is a test ONI translation layer that creates a simple device table and
generates data that is useful for testing ONI-compliant APIs. This is a minimal
implementation has the following limitations:

- Fixed size device table
- Writing frames is implemented without a visible effect: data is just ignored 
- Data generation takes place on the read thread.

Like hardware, the context starts with acquisition stopped. Data stream reads
block until acquisition is started by setting `ONI_OPT_RUNNING` to 1 or
`ONI_OPT_RESETACQCOUNTER` to 2. Stopping acquisition discards the frame that was
partially sent, as hardware flushes its FIFOs.

## Features for testing host software
- **Hub firmware version**: hubs report firmware version 2.0 (`0x0200`). Opening
  the context with host index 128 makes them report `0xFFFF` instead, a version
  that will never be valid, for testing how host software handles unsupported
  firmware.
- **Read fault injection**: writing N > 0 to register 4 of any test device lets
  N more data stream reads succeed, after which every data stream read fails
  with `ONI_EREADFAILURE` until the context is reset (`ONI_OPT_RESET`) or
  destroyed. This emulates a hardware failure during acquisition, such as a
  buffer overflow. Writing 0 before the fault is triggered disarms it. Reading
  the register returns the number of reads remaining before the fault, or 0 if
  no fault is armed.

| Register | Name | Access | Description |
|---|---|---|---|
| 0 | Enable | R/W | Enables the device's data stream |
| 1 | Message | R/W | Value of the message word in each frame |
| 2 | Dummy words | R | Number of counter words following the message |
| 3 | Frame rate | R | Frame rate in Hz, always 0 (undefined) |
| 4 | Read fault | R/W | Data stream reads remaining before an injected read fault |

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
