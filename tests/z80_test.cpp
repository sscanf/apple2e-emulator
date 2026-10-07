// Runs a CP/M .COM program (e.g. ZEXDOC/ZEXALL) on the Z80 core, with just
// enough BDOS (console output) to report results.
// https://github.com/anotherlin/z80emu/tree/master/testfiles
//
// usage: z80_test <program.com>
// Exit code 0 if the output contains no "ERROR".

#include "z80.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <string>

namespace {

class FlatBus : public apple2e::Bus {
public:
    std::array<uint8_t, 0x10000> mem{};
    uint8_t read(uint16_t addr) override { return mem[addr]; }
    void write(uint16_t addr, uint8_t val) override { mem[addr] = val; }
};

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <program.com>\n", argv[0]);
        return 2;
    }

    FlatBus bus;
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    file.read(reinterpret_cast<char*>(&bus.mem[0x100]), 0x10000 - 0x100);

    // Warm boot at 0 ends the program; BDOS entry at 5 (JP to the top of
    // memory, which the programs use to place their stack)
    bus.mem[5] = 0xC3;
    bus.mem[6] = 0x00;
    bus.mem[7] = 0xF0;

    apple2e::Z80 cpu(bus);
    cpu.setPC(0x100);

    std::string output;
    uint64_t tstates = 0;
    for (;;) {
        if (cpu.pc() == 0x0000) break;
        if (cpu.pc() == 0x0005) {
            if (cpu.c() == 2) {
                output += static_cast<char>(cpu.e());
                std::putchar(cpu.e());
            } else if (cpu.c() == 9) {
                for (uint16_t a = cpu.de(); bus.mem[a] != '$'; a++) {
                    output += static_cast<char>(bus.mem[a]);
                    std::putchar(bus.mem[a]);
                }
            }
            std::fflush(stdout);
            cpu.ret();
            continue;
        }
        tstates += cpu.step();
    }

    std::printf("\n%llu T-states\n", static_cast<unsigned long long>(tstates));
    return output.find("ERROR") == std::string::npos ? 0 : 1;
}
