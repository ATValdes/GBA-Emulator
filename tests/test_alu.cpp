#include <catch2/catch_test_macros.hpp>

#include "instructions/data_processing/DataProcessing.hpp"

#include <cstdint>

using DataProcessing::OpcodeType;
using DataProcessing::execute_alu;

// execute_alu<Op>(op1, op2, shifter_carry, carry_flag, old_overflow)
//   shifter_carry : carry-out of the barrel shifter (used by logical ops)
//   carry_flag    : current C flag of the CPSR (used by ADC / SBC / RSC)
//   old_overflow  : current V flag (logical ops leave it unchanged)

#define EXPECT_ALU(expr, expected_result, expected_carry, expected_overflow)  \
    do {                                                                      \
        const auto r_ = (expr);                                               \
        CHECK(r_.result == static_cast<uint32_t>(expected_result));           \
        CHECK(r_.carry == static_cast<bool>(expected_carry));                 \
        CHECK(r_.overflow == static_cast<bool>(expected_overflow));           \
    } while (0)

TEST_CASE("ADD / CMN") {
    EXPECT_ALU(execute_alu<OpcodeType::ADD>(1, 2, false, false, false), 3, false, false);
    EXPECT_ALU(execute_alu<OpcodeType::ADD>(0xFFFFFFFF, 1, false, false, false), 0, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::ADD>(0x7FFFFFFF, 1, false, false, false), 0x80000000, false, true);
    EXPECT_ALU(execute_alu<OpcodeType::ADD>(0x80000000, 0x80000000, false, false, false), 0, true, true);
    // V is recomputed by arithmetic ops, the old value must not leak through
    EXPECT_ALU(execute_alu<OpcodeType::ADD>(1, 2, false, false, true), 3, false, false);
    EXPECT_ALU(execute_alu<OpcodeType::CMN>(0xFFFFFFFF, 1, false, false, false), 0, true, false);
}

TEST_CASE("SUB / CMP: C means 'no borrow'") {
    EXPECT_ALU(execute_alu<OpcodeType::SUB>(5, 3, false, false, false), 2, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::SUB>(3, 5, false, false, false), 0xFFFFFFFE, false, false);
    EXPECT_ALU(execute_alu<OpcodeType::SUB>(5, 5, false, false, false), 0, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::SUB>(0x80000000, 1, false, false, false), 0x7FFFFFFF, true, true);
    EXPECT_ALU(execute_alu<OpcodeType::SUB>(0x7FFFFFFF, 0xFFFFFFFF, false, false, false), 0x80000000, false, true);
    EXPECT_ALU(execute_alu<OpcodeType::CMP>(5, 5, false, false, false), 0, true, false);
}

TEST_CASE("RSB: op2 - op1") {
    EXPECT_ALU(execute_alu<OpcodeType::RSB>(3, 5, false, false, false), 2, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::RSB>(5, 3, false, false, false), 0xFFFFFFFE, false, false);
    EXPECT_ALU(execute_alu<OpcodeType::RSB>(1, 0x80000000, false, false, false), 0x7FFFFFFF, true, true);
}

TEST_CASE("ADC uses the CPSR carry, not the shifter carry") {
    EXPECT_ALU(execute_alu<OpcodeType::ADC>(1, 2, false, true, false), 4, false, false);
    EXPECT_ALU(execute_alu<OpcodeType::ADC>(1, 2, true, false, false), 3, false, false);   // shifter carry ignored
    EXPECT_ALU(execute_alu<OpcodeType::ADC>(0xFFFFFFFF, 0, false, true, false), 0, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::ADC>(0x7FFFFFFF, 0, false, true, false), 0x80000000, false, true);
    EXPECT_ALU(execute_alu<OpcodeType::ADC>(0xFFFFFFFF, 0xFFFFFFFF, false, true, false), 0xFFFFFFFF, true, false);
}

TEST_CASE("SBC: op1 - op2 - !C") {
    EXPECT_ALU(execute_alu<OpcodeType::SBC>(5, 3, false, true, false), 2, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::SBC>(5, 3, false, false, false), 1, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::SBC>(5, 3, true, false, false), 1, true, false);    // shifter carry ignored
    EXPECT_ALU(execute_alu<OpcodeType::SBC>(0, 0, false, false, false), 0xFFFFFFFF, false, false);
    EXPECT_ALU(execute_alu<OpcodeType::SBC>(5, 5, false, false, false), 0xFFFFFFFF, false, false);
}

TEST_CASE("RSC: op2 - op1 - !C") {
    EXPECT_ALU(execute_alu<OpcodeType::RSC>(3, 5, false, true, false), 2, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::RSC>(3, 5, false, false, false), 1, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::RSC>(5, 5, false, false, false), 0xFFFFFFFF, false, false);
}

TEST_CASE("logical ops: result is correct, C comes from the shifter, V is preserved") {
    EXPECT_ALU(execute_alu<OpcodeType::AND>(0xF0F0F0F0, 0xFF00FF00, true,  false, true),  0xF000F000, true,  true);
    EXPECT_ALU(execute_alu<OpcodeType::EOR>(0xFFFF0000, 0x0F0F0F0F, false, true,  false), 0xF0F00F0F, false, false);
    EXPECT_ALU(execute_alu<OpcodeType::ORR>(0xF0F0F0F0, 0x0F0F0F0F, true,  false, false), 0xFFFFFFFF, true,  false);
    EXPECT_ALU(execute_alu<OpcodeType::BIC>(0xFFFFFFFF, 0x0000FFFF, false, true,  true),  0xFFFF0000, false, true);
    EXPECT_ALU(execute_alu<OpcodeType::TST>(0x000000F0, 0x0000000F, true,  false, false), 0,          true,  false);
    EXPECT_ALU(execute_alu<OpcodeType::TEQ>(0x000000FF, 0x000000FF, false, true,  true),  0,          false, true);
}

TEST_CASE("MOV / MVN ignore op1") {
    EXPECT_ALU(execute_alu<OpcodeType::MOV>(0x1234, 0xABCD, true, false, false), 0x0000ABCD, true, false);
    EXPECT_ALU(execute_alu<OpcodeType::MVN>(0x1234, 0x00000000, false, false, true), 0xFFFFFFFF, false, true);
    EXPECT_ALU(execute_alu<OpcodeType::MVN>(0x1234, 0xFFFF0000, false, false, false), 0x0000FFFF, false, false);
}