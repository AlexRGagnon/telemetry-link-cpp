#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace telemetry {
using Bytes = std::vector<std::uint8_t>;
inline constexpr std::size_t max_payload = 1024;
inline constexpr std::size_t header_size = 8;
inline constexpr std::size_t max_raw = header_size + max_payload + 4;
inline constexpr std::size_t max_encoded = max_raw + max_raw / 254 + 1;
struct Frame {
    std::uint8_t type{};
    std::uint32_t sequence{};
    Bytes payload;
    bool operator==(const Frame&) const = default;
};
class ProtocolError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept;
Bytes cobs_encode(std::span<const std::uint8_t> bytes);
Bytes cobs_decode(std::span<const std::uint8_t> bytes);
// encode() includes the trailing zero delimiter; decode() takes its absence.
Bytes encode(const Frame& frame);
Frame decode(std::span<const std::uint8_t> packet);
struct DecoderStats { std::uint64_t accepted{}, rejected{}, oversized{}; };
class StreamDecoder {
    Bytes buffer_;
    bool dropping_ = false;
    DecoderStats stats_;
public:
    // Output size is proportional to the caller's input chunk. Buffered state is bounded.
    std::vector<Frame> feed(std::span<const std::uint8_t> chunk);
    const DecoderStats& stats() const noexcept { return stats_; }
    std::size_t buffered() const noexcept { return buffer_.size(); }
    void reset() noexcept { buffer_.clear(); dropping_ = false; stats_ = {}; }
};
enum class SequenceState { first, in_order, gap, duplicate, stale };
struct SequenceResult { SequenceState state; std::uint32_t missing{}; };
class SequenceTracker {
    std::optional<std::uint32_t> previous_;
public:
    // Serial arithmetic: exactly half-range distance is considered stale.
    SequenceResult observe(std::uint32_t sequence) noexcept;
    void reset() noexcept { previous_.reset(); }
};
} // namespace telemetry
