// Runs Klaus Dormann's 6502/65C02 functional test binaries against the CPU core.
// https://github.com/Klaus2m5/6502_65C02_functional_tests
//
// usage: cpu_test <image.bin> <success-address-hex>
// The image is a 64 KB memory dump started at $0400. The test traps (jumps to
// itself) on failure; reaching the success address means every test passed.

#include "cpu.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace {

class FlatBus : public apple2e::Bus {
public:
    std::array<uint8_t, 0x10000> mem{};
    uint8_t read(uint16_t addr) override { return mem[addr]; }
    void write(uint16_t addr, uint8_t val) override { mem[addr] = val; }
};

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <image.bin> <success-address-hex>\n", argv[0]);
        return 2;
    }

    FlatBus bus;
    std::ifstream file(argv[1], std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(bus.mem.data()), bus.mem.size())) {
        std::fprintf(stderr, "cannot read 64 KB image %s\n", argv[1]);
        return 2;
    }
    uint16_t success = static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 16));

    // The published 65C02 image also exercises the Rockwell bit instructions
    apple2e::CPU cpu(bus, apple2e::CPU::Variant::Rockwell65C02);
    cpu.setPC(0x0400);

    uint64_t cycles = 0;
    for (;;) {
        uint16_t pc = cpu.pc();
        cycles += cpu.step();
        if (cpu.pc() == pc) break;  // trapped
    }

    bool ok = cpu.pc() == success;
    std::printf("%s: trapped at $%04X after %llu cycles (A=%02X X=%02X Y=%02X P=%02X)\n",
                ok ? "PASS" : "FAIL", cpu.pc(), static_cast<unsigned long long>(cycles),
                cpu.a(), cpu.x(), cpu.y(), cpu.p());
    return ok ? 0 : 1;
}
