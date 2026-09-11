# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Automated Non-Zero Message IDs**: Integrated fnv1a16 compile-time string hashing in include/sinew/message.hpp and include/sinew/sinew.hpp. Messages created with SINEW_MESSAGE(Name, ...) now produce unique, deterministic non-zero IDs by default.
- **Explicit ID Definition Macro**: Added SINEW_MESSAGE_ID(Name, MsgId, ...) alongside SINEW_MESSAGE_VERSIONED.
- **Typed Error Decoding**: Added ViewResult<T> and view_result<T>() in include/sinew/wire.hpp providing expressive error propagation and pointer ergonomics (ok(), has_value(), operator*, operator->).
- **Exclusive Shared Memory Creation**: Added SharedMemoryRegion::create_exclusive() in include/sinew/shm.hpp with O_EXCL guards on POSIX systems to prevent inadvertent truncations of active segments.
- **Determinism & Hardening Tests**: Added comprehensive tests in tests/test_wire.cpp verifying message ID uniqueness, prevention of type-confusion across equal-sized structs, zeroed padding determinism, and ViewResult error checking.
- **Scaffolding**: Added CONTRIBUTING.md and .github/CODEOWNERS.
- **PacketView Direct Dispatch**: Added `dispatch<MsgTypes...>(const PacketView&, Visitor&&, bool)` in `include/sinew/dispatch.hpp` allowing pattern matching directly from pre-inspected wire packets without pointer offset recalculation.
- **Ring Buffer Consumer Callbacks**: Added `consume(Func&&)` method to `SpscRingBuffer` and `IpcRingBuffer`, allowing in-place processing with zero copy and automatic commit.
- **StaticContainer Bounds Checking & Resizing**: Added `at(index)` and `resize(size, val)` to `StaticVector` and `at(index)` to `StaticString` with conditional exception support (`SINEW_NO_EXCEPTIONS`).
- **Write-Side Typed Error Reporting (`PrepareResult<T>` & `prepare_result<T>()`)**: Added `PrepareResult<T>` and `prepare_result<T>(buffer, capacity)` to `include/sinew/wire.hpp` providing structured error feedback (`ErrorCode::BufferTooSmall`, `ErrorCode::Misaligned`) during in-place serialization.
- **Freestanding Debug Traps & Annotations**: Added debug assertions (`assert(index < size_)`) and safety documentation to `StaticVector::at()` and `StaticString::at()` for `-fno-exceptions` embedded MCU targets.
- **StaticString STL Iterators & Element Accessors**: Added `begin()`, `end()`, `cbegin()`, `cend()`, `front()`, and `back()` to `StaticString` enabling standard range-based `for` loops, algorithms, and container uniformity.
- **IPC Envelope Typed Consumer Callback**: Added `consume<Msg>(Func&&, bool)` to `IpcEnvelopeRingBuffer` for safe single-type message consumption with automatic cursor advancement.
- **Freestanding IOStream Guarding**: Guarded `operator<<` overloads with `#if !defined(SINEW_NO_IOSTREAMS)` across headers and configured the embedded `mcu_freestanding` build with zero exception unwinding warnings.

### Changed
- **Loud Compile Failure for Unspecialized ROS 2 Converters**: Replaced generic fallback assignment in `sinew::ros2::Ros2Converter` with `static_assert(sizeof(SinewMsg) == 0)` and deleted methods to trigger clear, immediate compile-time errors when a specialization is omitted.
- **POSIX Shared Memory Name Clamping**: Enforced POSIX/macOS compliance in `SharedMemoryRegion` by normalizing names with a leading slash and clamping to 31 bytes (macOS `PSHMNAMELENGTH` limit), eliminating `ENAMETOOLONG` errors.
- **Zeroed Padding in prepare<T>()**: prepare<T>() now executes std::memset across the payload memory before placement-new, eliminating non-deterministic alignment padding bytes and ensuring reproducible CRC32 checksums.
- **Strict ID Validation**: Removed the wildcard ID check (expected_id != 0) from validate<T>() and portable_decode(), enforcing exact message ID matching across all decodes.
- **CMake Warning Isolation**: Warning flags (-Wall -Wextra -Wpedantic /W4) were extracted into an internal sinew_internal_warnings target, preventing flag leakage to downstream consumers of sinew::sinew.
- **Test Harness Warnings**: Fixed while (0,0) to while (0) in tests/test_harness.hpp, removing -Wunused-value warnings.
- **Benchmark Transparency**: Clarified that benchmarks/bench_comparison.cpp measures in-repo architectural simulations for FlatBuffers/Protobuf layout patterns.

### Removed
- Removed duplicate macro SINEW_MESSAGE_STRUCT in include/sinew/message.hpp.
