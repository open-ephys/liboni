# clroni
CLR/.NET bindings for [`liboni`](../liboni/README.md).

## Build

### Visual Studio (Windows)
1. Open the `clroni.sln` solution in visual studio. 
2. "Running" the solution will compile the library, test program, and
   generate a nuget package.

### Mono
[Mono](https://github.com/mono/mono) is an open source .NET implementation.
`mcs` is the mono C# compiler.

```
$ cd clroni
$ make
```

## Test Program
The [clroni-repl](clroni-repl) command line program that implements a basic
data acquisition loop and a Read Evaluate Print Loop (REPL) for modifying runtime
behavior. This will be automatically built when the Visual Studio solution is
built. It can also be built using mono via:

```
$ cd clroni-repl
$ make
```

## License
[MIT](https://en.wikipedia.org/wiki/MIT_License)
