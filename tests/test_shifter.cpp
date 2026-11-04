#include <catch2/catch_test_macros.hpp>

#include "shifter/Shifter.hpp"

#include <cstdint>

// Shift type encoding (opcode bits 6:5).
namespace {
    constexpr uint8_t LSL = 0;
    constexpr uint8_t LSR = 1;
    constexpr uint8_t ASR = 2;
    constexpr uint8_t ROR = 3;
}

#define EXPECT_SHIFT(expr, expected_result, expected_carry)              \
    do {                                                                 \
        const auto r_ = (expr);                                          \
        CHECK(r_.result == static_cast<uint32_t>(expected_result));      \
        CHECK(r_.carry_out == static_cast<bool>(expected_carry));        \
    } while (0)

// ---------------------------------------------------------------------------
// Shifter::shift(value, type, amount, carry_in)  -- amount taken from a register
// ---------------------------------------------------------------------------

TEST_CASE("shift by register: amount 0 keeps value and carry for every type") {
    for (uint8_t type = 0; type < 4; ++type) {
        INFO("type = " << int(type));
        EXPECT_SHIFT(Shifter::shift(0xDEADBEEF, type, 0, false), 0xDEADBEEF, false);
        EXPECT_SHIFT(Shifter::shift(0xDEADBEEF, type, 0, true),  0xDEADBEEF, true);
    }
}

TEST_CASE("shift by register: LSL") {
    EXPECT_SHIFT(Shifter::shift(0x12345678, LSL, 4,   false), 0x23456780, true);  // bit 28 is the last bit out
    EXPECT_SHIFT(Shifter::shift(0x00000003, LSL, 31,  false), 0x80000000, true);
    EXPECT_SHIFT(Shifter::shift(0x00000001, LSL, 32,  false), 0,          true);  // C = bit 0
    EXPECT_SHIFT(Shifter::shift(0x00000002, LSL, 32,  true),  0,          false);
    EXPECT_SHIFT(Shifter::shift(0xFFFFFFFF, LSL, 33,  true),  0,          false);
    EXPECT_SHIFT(Shifter::shift(0xFFFFFFFF, LSL, 255, true),  0,          false);
}

TEST_CASE("shift by register: LSR") {
    EXPECT_SHIFT(Shifter::shift(0x00000003, LSR, 1,  false), 0x00000001, true);
    EXPECT_SHIFT(Shifter::shift(0x000000F8, LSR, 4,  false), 0x0000000F, true);   // bit 3 is the last bit out
    EXPECT_SHIFT(Shifter::shift(0x80000000, LSR, 31, true),  0x00000001, false);
    EXPECT_SHIFT(Shifter::shift(0x80000000, LSR, 32, false), 0,          true);   // C = bit 31
    EXPECT_SHIFT(Shifter::shift(0x7FFFFFFF, LSR, 32, true),  0,          false);
    EXPECT_SHIFT(Shifter::shift(0xFFFFFFFF, LSR, 33, true),  0,          false);
}

TEST_CASE("shift by register: ASR") {
    EXPECT_SHIFT(Shifter::shift(0x80000008, ASR, 4,   false), 0xF8000000, true);
    EXPECT_SHIFT(Shifter::shift(0x7FFFFFF0, ASR, 4,   true),  0x07FFFFFF, false);
    EXPECT_SHIFT(Shifter::shift(0x80000000, ASR, 32,  false), 0xFFFFFFFF, true);  // saturates with the sign bit
    EXPECT_SHIFT(Shifter::shift(0x7FFFFFFF, ASR, 32,  true),  0x00000000, false);
    EXPECT_SHIFT(Shifter::shift(0xFFFFFFFF, ASR, 200, false), 0xFFFFFFFF, true);
    EXPECT_SHIFT(Shifter::shift(0x40000000, ASR, 200, true),  0x00000000, false);
}

TEST_CASE("shift by register: ROR") {
    EXPECT_SHIFT(Shifter::shift(0x0000000F, ROR, 4,  false), 0xF0000000, true);
    EXPECT_SHIFT(Shifter::shift(0x00000002, ROR, 1,  true),  0x00000001, false);
    EXPECT_SHIFT(Shifter::shift(0x00000001, ROR, 1,  false), 0x80000000, true);
    EXPECT_SHIFT(Shifter::shift(0x0000000F, ROR, 36, false), 0xF0000000, true);   // 36 behaves like 4
}

TEST_CASE("shift by register: ROR by a multiple of 32 leaves the value and sets C = bit 31") {
    EXPECT_SHIFT(Shifter::shift(0x80000001, ROR, 32, false), 0x80000001, true);
    EXPECT_SHIFT(Shifter::shift(0x00000001, ROR, 32, true),  0x00000001, false);
    EXPECT_SHIFT(Shifter::shift(0x80000001, ROR, 64, false), 0x80000001, true);
}

// ---------------------------------------------------------------------------
// Shifter::shift_by_immediate(value, amount, type, carry_in)
// ---------------------------------------------------------------------------

TEST_CASE("shift by immediate 0: LSL #0 is a no-op and C is preserved") {
    EXPECT_SHIFT(Shifter::shift_by_immediate(0xDEADBEEF, 0, LSL, false), 0xDEADBEEF, false);
    EXPECT_SHIFT(Shifter::shift_by_immediate(0xDEADBEEF, 0, LSL, true),  0xDEADBEEF, true);
}

TEST_CASE("shift by immediate 0: LSR #0 means LSR #32") {
    EXPECT_SHIFT(Shifter::shift_by_immediate(0x80000000, 0, LSR, false), 0, true);
    EXPECT_SHIFT(Shifter::shift_by_immediate(0x7FFFFFFF, 0, LSR, true),  0, false);
}

TEST_CASE("shift by immediate 0: ASR #0 means ASR #32") {
    EXPECT_SHIFT(Shifter::shift_by_immediate(0x80000000, 0, ASR, false), 0xFFFFFFFF, true);
    EXPECT_SHIFT(Shifter::shift_by_immediate(0x7FFFFFFF, 0, ASR, true),  0x00000000, false);
}

TEST_CASE("shift by immediate 0: ROR #0 means RRX (rotate right through carry)") {
    EXPECT_SHIFT(Shifter::shift_by_immediate(0x00000003, 0, ROR, true),  0x80000001, true);
    EXPECT_SHIFT(Shifter::shift_by_immediate(0x00000002, 0, ROR, false), 0x00000001, false);
    EXPECT_SHIFT(Shifter::shift_by_immediate(0x00000001, 0, ROR, false), 0x00000000, true);
}

TEST_CASE("shift by immediate: non-zero amounts behave like the register version") {
    EXPECT_SHIFT(Shifter::shift_by_immediate(0x12345678, 4, LSL, false), 0x23456780, true);
    EXPECT_SHIFT(Shifter::shift_by_immediate(0xFFFFFFF0, 4, ASR, true),  0xFFFFFFFF, false);
    EXPECT_SHIFT(Shifter::shift_by_immediate(0xF0000000, 4, ROR, true),  0x0F000000, false);
}

// ---------------------------------------------------------------------------
// Shifter::rotate_immediate(imm8, rotate_field, carry_in)
// rotate_field is the raw 4-bit field (opcode bits 11:8); rotation = field * 2.
// ---------------------------------------------------------------------------

TEST_CASE("rotate immediate: rotation 0 keeps the value and the carry") {
    EXPECT_SHIFT(Shifter::rotate_immediate(0x01, 0, true),  0x00000001, true);
    EXPECT_SHIFT(Shifter::rotate_immediate(0x01, 0, false), 0x00000001, false);
}

TEST_CASE("rotate immediate: rotation is field * 2") {
    EXPECT_SHIFT(Shifter::rotate_immediate(0x02, 1,  false), 0x80000000, true);   // ROR 2
    EXPECT_SHIFT(Shifter::rotate_immediate(0xFF, 1,  false), 0xC000003F, true);   // ROR 2
    EXPECT_SHIFT(Shifter::rotate_immediate(0xFF, 4,  false), 0xFF000000, true);   // ROR 8
    EXPECT_SHIFT(Shifter::rotate_immediate(0x01, 8,  true),  0x00010000, false);  // ROR 16
    EXPECT_SHIFT(Shifter::rotate_immediate(0x03, 15, true),  0x0000000C, false);  // ROR 30
}