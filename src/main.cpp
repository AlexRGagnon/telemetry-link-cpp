#include "telemetry/protocol.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc != 3 || (std::string(argv[1]) != "write" && std::string(argv[1]) != "read")) {
            std::cerr << "Usage: telemetry-demo write|read frames.bin\n"; return 2;
        }
        if (std::string(argv[1]) == "write") {
            std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("cannot open output");
            for (std::uint32_t i = 0; i < 5; ++i) {
                const auto bytes = telemetry::encode({1, i, {0, 42, static_cast<std::uint8_t>(i)}});
                out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            }
            if (!out) throw std::runtime_error("write failed");
            std::cout << "Wrote five synthetic telemetry frames\n";
        } else {
            std::ifstream in(argv[2], std::ios::binary);
            if (!in) throw std::runtime_error("cannot open input");
            telemetry::StreamDecoder decoder;
            char block[17];
            while (in.read(block, sizeof(block)) || in.gcount()) {
                const auto data = std::span(reinterpret_cast<const std::uint8_t*>(block), static_cast<std::size_t>(in.gcount()));
                for (const auto& f : decoder.feed(data))
                    std::cout << "type=" << unsigned(f.type) << " sequence=" << f.sequence << " payload_bytes=" << f.payload.size() << '\n';
            }
            if (in.bad()) throw std::runtime_error("read failed");
            std::cout << "accepted=" << decoder.stats().accepted << " rejected=" << decoder.stats().rejected
                      << " oversized=" << decoder.stats().oversized << " trailing_bytes=" << decoder.buffered() << '\n';
            if (decoder.stats().rejected || decoder.stats().oversized || decoder.buffered()) return 1;
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
