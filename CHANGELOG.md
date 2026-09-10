# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Automated Non-Zero Message IDs**: Integrated nv1a16 compile-time string hashing in include/sinew/message.hpp and include/sinew/sinew.hpp. Messages created with SINEW_MESSAGE(Name, ...) now produce unique, deterministic non-zero IDs by default.
- **Explicit ID Definition Macro**: Added SINEW_MESSAGE_ID(Name, MsgId, ...) alongside SINEW_MESSAGE_VERSIONED.
- **Typed Error Decoding**: Added ViewResult<T> and iew_result<T>() in include/sinew/wire.hpp providing expressive error propagation and pointer ergonomics (ok(), has_value(), operator*, operator->).
- **Exclusive Shared Memory Creation**: Added SharedMemoryRegion::create_exclusive() in include/sinew/shm.hpp with O_EXCL guards on POSIX systems to prevent inadvertent truncations of active segments.
- **Determinism & Hardening Tests**: Added comprehensive tests in 	ests/test_wire.cpp verifying message ID uniqueness, prevention of type-confusion across equal-sized structs, zeroed padding determinism, and ViewResult error checking.
- **Scaffolding**: Added CONTRIBUTING.md and .github/CODEOWNERS.
- **CI Sanitizers**: Added Ubuntu GCC 12 Debug matrix job with AddressSanitizer and UndefinedBehaviorSanitizer (-fsanitize=address,undefined) to .github/workflows/ci.yml.

### Changed
- **Zeroed Padding in prepare<T>()**: prepare<T>() now executes std::memset across the payload memory before placement-new, eliminating non-deterministic alignment padding bytes and ensuring reproducible CRC32 checksums.
- **Strict ID Validation**: Removed the wildcard ID check (xpected_id != 0) from alidate<T>() and portable_decode(), enforcing exact message ID matching across all decodes.
- **CMake Warning Isolation**: Warning flags (-Wall -Wextra -Wpedantic /W4) were extracted into an internal sinew_internal_warnings target, preventing flag leakage to downstream consumers of sinew::sinew.
- **Test Harness Warnings**: Fixed while (0,0) to while (0) in 	ests/test_harness.hpp, removing -Wunused-value warnings.
- **Benchmark Transparency**: Clarified that enchmarks/bench_comparison.cpp measures in-repo architectural simulations for FlatBuffers/Protobuf layout patterns.

### Removed
- Removed duplicate macro SINEW_MESSAGE_STRUCT in include/sinew/message.hpp.
