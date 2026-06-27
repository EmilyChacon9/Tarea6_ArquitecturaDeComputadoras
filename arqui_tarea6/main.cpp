#include <iostream>
#include <vector>
#include <array>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cstdint>

struct DecodedInstruction {
    uint32_t opcode, rd, funct3, rs1, rs2, funct7;
    int32_t imm_I, imm_S, imm_B, imm_J;
    uint32_t imm_U;
};

class Memory {
    std::vector<uint8_t> data;

public:
    Memory(size_t size) : data(size, 0) {
    }

    bool load_program(const std::string &filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        std::streamsize sz = file.tellg();
        if (sz > (std::streamsize) data.size()) sz = data.size();
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char *>(data.data()), sz);
        return true;
    }

    uint8_t read8(uint32_t a) const { return (a < data.size()) ? data[a] : 0; }
    uint16_t read16(uint32_t a) const { return read8(a) | (uint16_t(read8(a + 1)) << 8); }

    uint32_t read32(uint32_t a) const {
        return read8(a) | (uint32_t(read8(a + 1)) << 8) |
               (uint32_t(read8(a + 2)) << 16) | (uint32_t(read8(a + 3)) << 24);
    }

    void write8(uint32_t a, uint8_t v) { if (a < data.size()) data[a] = v; }

    void write16(uint32_t a, uint16_t v) {
        write8(a, v & 0xFF);
        write8(a + 1, v >> 8);
    }

    void write32(uint32_t a, uint32_t v) {
        write8(a, v & 0xFF);
        write8(a + 1, (v >> 8) & 0xFF);
        write8(a + 2, (v >> 16) & 0xFF);
        write8(a + 3, (v >> 24) & 0xFF);
    }

    void dump(uint32_t start, uint32_t end) const {
        std::cout << std::hex << std::uppercase;
        for (uint32_t a = start; a <= end; ++a) {
            std::cout << std::setw(2) << std::setfill('0') << (int) read8(a);
            if (a < end) std::cout << ' ';
        }
        std::cout << std::dec << '\n';
    }
};

class CPU {
    std::array<uint32_t, 32> regs = {};
    uint32_t pc = 0;
    Memory &mem;
    bool halted = false;

    DecodedInstruction decode(uint32_t instr) {
        DecodedInstruction d{};
        d.opcode = instr & 0x7F;
        d.rd = (instr >> 7) & 0x1F;
        d.funct3 = (instr >> 12) & 0x07;
        d.rs1 = (instr >> 15) & 0x1F;
        d.rs2 = (instr >> 20) & 0x1F;
        d.funct7 = (instr >> 25) & 0x7F;

        int32_t s = static_cast<int32_t>(instr);

        // I-type
        d.imm_I = s >> 20;

        // S-type
        d.imm_S = ((s >> 20) & ~0x1F) | ((instr >> 7) & 0x1F);

        // B-type
        d.imm_B = ((s >> 19) & 0xFFFFF000)
                  | ((instr >> 7) & 0x1E)
                  | ((instr << 4) & 0x800)
                  | ((instr >> 20) & 0x7E0);

        // U-type
        d.imm_U = instr & 0xFFFFF000;

        // J-type
        d.imm_J = ((s >> 11) & 0xFFF00000)
                  | (instr & 0xFF000)
                  | ((instr >> 9) & 0x800)
                  | ((instr >> 20) & 0x7FE);

        return d;
    }

    void wr(uint32_t r, uint32_t v) { if (r) regs[r] = v; }
    uint32_t rd(uint32_t r) const { return r ? regs[r] : 0; }

public:
    CPU(Memory &m) : mem(m) {
    }

    uint32_t get_pc() const { return pc; }
    uint32_t get_reg(uint32_t r) const { return (r < 32) ? rd(r) : 0; }
    bool is_halted() const { return halted; }

    void step() {
        if (halted) {
            std::cout << "CPU halted.\n";
            return;
        }

        uint32_t instr = mem.read32(pc);
        auto d = decode(instr);
        bool jumped = false;

        switch (d.opcode) {
            case 0x03: {
                uint32_t addr = rd(d.rs1) + d.imm_I;
                switch (d.funct3) {
                    case 0x0: wr(d.rd, (int32_t) (int8_t) mem.read8(addr));
                        break; // LB
                    case 0x1: wr(d.rd, (int32_t) (int16_t) mem.read16(addr));
                        break; // LH
                    case 0x2: wr(d.rd, mem.read32(addr));
                        break; // LW
                    case 0x4: wr(d.rd, mem.read8(addr));
                        break; // LBU
                    case 0x5: wr(d.rd, mem.read16(addr));
                        break; // LHU
                    default: std::cerr << "Unknown load funct3\n";
                }
                break;
            }

            case 0x13:
                switch (d.funct3) {
                    case 0x0: wr(d.rd, rd(d.rs1) + d.imm_I);
                        break; // ADDI
                    case 0x1: wr(d.rd, rd(d.rs1) << (d.imm_I & 0x1F));
                        break; // SLLI
                    case 0x2: wr(d.rd, (int32_t) rd(d.rs1) < d.imm_I ? 1u : 0u);
                        break; // SLTI
                    case 0x3: wr(d.rd, rd(d.rs1) < (uint32_t) d.imm_I ? 1u : 0u);
                        break; // SLTIU
                    case 0x4: wr(d.rd, rd(d.rs1) ^ d.imm_I);
                        break; // XORI
                    case 0x5:
                        if (d.funct7 == 0x20)
                            wr(d.rd, (int32_t) rd(d.rs1) >> (d.imm_I & 0x1F)); // SRAI
                        else
                            wr(d.rd, rd(d.rs1) >> (d.imm_I & 0x1F)); // SRLI
                        break;
                    case 0x6: wr(d.rd, rd(d.rs1) | d.imm_I);
                        break; // ORI
                    case 0x7: wr(d.rd, rd(d.rs1) & d.imm_I);
                        break; // ANDI
                }
                break;

            case 0x17:
                wr(d.rd, pc + d.imm_U);
                break;

            case 0x23: {
                uint32_t addr = rd(d.rs1) + d.imm_S;
                switch (d.funct3) {
                    case 0x0: mem.write8(addr, rd(d.rs2) & 0xFF);
                        break; // SB
                    case 0x1: mem.write16(addr, rd(d.rs2) & 0xFFFF);
                        break; // SH
                    case 0x2: mem.write32(addr, rd(d.rs2));
                        break; // SW
                    default: std::cerr << "Unknown store funct3\n";
                }
                break;
            }

            case 0x33:
                switch (d.funct3) {
                    case 0x0:
                        wr(d.rd, d.funct7 == 0x20
                                     ? rd(d.rs1) - rd(d.rs2)
                                     : rd(d.rs1) + rd(d.rs2)); // SUB/ADD
                        break;
                    case 0x1: wr(d.rd, rd(d.rs1) << (rd(d.rs2) & 0x1F));
                        break; // SLL
                    case 0x2: wr(d.rd, (int32_t) rd(d.rs1) < (int32_t) rd(d.rs2) ? 1u : 0u);
                        break; // SLT
                    case 0x3: wr(d.rd, rd(d.rs1) < rd(d.rs2) ? 1u : 0u);
                        break; // SLTU
                    case 0x4: wr(d.rd, rd(d.rs1) ^ rd(d.rs2));
                        break; // XOR
                    case 0x5:
                        wr(d.rd, d.funct7 == 0x20
                                     ? (uint32_t) ((int32_t) rd(d.rs1) >> (rd(d.rs2) & 0x1F)) // SRA
                                     : rd(d.rs1) >> (rd(d.rs2) & 0x1F)); // SRL
                        break;
                    case 0x6: wr(d.rd, rd(d.rs1) | rd(d.rs2));
                        break; // OR
                    case 0x7: wr(d.rd, rd(d.rs1) & rd(d.rs2));
                        break; // AND
                }
                break;

            case 0x37:
                wr(d.rd, d.imm_U);
                break;

            case 0x63: {
                bool taken = false;
                switch (d.funct3) {
                    case 0x0: taken = rd(d.rs1) == rd(d.rs2);
                        break; // BEQ
                    case 0x1: taken = rd(d.rs1) != rd(d.rs2);
                        break; // BNE
                    case 0x4: taken = (int32_t) rd(d.rs1) < (int32_t) rd(d.rs2);
                        break; // BLT
                    case 0x5: taken = (int32_t) rd(d.rs1) >= (int32_t) rd(d.rs2);
                        break; // BGE
                    case 0x6: taken = rd(d.rs1) < rd(d.rs2);
                        break; // BLTU
                    case 0x7: taken = rd(d.rs1) >= rd(d.rs2);
                        break; // BGEU
                    default: std::cerr << "Unknown branch funct3\n";
                }
                if (taken) {
                    pc += d.imm_B;
                    jumped = true;
                }
                break;
            }

            case 0x67:
                wr(d.rd, pc + 4);
                pc = (rd(d.rs1) + d.imm_I) & ~1u;
                jumped = true;
                break;

            case 0x6F:
                wr(d.rd, pc + 4);
                pc += d.imm_J;
                jumped = true;
                break;

            default:
                std::cerr << "Unknown opcode 0x" << std::hex << d.opcode
                        << " at PC=0x" << pc << std::dec << '\n';
                halted = true;
                return;
        }

        if (!jumped) pc += 4;
    }
};

static int parse_reg(const std::string &s) {
    if (s.size() >= 2 && s[0] == 'x') {
        try { return std::stoi(s.substr(1)); } catch (...) {
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <program.bin>\n";
        return 1;
    }

    Memory mem(1024 * 1024);
    if (!mem.load_program(argv[1])) {
        std::cerr << "Error: could not open \"" << argv[1] << "\"\n";
        return 1;
    }
    std::cout << '"' << argv[1] << "\" loaded into memory.\n";

    CPU cpu(mem);
    std::string line;
    std::cout << "> ";

    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd.empty()) {
        }
        else if (cmd == "step") {
            cpu.step();
            std::cout << "Executed instruction. PC = 0x"
                    << std::hex << std::setw(8) << std::setfill('0')
                    << cpu.get_pc() << std::dec << '\n';
        }
        else if (cmd == "run") {
            while (!cpu.is_halted()) cpu.step();
        }
        else if (cmd == "pc") {
            std::cout << "pc = 0x"
                    << std::hex << std::setw(8) << std::setfill('0')
                    << cpu.get_pc() << std::dec << '\n';
        }
        else if (cmd == "regs") {
            std::string tok;
            std::vector<int> rs;
            while (ss >> tok) {
                int r = parse_reg(tok);
                if (r >= 0 && r < 32) rs.push_back(r);
                else std::cerr << "Unknown register: " << tok << '\n';
            }
            if (rs.empty()) {
                for (int i = 0; i < 32; ++i) {
                    std::cout << "x" << std::setw(2) << std::setfill('0') << i
                            << " = 0x" << std::hex << std::setw(8) << std::setfill('0')
                            << cpu.get_reg(i) << std::dec;
                    std::cout << ((i % 4 == 3) ? '\n' : '\t');
                }
            } else {
                for (int r: rs) {
                    std::cout << 'x' << r << " = 0x"
                            << std::hex << std::setw(8) << std::setfill('0')
                            << cpu.get_reg(r) << std::dec << '\n';
                }
            }
        }
        else if (cmd == "mem") {
            std::string s1, s2;
            if (ss >> s1 >> s2) {
                uint32_t start = std::stoul(s1, nullptr, 16);
                uint32_t end = std::stoul(s2, nullptr, 16);
                std::cout << "Memory (0x" << std::hex << std::uppercase
                        << start << "-0x" << end << "): ";
                mem.dump(start, end);
                std::cout << std::dec;
            } else {
                std::cerr << "Usage: mem <start_hex> <end_hex>\n";
            }
        }
        else if (cmd == "exit") {
            std::cout << "See you next time...\nProgram exited with code 0.\n";
            break;
        }
        else if (cmd == "help") {
            std::cout <<
                    "Commands:\n"
                    "  step              Execute one instruction\n"
                    "  run               Run until halt\n"
                    "  pc                Print program counter\n"
                    "  regs [x0 x1 ...]  Print all registers or listed ones\n"
                    "  mem <start> <end> Print memory range (hex addresses)\n"
                    "  exit              Quit\n";
        } else {
            std::cerr << "Unknown command \"" << cmd << "\". Type 'help'.\n";
        }

        if (!cpu.is_halted()) std::cout << "> ";
    }
    return 0;
}
