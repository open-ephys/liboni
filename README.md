`liboni` is the [Open Ephys](https://open-ephys.org/)
[ONI](https://github.com/jonnew/ONI)-compliant API implementation
for use with ONI-compliant hardware. It is focused on performance in terms of
bandwidth and closed-loop reaction times and includes means for balancing these
characteristics on-the-fly. When used in combination with
[ONIX Hardware](https://open-ephys.github.io/onix-docs/index.html), it can be 
used to acquire from:

- Tetrode headstages
- Miniscopes
- Neuropixels
- Etc.

and provide sub-millisecond round-trip communication from brain, through host PC's
main memory, and back again. This repository contains the following folders:

- **[api](api/README.md)** liboni API and language bindings. MIT-licensed.
- **[drivers](drivers/README.md)** device drivers used by the API at runtime.
  License depends on driver.

