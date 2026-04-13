# Basics — RANCS Lab

Foundational networking and binary data skills for autonomous vehicle systems research.

## Topics covered

- **UDP & TCP sockets** — Python and C++ server/client implementations
- **Bitwise operations** — `&`, `|`, `^`, `~`, `>>`, `<<` in Python and C++
- **Struct packing/unpacking** — converting between raw bytes and floats/ints
- **Endianness** — big-endian vs little-endian, `htonl`/`ntohl` in C++, `struct` format prefixes in Python

## Why this matters

Sensors (LiDAR, radar, IMU) send raw binary data over UDP/TCP.
Reading that data correctly requires knowing how bytes are ordered and how to extract packed bit fields.
