# Telemetry Link (C++20)

A transport-independent binary framing library for serial/USB telemetry. It encodes bounded payloads, verifies CRCs, recovers at frame boundaries after corrupted input, and tracks sequence gaps. No device driver or real hardware is required for the examples.

## Build and run

Requires CMake 3.20+ and a C++20 compiler. No external libraries.

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Visual Studio multi-configuration builds:

```powershell
.\build\Release\telemetry-demo.exe write frames.bin
.\build\Release\telemetry-demo.exe read frames.bin
```

On single-configuration Linux builds, executables are directly under `build/`.
`write` intentionally replaces the requested output file. `read` returns nonzero for corrupted, oversized, or truncated input. The synthetic example produces five frames, fed back through 17-byte chunks.

## Wire format

Raw bytes: version `u8` (1), application type `u8`, payload length `u16`, sequence `u32`, payload (0..1024 bytes), CRC-32/ISO-HDLC `u32`. All integers are little-endian. CRC covers header and payload. COBS encodes the complete raw frame, followed by a single zero delimiter. The decoded header is exactly 8 bytes. CRC polynomial is reflected `0xEDB88320`, initial/final XOR `0xFFFFFFFF`.

```cpp
telemetry::StreamDecoder decoder;
auto packet = telemetry::encode({1, 42, {0, 17, 255}});
for (const auto& frame : decoder.feed(packet)) {
    // Consume a verified frame; payload ownership belongs to the returned Frame.
}
```

`decode` accepts the COBS packet **without** the zero delimiter; `encode` includes it. Stream decoder storage is bounded at 1041 bytes. Oversized packets are discarded until the next delimiter. Empty delimiters are ignored. Accepted frames returned by one `feed` call can grow with that call's input length; pass bounded chunks in memory-constrained integrations.

Sequence tracking uses modulo-2^32 arithmetic. Repeated values are duplicates; forward distances below 2^31 indicate progress; other distances are stale. There is no automatic reboot/session detection: reset the tracker when the device session changes.

## Tests and limitations

Ten separately registered tests include a published CRC check vector, fixed COBS vectors, all split boundaries, truncation, malformed lengths, corruption, oversize recovery, wraparound, and 3,000 deterministic randomized round trips plus malformed-stream recovery checks. Test checks remain active in Release builds.

This is a protocol library and file replay demonstration, not STM32 firmware, a serial-port driver, authenticated encryption, a safety-certified protocol, or evidence of measured hardware reliability. `StreamDecoder` and `SequenceTracker` are not thread-safe. CRC detects accidental corruption, not malicious modification. Real-device integration, MCU memory/CPU measurements, and sanitizer runs must be reported separately.

All builds/tests run locally. No GitHub Actions, cloud services, or network dependencies at runtime.

## Design references

- COBS paper by Stuart Cheshire and Mary Baker: https://www.stuartcheshire.org/papers/COBSforToN.pdf
- Serial number arithmetic: https://www.rfc-editor.org/rfc/rfc1982

MIT licensed. See `VALIDATION.md` for measurements from the tested build.
