#include "telemetry/protocol.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
using namespace telemetry;
void check(bool ok) { if (!ok) throw std::runtime_error("check failed"); }
template<class F> void rejects(F&& f) {
    bool threw = false;
    try { f(); } catch (const ProtocolError&) { threw = true; }
    check(threw);
}
Frame unpack(Bytes b) { b.pop_back(); return decode(b); }
const Frame sample{7, 12345, {0, 1, 255, 0, 42}};
int main(int argc, char** argv) {
    const std::map<std::string, std::function<void()>> cases{
        {"crc", [] { const std::string s = "123456789";
            check(crc32(std::span(reinterpret_cast<const std::uint8_t*>(s.data()), s.size())) == 0xcbf43926U);
            check(crc32({}) == 0); }},
        {"cobs", [] {
            check(cobs_encode(Bytes{}) == Bytes{1});
            check(cobs_encode(Bytes{0}) == (Bytes{1, 1}));
            check(cobs_encode(Bytes{1, 0, 2}) == (Bytes{2, 1, 2, 2}));
            for (auto n : {0, 1, 253, 254, 255, 508, 1024}) {
                Bytes b(static_cast<std::size_t>(n), 7);
                check(cobs_decode(cobs_encode(b)) == b);
            }
        }},
        {"roundtrip", [] { check(unpack(encode(sample)) == sample);
            check(unpack(encode({0, 0xffffffffU, {}})) == (Frame{0, 0xffffffffU, {}})); }},
        {"limits", [] { Frame f{255, 0, Bytes(max_payload, 0)};
            check(unpack(encode(f)) == f); f.payload.push_back(0);
            rejects([&] { encode(f); }); rejects([] { decode(Bytes(max_encoded + 1, 1)); }); }},
        {"errors", [] {
            rejects([] { cobs_decode({}); }); rejects([] { cobs_decode(Bytes{0}); });
            rejects([] { cobs_decode(Bytes{3, 1}); }); rejects([] { cobs_decode(Bytes{2, 0}); });
            auto p = encode(sample); p.pop_back(); auto raw = cobs_decode(p);
            raw[8] ^= 1; rejects([&] { decode(cobs_encode(raw)); });
            raw[0] = 2; rejects([&] { decode(cobs_encode(raw)); });
            raw[0] = 1; raw[2] = 255; rejects([&] { decode(cobs_encode(raw)); });
            rejects([] { decode(cobs_encode(Bytes{1, 2})); });
        }},
        {"stream", [] { const auto p = encode(sample);
            for (std::size_t cut = 0; cut <= p.size(); ++cut) {
                StreamDecoder d; auto a = d.feed(std::span(p).first(cut));
                auto b = d.feed(std::span(p).subspan(cut)); a.insert(a.end(), b.begin(), b.end());
                check(a == std::vector<Frame>{sample}); check(d.buffered() == 0);
            }
        }},
        {"oversize", [] { StreamDecoder d; d.feed(Bytes(max_encoded + 4000, 1));
            check(d.buffered() == 0); check(d.stats().oversized == 1);
            check(d.feed(encode(sample)).empty()); // drop until delimiter, even a valid suffix
            check(d.feed(encode(sample)) == std::vector<Frame>{sample});
            d.reset(); check(d.stats().accepted == 0); }},
        {"recovery", [] { StreamDecoder d; d.feed(Bytes{2, 0, 0});
            check(d.stats().rejected == 1); check(d.feed(encode(sample)) == std::vector<Frame>{sample});
            Bytes both = encode(sample); const auto p = encode(sample); both.insert(both.end(), p.begin(), p.end());
            check(d.feed(both).size() == 2); }},
        {"sequence", [] { SequenceTracker t;
            check(t.observe(0xfffffffeU).state == SequenceState::first);
            check(t.observe(0xffffffffU).state == SequenceState::in_order);
            check(t.observe(0).state == SequenceState::in_order);
            check(t.observe(0).state == SequenceState::duplicate);
            const auto r = t.observe(5); check(r.state == SequenceState::gap && r.missing == 4);
            check(t.observe(4).state == SequenceState::stale);
            check(t.observe(0x80000005U).state == SequenceState::stale);
            check(t.observe(6).state == SequenceState::in_order); t.reset();
            check(t.observe(3).state == SequenceState::first); }},
        {"fuzz", [] { std::mt19937 rng(7171); StreamDecoder d;
            for (int i = 0; i < 3000; ++i) {
                Bytes b(static_cast<std::size_t>(rng() % (max_payload + 1)));
                for (auto& byte : b) byte = static_cast<std::uint8_t>(rng());
                Frame f{static_cast<std::uint8_t>(rng()), static_cast<std::uint32_t>(rng()), b};
                check(unpack(encode(f)) == f);
                for (auto& byte : b) byte = static_cast<std::uint8_t>(rng());
                d.feed(b); check(d.buffered() <= max_encoded);
                d.feed(Bytes{0}); check(d.feed(encode(f)) == std::vector<Frame>{f});
            }
        }}
    };
    try {
        if (argc != 2 || !cases.contains(argv[1])) { std::cerr << "unknown test\n"; return 2; }
        cases.at(argv[1])(); std::cout << "PASS " << argv[1] << '\n'; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
