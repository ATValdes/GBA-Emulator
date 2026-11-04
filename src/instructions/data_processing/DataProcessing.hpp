#pragma once

#include "cpu/ArmCpu.hpp"
#include "InstructionTypes.hpp"

class ArmCpu;

namespace DataProcessing {
    struct ALUResult {
        uint32_t result;
        bool carry;
        bool overflow;
    };

    enum class OpcodeType : uint8_t {
        AND = 0x0, EOR = 0x1, SUB = 0x2, RSB = 0x3,
        ADD = 0x4, ADC = 0x5, SBC = 0x6, RSC = 0x7,
        TST = 0x8, TEQ = 0x9, CMP = 0xA, CMN = 0xB,
        ORR = 0xC, MOV = 0xD, BIC = 0xE, MVN = 0xF
    };

    // shifter_carry: carry-out of the barrel shifter (used by logical ops).
    // carry_flag:    current C flag of the CPSR (used by ADC / SBC / RSC).
    // old_overflow:  current V flag (logical ops leave it unchanged).
    template <OpcodeType Operation>
    ALUResult execute_alu(uint32_t op1, uint32_t op2, bool shifter_carry, bool carry_flag, bool old_overflow);

    template <OpcodeType Operation>
    void apply_alu_result(ArmCpu& cpu, uint8_t Rd, bool set_flags, const ALUResult& alu);

    template <OpcodeType Operation>
    void handler_register_instructions(ArmCpu& cpu, uint32_t opcode);

    template <OpcodeType Operation>
    void handler_immediate_instructions(ArmCpu& cpu, uint32_t opcode);

    ArmInstructionHandler get_data_proc_handler(OpcodeType operation, bool immediate_instruction);
}