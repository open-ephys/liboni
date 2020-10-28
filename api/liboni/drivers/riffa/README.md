# `RIFFA` ONI Translation Layer
[RIFFA](https://github.com/KastnerRG/riffa) is an open-source kernel driver and
FPGA core that abstracts PCIe communication and allows its easy integration
into C programs. RIFFA is currently the PCIe backed used by ONIX hardware. This
ONI translation layer converts RIFFA's raw API into ONI-compatible API by
implementing all the functions in `onidriver.h`. 

**NOTE** Currently, when using RIFFA, we are only supporting 64-bit architectures.

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

## Obtaining Host Device Driver
The RIFFA device driver is available in the `drivers` folder at the top level
of this repo. We have modified this slightly compared to the original driver so
you should use this. You can compile it from source, but we have also included
pre-compiled binaries that can be installed in Windows or Linux. This driver
must be installed before using the RIFFA backend.


