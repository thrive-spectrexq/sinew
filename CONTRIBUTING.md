# Contributing to Sinew

Thank you for your interest in contributing to **Sinew**!

Sinew is a zero-copy, zero-allocation wire format and messaging library designed specifically for sensor telemetry and robotics control loops.

---

## Core Architectural Principles

Any contributions to the core library MUST adhere strictly to these principles:

1. **Zero Heap Allocation on Hot Path**:
   - No `malloc`, `free`, `new` (except placement-new), or dynamically resizing heap STL containers (`std::vector`, `std::string`) in encode, decode, or message passing routines.
   - Use bounded containers (`sinew::StaticVector`, `sinew::StaticString`) or fixed compile-time buffers.
2. **Freestanding & Embedded Friendly**:
   - Must compile with `-fno-exceptions` (`/EHs-c-`) and `-fno-rtti` (`/GR-`).
   - Core headers must have no external dependencies beyond the standard C++ library (and remain functional in freestanding environments).
3. **Deterministic & Standard Layout**:
   - Message payloads must be standard layout (`std::is_standard_layout_v`) and trivially copyable (`std::is_trivially_copyable_v`).
   - Pointers cannot be serialized across wire/shared-memory boundaries.
4. **Clean Warning Hygiene**:
   - Code must compile with zero warnings under `-Wall -Wextra -Wpedantic` on GCC/Clang and `/W4 /permissive-` on MSVC.
   - Do not add compile options to the `sinew` INTERFACE target that propagate to downstream consumers.

---

## Development & Build Instructions

### Prerequisites
- CMake 3.20+
- C++17 compatible compiler:
  - MSVC 2019+ (Windows)
  - GCC 9+ (Linux)
  - Clang 10+ (Linux/macOS)

### Building the Project
```bash
# Configure
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSINEW_BUILD_TESTS=ON \
  -DSINEW_BUILD_EXAMPLES=ON \
  -DSINEW_BUILD_BENCHMARKS=ON

# Build
cmake --build build --config Debug
```

### Running Tests
```bash
ctest --test-dir build -C Debug --output-on-failure
```

### Running Benchmarks
```bash
# Build in Release mode for accurate benchmarks
cmake --build build --config Release
./build/benchmarks/Release/bench_wire
./build/benchmarks/Release/bench_comparison
```

---

## Pull Request Guidelines

1. **Keep Changes Focused**: Address one issue or feature per pull request.
2. **Add Unit Tests**: Every bugfix or new feature must include automated tests in `tests/`.
3. **Maintain Memory Safety**: Verify changes with AddressSanitizer or Valgrind when modifying raw memory buffers or alignment routines.
4. **Conventional Commits**: Use descriptive commit messages (e.g. `feat: ...`, `fix: ...`, `docs: ...`, `test: ...`).
