#include "telemetry/protocol.hpp"
#include <algorithm>
#include <limits>

namespace telemetry {
namespace {
void put16(Bytes& b, std::uint16_t n) {
    b.push_back(static_cast<std::uint8_t>(n));
    b.push_back(static_cast<std::uint8_t>(n >> 8));
}
void put32(Bytes& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
}
std::uint32_t get32(std::span<const std::uint8_t> b, std::size_t p) {
    std::uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i) n |= static_cast<std::uint32_t>(b[p + i]) << (8 * i);
    return n;
}
}
std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc = 0xffffffffU;
    for (auto byte : bytes) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return crc ^ 0xffffffffU;
}
Bytes cobs_encode(std::span<const std::uint8_t> bytes) {
    Bytes out(1, 0);
    std::size_t code_index = 0;
    std::uint8_t code = 1;
    for (auto byte : bytes) {
        if (byte == 0) {
            out[code_index] = code;
            code_index = out.size(); out.push_back(0); code = 1;
        } else {
            out.push_back(byte);
            ++code;
            if (code == 0xff) {
                out[code_index] = code;
                code_index = out.size(); out.push_back(0); code = 1;
            }
        }
    }
    out[code_index] = code;
    return out;
}
Bytes cobs_decode(std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) throw ProtocolError("empty COBS packet");
    Bytes out;
    std::size_t index = 0;
    while (index < bytes.size()) {
        const auto code = bytes[index++];
        if (code == 0 || static_cast<std::size_t>(code - 1) > bytes.size() - index)
            throw ProtocolError("invalid COBS run");
        for (unsigned i = 1; i < code; ++i) {
            if (bytes[index] == 0) throw ProtocolError("zero inside COBS run");
            out.push_back(bytes[index++]);
        }
        if (code != 0xff && index < bytes.size()) out.push_back(0);
    }
    return out;
}
Bytes encode(const Frame& frame) {
    if (frame.payload.size() > max_payload) throw ProtocolError("payload exceeds 1024 bytes");
    Bytes raw{1, frame.type};
    put16(raw, static_cast<std::uint16_t>(frame.payload.size()));
    put32(raw, frame.sequence);
    raw.insert(raw.end(), frame.payload.begin(), frame.payload.end());
    const auto crc = crc32(raw);
    put32(raw, crc);
    auto packet = cobs_encode(raw);
    packet.push_back(0);
    return packet;
}
Frame decode(std::span<const std::uint8_t> packet) {
    if (packet.size() > max_encoded) throw ProtocolError("encoded packet too large");
    const auto raw = cobs_decode(packet);
    if (raw.size() < header_size + 4 || raw[0] != 1) throw ProtocolError("bad header/version");
    const auto n = static_cast<std::size_t>(raw[2]) | (static_cast<std::size_t>(raw[3]) << 8);
    if (n > max_payload || raw.size() != header_size + n + 4) throw ProtocolError("length mismatch");
    if (crc32(std::span(raw).first(raw.size() - 4)) != get32(raw, raw.size() - 4))
        throw ProtocolError("CRC mismatch");
    return Frame{raw[1], get32(raw, 4), Bytes(raw.begin() + header_size, raw.end() - 4)};
}
std::vector<Frame> StreamDecoder::feed(std::span<const std::uint8_t> chunk) {
    std::vector<Frame> frames;
    for (const auto byte : chunk) {
        if (byte == 0) {
            if (!dropping_ && !buffer_.empty()) {
                try { frames.push_back(decode(buffer_)); ++stats_.accepted; }
                catch (const ProtocolError&) { ++stats_.rejected; }
            }
            buffer_.clear(); dropping_ = false;
        } else if (!dropping_) {
            if (buffer_.size() >= max_encoded) {
                buffer_.clear(); dropping_ = true; ++stats_.oversized;
            } else { buffer_.push_back(byte); }
        }
    }
    return frames;
}
SequenceResult SequenceTracker::observe(std::uint32_t sequence) noexcept {
    if (!previous_) { previous_ = sequence; return {SequenceState::first, 0}; }
    const auto delta = sequence - *previous_;
    if (delta == 0) return {SequenceState::duplicate, 0};
    if (delta >= 0x80000000U) return {SequenceState::stale, 0};
    previous_ = sequence;
    return delta == 1 ? SequenceResult{SequenceState::in_order, 0}
                      : SequenceResult{SequenceState::gap, delta - 1};
}
} // namespace telemetry
