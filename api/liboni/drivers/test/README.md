# Test ONI Translation Layer
This is a test ONI translation layer that creates a simple device table and
generates data that is useful for testing ONI-compliant APIs. This is a minimal
implementation has the following limitations:

- Fixed size device table
- Fixed size data frame
- Block read size must be set to a multiple of the frame size 
    - 48 bytes, currently
- Writing frames is implemented without a visible effect: data is just ignored 
- Data generation takes place on the read thread.
    - Side effect: ONI_OPT_RUNNING does nothing. 
- Minimal register R/W facility
    - Each device has a single register at address 0 that can be read from and written to
    - All else will NACK.

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
