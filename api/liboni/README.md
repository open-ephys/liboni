# `liboni`
ANSI C implementation of the [Open Neuro Interface API
Specification](https://open-ephys.github.io/ONI/api/index.html).

## Scope and External Dependencies
`liboni` is written in C to facilitate cross platform and cross-language use and provide adeq. It is composed of the following files:

1. oni.h and oni.c: main API implementation
1. onix.h and onix.c: ONIX-specific extension funtions. These filese can be ignored for third party projects. These files are likely to be deprecated in the future.
1. ondriverloader.h and onidriverloader.c: functions for dynamically loading
   hardware translation driver.
1. onidriver.h: hardware translation driver header that must be implemented for
   a particular host hardware

`liboni` is a low level library used by high-level language binding and/or
software plugin developers.

## License
[MIT](https://en.wikipedia.org/wiki/MIT_License)

## Build

### Linux
For build options, look at the [Makefile](Makefile). To build and install:
```
$ make <options>
$ make install PREFIX=/path/to/install
```
to place headers in whatever path is specified by PREFIX. PREFIX defaults to
`/usr/lib/include`. You can uninstall (delete headers and libraries) via
```
$ make uninstall PREFIX=/path/to/uninstall
```
To make a particular driver, navigate to its location within the `drivers`
subdirectory and:
```
$ make <options>
$ make install PREFIX=/path/to/install
```
Then update dynamic library cache via:
```
$ ldconfig
```

### Windows
Open the included Visual Studio solution and press play. For whatever reason,
it seems that the startup project is not consistently saved with the solution.
So make sure that is set to `liboni-test` in the solution properties.

## Test Programs (Linux Only)
The [liboni-test](liboni-test) directory contains minimal working programs that use this library.

1. `oni-repl` : Basic data acquisition with a Read Evaluate Print Loop for changing runtime behavior.

## Performance Testing (Linux Only)
1. Install google perftools:
```
$ sudo apt-get install google-perftools
```
1. Check what library is installed:
```
ldconfig -p | grep profiler
```
if `libprofiler.so` is not there, but `libprofiler.so.x` exists, create a softlink:
```
sudo ln -rs /path/to/libprofiler.so.x /path/to/libprofiler.so
```
1. Link test programs against the CPU profiler:
```
$ cd oni-repl
$ make profile
```
1. Run `oni-repl` program using while dumping profile info:
```
$ env CPUPROFILE=/tmp/oni-repl.prof ./oni-repl test 0 --rbytes=28
```
1. Examine output
```
$ pprof ./oni-repl /tmp/oni-repl.prof
```

## Memory Testing (Linux Only)
Run `oni-repl` with valgrind using full leak check
```
$ valgrind --leak-check=full ./oni-repl test 0 --rbytes=28
```
