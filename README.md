# Sinew

**A zero-copy, zero-allocation wire format for sensor telemetry and robotics message passing.**

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17%2F20-00599C.svg)](https://en.cppreference.com/)
[![Status](https://img.shields.io/badge/status-early%20design-orange.svg)]()
[![PRs welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)

Sinew is a C++ library for encoding and decoding high-frequency, structured data — IMU samples, LiDAR scans, joint states, actuator commands — with no allocation, no schema compiler, and no copy between the wire and your application's memory. It is not a general-purpose serialization format. It is built for the specific case where a producer and consumer already agree on message shape and need to move data between them as fast as physically possible: over shared memory, a local socket, or a real-time bus.

---

## Why Sinew exists

Most serialization formats are built to solve a harder problem than the one most robotics and embedded systems actually have. Protobuf, FlatBuffers, and Cap'n Proto are designed for cross-service compatibility, schema evolution across years, and safe use between parties that don't control each other's code. That generality has a cost: a compile step, a runtime library, indirection on read, and — for most of them — at least one allocation or copy somewhere in the path.

Inside a robot or an embedded telemetry pipeline, that problem usually doesn't exist. The producer and consumer are compiled from the same repository, deployed together, and versioned together. What's actually needed is:

- A wire layout that **is** the in-memory layout, so decoding is a pointer cast, not a parse
- No heap allocation on the hot path — every message fits in a fixed-size buffer known at compile time
- Deterministic, bounded encode/decode time — suitable for hard real-time loops
- A small enough footprint to run on a Cortex-M class microcontroller
- Enough structure (a header, a version tag, an optional checksum) to catch mismatches without a full schema negotiation protocol

Sinew is built narrowly around that problem. If you need cross-language interop, long-term wire compatibility, or safe communication between mutually distrusting services, use Protobuf, Cap'n Proto, or DDS instead — they solve that problem well, and Sinew intentionally doesn't try to.

## Key features

- **Zero-copy reads** — a received buffer is reinterpreted in place; there is no decode step for same-architecture consumers
- **Zero-allocation encode/decode** — all message layouts are fixed-size and known at compile time; no `new`, no `malloc`, no STL containers in the hot path
- **No schema compiler** — messages are plain C++ structs annotated with a lightweight macro or C++20 concept; there is no `.proto`-style IDL and no code-generation step to wire into your build
- **Deterministic timing** — encode and decode are O(1) memory operations, making Sinew suitable for hard real-time control loops
- **Small footprint** — the core library has no dependencies and is small enough to target MCUs, not just Linux-class hardware
- **Shared-memory friendly** — messages can be written directly into a shared-memory ring buffer and read by another process with no serialization step at all
- **Portable mode (opt-in)** — for the cases where producer and consumer *don't* share architecture/endianness, a portable encoding path normalizes byte order at the boundary, at the cost of a copy

## How it works

A Sinew message is a plain C++ struct with a fixed memory layout, wrapped by a small fixed header:

```
┌─────────────┬─────────────┬──────────────┬───────────┬──────────────────┐
│ magic (4B)  │ msg_id (2B) │ version (2B) │ crc (4B)  │ payload (fixed)  │
└─────────────┴─────────────┴──────────────┴───────────┴──────────────────┘
```

On a same-architecture, same-ABI transport (shared memory, Unix domain socket, a real-time bus within one embedded system), the payload bytes on the wire are bit-for-bit identical to the struct in memory. Reading a message is:

```cpp
const auto* msg = sinew::view<ImuSample>(buffer);
// msg->accel_x, msg->gyro_z, msg->timestamp_ns — no parsing, just a cast
```

Writing one is the inverse — construct the struct in place inside the send buffer, no intermediate representation:

```cpp
auto* out = sinew::prepare<ImuSample>(send_buffer);
out->accel_x = ax;
out->gyro_z  = gz;
out->timestamp_ns = now_ns();
sinew::finalize(out); // stamps header, version, and checksum
```

Struct layout and alignment are pinned at compile time (`static_assert`-checked, no implicit padding surprises), so what you write is exactly what's read on the other side.

## Comparison

| | **Sinew** | FlatBuffers | Cap'n Proto | Protobuf | MCAP |
|---|---|---|---|---|---|
| Zero-copy read | ✅ | ✅ | ✅ | ❌ | N/A (container format) |
| Zero-copy write | ✅ | ❌ (builder) | ⚠️ (arena) | ❌ | N/A |
| Schema compiler required | ❌ | ✅ | ✅ | ✅ | N/A |
| Heap allocation on hot path | ❌ | ⚠️ (builder) | ⚠️ (arena) | ✅ | N/A |
| Cross-language codegen | ❌ (C++ only) | ✅ | ✅ | ✅ | ✅ (via nested formats) |
| MCU-friendly footprint | ✅ | ⚠️ | ❌ | ❌ | N/A |
| Best for | tightly-coupled producer/consumer in one codebase | cross-language APIs | RPC between services | cross-service APIs, long-term compatibility | logging/recording robotics data to disk |

MCAP solves a different problem entirely (a container format for *recording* timestamped messages to disk, regardless of encoding) and is a natural companion to Sinew rather than a competitor — you could log Sinew-encoded messages into an MCAP file.

## Getting started

Sinew is header-only, so integrating it is a single include path:

```cmake
# CMakeLists.txt
add_subdirectory(third_party/sinew)
target_link_libraries(your_target PRIVATE sinew::sinew)
```

```cpp
#include <sinew/sinew.hpp>

SINEW_MESSAGE(ImuSample,
    (double, timestamp_ns)
    (float,  accel_x)
    (float,  accel_y)
    (float,  accel_z)
    (float,  gyro_x)
    (float,  gyro_y)
    (float,  gyro_z)
);
```

Requires C++17 at minimum; the reflection layer is cleaner under C++20 and that path will be the default once compiler support is broadly available on target embedded toolchains.

## Project status

This project is in **early design** — the layout format and macro API above describe the intended design, not yet a finished implementation. Nothing here is stable or benchmarked yet. If you're evaluating Sinew for a real system today, don't — check back once there's a tagged release.

Planned early milestones:

- [x] Core header layout + compile-time struct reflection macro
- [x] In-process lock-free SPSC ring buffer transport
- [x] Cross-process IPC shared-memory ring buffer (`sinew::IpcRingBuffer` for Windows & POSIX)
- [x] Zero-allocation bounded containers (`StaticVector`, `StaticString`)
- [x] Heterogeneous message stream inspect and visitor dispatch (`sinew::dispatch`)
- [x] Modern CMake package export and multi-platform CI
- [ ] Portable (endianness-normalized) encode path
- [ ] Benchmark suite vs FlatBuffers/Cap'n Proto/SBE on representative telemetry payloads
- [ ] MCU target validation (Cortex-M4/M7)
- [ ] Optional ROS 2 bridge for interop with existing robotics stacks

## Design non-goals

To keep scope honest: Sinew does **not** aim to provide cross-language bindings, long-horizon wire-format evolution guarantees, RPC semantics, or a transport layer of its own. It pairs with whatever transport you already use — shared memory, UDP, a serial link, ZeroMQ — and focuses only on making the encode/decode step disappear.

## Contributing

Issues and PRs are welcome, especially around the core layout design while it's still open for discussion. See `CONTRIBUTING.md` for build instructions and coding conventions once the initial scaffold lands.

## License

MIT — see [LICENSE](LICENSE).
