`liboni` is the official [Open Ephys](https://open-ephys.org/) 
[ONI](https://github.com/jonnew/ONI)-compliant API implementation
for controlling ONI-compliant hardware. It is focused on performance in terms of
bandwidth and closed-loop reaction times and includes means for balancing these
characteristics on-the-fly. When used in combination with 
[ONIX Hardware](https://github.com/jonnew/ONIX), it can be use to acquire from

- Tetrode headstages
- Miniscopes
- Neuropixels
- Etc.

and provide sub-millisecond round-trip communication from brain, through host PC's
main memory, and back again. This repository contains the following folders:

- **[api](./api/README)** liboni API and language bindings. MIT-licensed.
- **[drivers](./drivers/README)** device drivers used by the API at runtime.
  License depends on driver.
  
__Citing this work__: 

1. Citing the paper

  - TODO

2. Citing the repository itself

  - TODO
