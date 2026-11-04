#include <catch2/catch_test_macros.hpp>

#include "cpu/ArmCpu.hpp"
#include "memory/Bus.hpp"
#include "instructions/data_processing/DataProcessing.hpp"

#include <cstdint>
#include <memory>

using Op = DataProcessing::OpcodeType;

namespace {

constexpr unsigned LSL = 0, LSR = 1, ASR = 2, ROR = 3;

// Runs one data-processing instruction on a fresh CPU. The opcode is built the way an
// assembler would (condition = AL), so the tests read like ARM assembly.
struct Fixture {
    std::unique_ptr<Bus> bus = std::make_unique<Bus>();   // Bus is ~370 KB: keep it off the stack
    ArmCpu cpu{*bus};

    // --- registers and flags -------------------------------------------------
    void set_reg(uint8_t n, uint32_t value) { cpu.write_register(n, value); }
    uint32_t reg(uint8_t n) { return cpu.read_register(n); }

    // Only touches N, Z, C, V (bits 31..28); the mode bits stay as the CPU initialised them.
    void set_flags(bool n, bool z, bool c, bool v) {
        const uint32_t cpsr = cpu.get_cpsr() & 0x0FFFFFFFu;
        cpu.set_cpsr(cpsr | (uint32_t(n) << 31) | (uint32_t(z) << 30) | (uint32_t(c) << 29) | (uint32_t(v) << 28));
    }
    bool N() { return (cpu.get_cpsr() >> 31) & 1; }
    bool Z() { return (cpu.get_cpsr() >> 30) & 1; }
    bool C() { return (cpu.get_cpsr() >> 29) & 1; }
    bool V() { return (cpu.get_cpsr() >> 28) & 1; }

    // --- instruction forms ---------------------------------------------------
    // <op>{S} Rd, Rn, #imm8 ROR (2 * rotate)
    void imm(Op op, bool s, unsigned rd, unsigned rn, unsigned imm8, unsigned rotate = 0) {
        const uint32_t opcode = 0xE0000000u | (1u << 25) | (static_cast<uint32_t>(op) << 21) | (uint32_t(s) << 20)
                              | (rn << 16) | (rd << 12) | (rotate << 8) | imm8;
        DataProcessing::get_data_proc_handler(op, true)(cpu, opcode);
    }
    // <op>{S} Rd, Rn, Rm, <shift> #amount
    void reg_imm_shift(Op op, bool s, unsigned rd, unsigned rn, unsigned rm, unsigned type = LSL, unsigned amount = 0) {
        const uint32_t opcode = 0xE0000000u | (static_cast<uint32_t>(op) << 21) | (uint32_t(s) << 20)
                              | (rn << 16) | (rd << 12) | (amount << 7) | (type << 5) | rm;
        DataProcessing::get_data_proc_handler(op, false)(cpu, opcode);
    }
    // <op>{S} Rd, Rn, Rm, <shift> Rs
    void reg_reg_shift(Op op, bool s, unsigned rd, unsigned rn, unsigned rm, unsigned type, unsigned rs) {
        const uint32_t opcode = 0xE0000000u | (static_cast<uint32_t>(op) << 21) | (uint32_t(s) << 20)
                              | (rn << 16) | (rd << 12) | (rs << 8) | (type << 5) | (1u << 4) | rm;
        DataProcessing::get_data_proc_handler(op, false)(cpu, opcode);
    }
};

} // namespace

#define EXPECT_FLAGS(f, n, z, c, v)                \
    do {                                           \
        CHECK((f).N() == static_cast<bool>(n));    \
        CHECK((f).Z() == static_cast<bool>(z));    \
        CHECK((f).C() == static_cast<bool>(c));    \
        CHECK((f).V() == static_cast<bool>(v));    \
    } while (0)

// ---------------------------------------------------------------------------
// Immediate operand:  imm(op, S, Rd, Rn, imm8, rotate)
// ---------------------------------------------------------------------------

TEST_CASE("MOV #imm writes the register and leaves the flags alone when S=0") {
    Fixture f;
    f.set_flags(true, false, true, true);
    f.imm(Op::MOV, false, 0, 0, 0xFF);
    CHECK(f.reg(0) == 0xFF);
    EXPECT_FLAGS(f, true, false, true, true);
}

TEST_CASE("MOVS #0 sets Z, clears N, and keeps C and V (no rotation)") {
    Fixture f;
    f.set_flags(true, false, true, true);
    f.imm(Op::MOV, true, 0, 0, 0);
    CHECK(f.reg(0) == 0);
    EXPECT_FLAGS(f, false, true, true, true);
}

TEST_CASE("MOVS #1 with C=0 keeps C=0") {
    Fixture f;
    f.set_flags(false, false, false, false);
    f.imm(Op::MOV, true, 0, 0, 1);
    CHECK(f.reg(0) == 1);
    EXPECT_FLAGS(f, false, false, false, false);
}

TEST_CASE("immediate rotation: the rotate field is multiplied by 2 exactly once") {
    Fixture f;
    f.imm(Op::MOV, false, 0, 0, 0xFF, 1);    // ROR 2
    CHECK(f.reg(0) == 0xC000003F);
    f.imm(Op::MOV, false, 1, 0, 0xFF, 4);    // ROR 8
    CHECK(f.reg(1) == 0xFF000000);
    f.imm(Op::MOV, false, 2, 0, 0x03, 15);   // ROR 30
    CHECK(f.reg(2) == 0x0000000C);
}

TEST_CASE("MOVS with a rotated immediate takes C from the rotation") {
    Fixture f;
    f.set_flags(false, false, false, false);
    f.imm(Op::MOV, true, 0, 0, 0xFF, 4);     // 0xFF000000
    CHECK(f.reg(0) == 0xFF000000);
    EXPECT_FLAGS(f, true, false, true, false);
}

TEST_CASE("ADD / ADDS") {
    Fixture f;
    f.set_reg(1, 10);
    f.imm(Op::ADD, false, 2, 1, 5);
    CHECK(f.reg(2) == 15);

    // S=0: flags untouched even if the operation would overflow
    f.set_flags(false, false, false, false);
    f.set_reg(1, 0x7FFFFFFF);
    f.imm(Op::ADD, false, 2, 1, 1);
    CHECK(f.reg(2) == 0x80000000);
    EXPECT_FLAGS(f, false, false, false, false);

    // S=1: signed overflow
    f.imm(Op::ADD, true, 2, 1, 1);
    CHECK(f.reg(2) == 0x80000000);
    EXPECT_FLAGS(f, true, false, false, true);

    // S=1: unsigned carry
    f.set_reg(1, 0xFFFFFFFF);
    f.imm(Op::ADD, true, 2, 1, 1);
    CHECK(f.reg(2) == 0);
    EXPECT_FLAGS(f, false, true, true, false);
}

TEST_CASE("SUBS: C means 'no borrow'") {
    Fixture f;
    f.set_reg(1, 5);
    f.imm(Op::SUB, true, 2, 1, 5);
    CHECK(f.reg(2) == 0);
    EXPECT_FLAGS(f, false, true, true, false);

    f.set_reg(1, 3);
    f.imm(Op::SUB, true, 2, 1, 5);
    CHECK(f.reg(2) == 0xFFFFFFFE);
    EXPECT_FLAGS(f, true, false, false, false);
}

TEST_CASE("RSB #0 negates the register") {
    Fixture f;
    f.set_reg(1, 1);
    f.imm(Op::RSB, false, 2, 1, 0);
    CHECK(f.reg(2) == 0xFFFFFFFF);
}

TEST_CASE("CMP / CMN / TST / TEQ set the flags and never write Rd") {
    Fixture f;
    f.set_reg(3, 0x1234);                    // sentinel in Rd

    f.set_reg(1, 5);
    f.imm(Op::CMP, true, 3, 1, 5);
    CHECK(f.reg(3) == 0x1234);
    CHECK(f.reg(1) == 5);
    EXPECT_FLAGS(f, false, true, true, false);

    f.set_reg(1, 0xFFFFFFFF);
    f.imm(Op::CMN, true, 3, 1, 1);
    CHECK(f.reg(3) == 0x1234);
    EXPECT_FLAGS(f, false, true, true, false);

    f.set_flags(true, true, false, false);
    f.set_reg(1, 0x80);
    f.imm(Op::TST, true, 3, 1, 0x80);        // 0x80 & 0x80 != 0
    CHECK(f.reg(3) == 0x1234);
    EXPECT_FLAGS(f, false, false, false, false);

    f.set_reg(1, 0xFF);
    f.imm(Op::TEQ, true, 3, 1, 0xFF);        // 0xFF ^ 0xFF == 0
    CHECK(f.reg(3) == 0x1234);
    EXPECT_FLAGS(f, false, true, false, false);
}

TEST_CASE("ADC / SBC add the C flag") {
    Fixture f;
    f.set_reg(1, 1);
    f.set_flags(false, false, true, false);
    f.imm(Op::ADC, false, 2, 1, 2);
    CHECK(f.reg(2) == 4);

    f.set_flags(false, false, false, false);
    f.imm(Op::ADC, false, 2, 1, 2);
    CHECK(f.reg(2) == 3);

    f.set_reg(1, 5);
    f.imm(Op::SBC, false, 2, 1, 3);          // 5 - 3 - !C(0) = 1
    CHECK(f.reg(2) == 1);
    f.set_flags(false, false, true, false);
    f.imm(Op::SBC, false, 2, 1, 3);          // 5 - 3 - !C(1) = 2
    CHECK(f.reg(2) == 2);
}

TEST_CASE("ADC / SBC use the CPSR carry, not the shifter carry") {
    Fixture f;
    f.set_flags(false, false, false, false);

    // 0xFF ROR 8 = 0xFF000000 and the shifter carry-out is 1, but the C flag is 0
    f.set_reg(1, 1);
    f.imm(Op::ADC, false, 2, 1, 0xFF, 4);
    CHECK(f.reg(2) == 0xFF000001);

    // 0x02 ROR 2 = 0x80000000, shifter carry-out is 1, C flag is 0: 5 - 0x80000000 - 1
    f.set_reg(1, 5);
    f.imm(Op::SBC, false, 2, 1, 0x02, 1);
    CHECK(f.reg(2) == 0x80000004);
}

TEST_CASE("ADCS propagates the carry into the flags") {
    Fixture f;
    f.set_reg(1, 0xFFFFFFFF);
    f.set_flags(false, false, true, false);
    f.imm(Op::ADC, true, 2, 1, 0);
    CHECK(f.reg(2) == 0);
    EXPECT_FLAGS(f, false, true, true, false);
}

TEST_CASE("logical operations") {
    Fixture f;
    f.set_reg(1, 0xAB);
    f.imm(Op::AND, false, 2, 1, 0x0F);
    CHECK(f.reg(2) == 0x0B);

    f.set_reg(1, 0xF0);
    f.imm(Op::ORR, false, 2, 1, 0x0F);
    CHECK(f.reg(2) == 0xFF);

    f.set_reg(1, 0xFF);
    f.imm(Op::EOR, false, 2, 1, 0x0F);
    CHECK(f.reg(2) == 0xF0);

    f.imm(Op::BIC, false, 2, 1, 0x0F);
    CHECK(f.reg(2) == 0xF0);

    f.imm(Op::MVN, false, 2, 0, 0);
    CHECK(f.reg(2) == 0xFFFFFFFF);
}

// ---------------------------------------------------------------------------
// Register operand, shift by immediate:  reg_imm_shift(op, S, Rd, Rn, Rm, type, amount)
// ---------------------------------------------------------------------------

TEST_CASE("ADD Rd, Rn, Rm") {
    Fixture f;
    f.set_reg(0, 3);
    f.set_reg(1, 4);
    f.reg_imm_shift(Op::ADD, false, 2, 0, 1);
    CHECK(f.reg(2) == 7);
}

TEST_CASE("MOV Rd, Rm, LSL #4") {
    Fixture f;
    f.set_reg(0, 0x12345678);
    f.set_flags(false, false, false, false);

    f.reg_imm_shift(Op::MOV, false, 1, 0, 0, LSL, 4);
    CHECK(f.reg(1) == 0x23456780);
    EXPECT_FLAGS(f, false, false, false, false);   // S=0

    f.reg_imm_shift(Op::MOV, true, 1, 0, 0, LSL, 4);
    EXPECT_FLAGS(f, false, false, true, false);    // C = bit 28 of the original value
}

TEST_CASE("immediate shift amount 0 has special meanings") {
    Fixture f;

    // LSR #0 means LSR #32
    f.set_reg(0, 0x80000000);
    f.set_flags(false, false, false, false);
    f.reg_imm_shift(Op::MOV, true, 1, 0, 0, LSR, 0);
    CHECK(f.reg(1) == 0);
    EXPECT_FLAGS(f, false, true, true, false);

    // ASR #0 means ASR #32
    f.set_flags(false, false, false, false);
    f.reg_imm_shift(Op::MOV, true, 1, 0, 0, ASR, 0);
    CHECK(f.reg(1) == 0xFFFFFFFF);
    EXPECT_FLAGS(f, true, false, true, false);

    // ROR #0 means RRX: rotate right through the carry
    f.set_reg(0, 3);
    f.set_flags(false, false, true, false);
    f.reg_imm_shift(Op::MOV, true, 1, 0, 0, ROR, 0);
    CHECK(f.reg(1) == 0x80000001);
    EXPECT_FLAGS(f, true, false, true, false);
}

TEST_CASE("ADC with a shifted register operand uses the C flag, not the shifter carry") {
    Fixture f;
    f.set_reg(0, 1);
    f.set_reg(1, 0x80000000);
    f.set_flags(false, false, false, false);
    f.reg_imm_shift(Op::ADC, false, 3, 0, 1, LSL, 1);   // op2 = 0, shifter carry = 1, C flag = 0
    CHECK(f.reg(3) == 1);
}

// ---------------------------------------------------------------------------
// Register operand, shift by register:  reg_reg_shift(op, S, Rd, Rn, Rm, type, Rs)
// ---------------------------------------------------------------------------

TEST_CASE("shift by register with amount 0 leaves the value and C untouched") {
    Fixture f;
    f.set_reg(0, 0xDEADBEEF);
    f.set_reg(1, 0);

    f.set_flags(false, false, true, false);
    f.reg_reg_shift(Op::MOV, true, 2, 0, 0, LSL, 1);
    CHECK(f.reg(2) == 0xDEADBEEF);
    EXPECT_FLAGS(f, true, false, true, false);

    f.set_flags(false, false, false, false);
    f.reg_reg_shift(Op::MOV, true, 2, 0, 0, LSL, 1);
    CHECK(f.reg(2) == 0xDEADBEEF);
    EXPECT_FLAGS(f, true, false, false, false);
}

TEST_CASE("shift by register only uses the low byte of Rs") {
    Fixture f;
    f.set_reg(0, 0xDEADBEEF);
    f.set_reg(1, 0x100);                     // low byte is 0
    f.reg_reg_shift(Op::MOV, false, 2, 0, 0, LSL, 1);
    CHECK(f.reg(2) == 0xDEADBEEF);
}

TEST_CASE("shift by register: LSL by 4 and by 32") {
    Fixture f;
    f.set_flags(false, false, false, false);

    f.set_reg(0, 0x12345678);
    f.set_reg(1, 4);
    f.reg_reg_shift(Op::MOV, true, 2, 0, 0, LSL, 1);
    CHECK(f.reg(2) == 0x23456780);
    EXPECT_FLAGS(f, false, false, true, false);

    f.set_reg(0, 1);
    f.set_reg(1, 32);
    f.reg_reg_shift(Op::MOV, true, 2, 0, 0, LSL, 1);
    CHECK(f.reg(2) == 0);
    EXPECT_FLAGS(f, false, true, true, false);   // C = bit 0 of the original value
}