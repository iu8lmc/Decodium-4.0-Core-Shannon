# Third-party notices

## SGP4 implementation

`src/services/satellite/sgp4/SGP4.cpp` and `SGP4.h` contain the C++ SGP4
implementation derived from the reference implementation by David Vallado,
distributed with the accompanying source comments and attribution. Decodium
keeps the implementation isolated in the `decodium_sgp4` namespace and uses it
for TLE parsing and orbital propagation.

Reference material:

- https://celestrak.org/software/vallado-sw.php
- https://github.com/rirze/sgp4-cpp
