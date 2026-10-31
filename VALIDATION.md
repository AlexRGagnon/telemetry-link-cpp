# Local verification

Recorded UTC: 2026-10-31T01:00:01.748035+00:00

Environment: Windows x64, Visual Studio 2022 Build Tools, MSVC 19.44.35214.0, Windows SDK 10.0.26100.0, C++20, x64 Release.

## Results

- CMake configure and Release build: passed. Library compiled with `/W4 /WX`.
- CTest: **10/10** individually registered tests passed: crc, cobs, roundtrip, limits, errors, stream, oversize, recovery, sequence, fuzz.
- The fuzz case ran 3,000 seeded round trips plus malformed-stream recovery checks.
- File demo: wrote five synthetic frames; replay accepted all five, with zero rejected/oversized frames and zero trailing bytes.

## Reproduce

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\telemetry-demo.exe write frames.bin
.\build\Release\telemetry-demo.exe read frames.bin
```

The build tools' CMake directory may need to be added to PATH. All validation was local. No real serial device, MCU deployment, Linux compiler, sanitizer run, or throughput benchmark was validated in this release. This is a host-side protocol library, not hardware validation.
