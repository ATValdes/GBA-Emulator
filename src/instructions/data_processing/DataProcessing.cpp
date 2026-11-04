#include "DataProcessing.hpp"
#include "ArmConstants.hpp"
#include "shifter/Shifter.hpp"

#include <cstdint>

template <DataProcessing::OpcodeType Operation>
DataProcessing::ALUResult DataProcessing::execute_alu(uint32_t op1, uint32_t op2, bool shifter_carry, bool carry_flag, bool old_overflow) {
    uint32_t result = 0;
    bool carry = shifter_carry;    // logical ops: C comes from the barrel shifter
    bool overflow = old_overflow;  // logical ops: V is unchanged

    if constexpr (Operation == OpcodeType::AND) { result = op1 & op2; }
    else if constexpr (Operation == OpcodeType::EOR) { result = op1 ^ op2; }
    else if constexpr ((Operation == OpcodeType::SUB) || (Operation == OpcodeType::CMP)) {
        result = op1 - op2;
        carry = op1 >= op2;
        overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::RSB) {
        result = op2 - op1;
        carry = op2 >= op1;
        overflow = ((op2 ^ op1) & (op2 ^ result)) >> 31;
    }
    else if constexpr ((Operation == OpcodeType::ADD) || (Operation == OpcodeType::CMN)) {
        result = op1 + op2;
        carry = (uint64_t)op1 + op2 > 0xFFFFFFFF;
        overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::ADC) {
        result = op1 + op2 + carry_flag;
        carry = (uint64_t(op1) + op2 + carry_flag) > 0xFFFFFFFF;
        overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::SBC) {
        result = op1 + ~op2 + carry_flag;
        carry = (uint64_t(op1) + ~op2 + carry_flag) > 0xFFFFFFFF;
        overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::RSC) {
        result = op2 + ~op1 + carry_flag;
        carry = (uint64_t(op2) + ~op1 + carry_flag) > 0xFFFFFFFF;
        overflow = ((op1 ^ op2) & (op2 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::TST) {result = op1 & op2;}
    else if constexpr (Operation == OpcodeType::TEQ) {result = op1 ^ op2;}
    else if constexpr (Operation == OpcodeType::ORR) {result = op1 | op2;}
    else if constexpr (Operation == OpcodeType::MOV) {result = op2;}
    else if constexpr (Operation == OpcodeType::BIC) {result = op1 & (~op2);}
    else if constexpr (Operation == OpcodeType::MVN) {result = ~op2;}

    return {result, carry, overflow};
}

template <DataProcessing::OpcodeType Operation>
void DataProcessing::apply_alu_result(ArmCpu& cpu, uint8_t Rd, bool set_flags, const ALUResult& alu) {

    constexpr bool is_test_instruction = (Operation == OpcodeType::TST || Operation == OpcodeType::TEQ ||
                                          Operation == OpcodeType::CMP || Operation == OpcodeType::CMN);

    // TST, TEQ, CMP, and CMN are "test" instructions; they update flags but don't save results.
    if constexpr (is_test_instruction) {
        cpu.set_nzcv(alu.result, alu.carry, alu.overflow);
    } else {
        // Special case: destination is PC.
        if (Rd == ArmConstants::REG_PROGRAM_COUNTER) [[unlikely]] {
            if (set_flags)
                cpu.exception_return(alu.result); // Restore CPSR from SPSR and branch.
            else
                cpu.write_pc(alu.result);         // Normal branch.

            return;
        }

        cpu.write_register(Rd, alu.result);

        if (set_flags)
            cpu.set_nzcv(alu.result, alu.carry, alu.overflow);
    }
}

template <DataProcessing::OpcodeType Operation>
void DataProcessing::handler_register_instructions(ArmCpu& cpu, uint32_t opcode) {
    const bool set_condition_codes = (opcode >> 20) & 0x1;

    const uint8_t Rn = (opcode >> 16) & 0xF;
    const uint8_t Rd = (opcode >> 12) & 0xF;
    const uint8_t Rm = opcode & 0xF;

    const bool shift_by_register = (opcode >> 4) & 0x1;
    const uint8_t shift_type = (opcode >> 5) & 0x3;

    const bool carry_flag = (cpu.get_cpsr() >> 29) & 1;
    const bool overflow = (cpu.get_cpsr() >> 28) & 1; // current V flag

    const uint32_t op1 = cpu.read_register(Rn);
    const uint32_t rm_value = cpu.read_register(Rm);

    Shifter::ShiftResult shift_result;

    if (shift_by_register) {
        const uint8_t Rs = (opcode >> 8) & 0xF;
        const uint8_t shift_amount = cpu.read_register(Rs) & 0xFF;
        shift_result = Shifter::shift(rm_value, shift_type, shift_amount, carry_flag);
    } else {
        const uint8_t shift_amount = (opcode >> 7) & 0x1F;
        shift_result = Shifter::shift_by_immediate(rm_value, shift_amount, shift_type, carry_flag);
    }

    const auto alu_result = execute_alu<Operation>(op1, shift_result.result, shift_result.carry_out, carry_flag, overflow);
    apply_alu_result<Operation>(cpu, Rd, set_condition_codes, alu_result);
}

template <DataProcessing::OpcodeType Operation>
void DataProcessing::handler_immediate_instructions(ArmCpu& cpu, uint32_t opcode) {
    const bool set_condition_codes = (opcode >> 20) & 0x1;

    const uint32_t immediate = opcode & 0xFF;          // 8-bit immediate
    const uint8_t rotate_field = (opcode >> 8) & 0xF;  // raw 4-bit field; Shifter::rotate_immediate multiplies it by 2

    const uint8_t Rn = (opcode >> 16) & 0xF;
    const uint8_t Rd = (opcode >> 12) & 0xF;

    const bool carry_flag = (cpu.get_cpsr() >> 29) & 1;
    const bool overflow = (cpu.get_cpsr() >> 28) & 1;

    const uint32_t op1 = cpu.read_register(Rn);

    const auto shift_result = Shifter::rotate_immediate(immediate, rotate_field, carry_flag);

    const auto alu_result = execute_alu<Operation>(op1, shift_result.result, shift_result.carry_out, carry_flag, overflow);
    apply_alu_result<Operation>(cpu, Rd, set_condition_codes, alu_result);
}

ArmInstructionHandler DataProcessing::get_data_proc_handler(OpcodeType operation, bool immediate_instruction) {
    if (immediate_instruction) {
        switch (operation) {
            case OpcodeType::AND: return &handler_immediate_instructions<OpcodeType::AND>;
            case OpcodeType::EOR: return &handler_immediate_instructions<OpcodeType::EOR>;
            case OpcodeType::SUB: return &handler_immediate_instructions<OpcodeType::SUB>;
            case OpcodeType::RSB: return &handler_immediate_instructions<OpcodeType::RSB>;
            case OpcodeType::ADD: return &handler_immediate_instructions<OpcodeType::ADD>;
            case OpcodeType::ADC: return &handler_immediate_instructions<OpcodeType::ADC>;
            case OpcodeType::SBC: return &handler_immediate_instructions<OpcodeType::SBC>;
            case OpcodeType::RSC: return &handler_immediate_instructions<OpcodeType::RSC>;
            case OpcodeType::TST: return &handler_immediate_instructions<OpcodeType::TST>;
            case OpcodeType::TEQ: return &handler_immediate_instructions<OpcodeType::TEQ>;
            case OpcodeType::CMP: return &handler_immediate_instructions<OpcodeType::CMP>;
            case OpcodeType::CMN: return &handler_immediate_instructions<OpcodeType::CMN>;
            case OpcodeType::ORR: return &handler_immediate_instructions<OpcodeType::ORR>;
            case OpcodeType::MOV: return &handler_immediate_instructions<OpcodeType::MOV>;
            case OpcodeType::BIC: return &handler_immediate_instructions<OpcodeType::BIC>;
            case OpcodeType::MVN: return &handler_immediate_instructions<OpcodeType::MVN>;

            default: return &ArmCpu::fallback_unimplemented;
        }
    }

    switch (operation) {
        case OpcodeType::AND: return &handler_register_instructions<OpcodeType::AND>;
        case OpcodeType::EOR: return &handler_register_instructions<OpcodeType::EOR>;
        case OpcodeType::SUB: return &handler_register_instructions<OpcodeType::SUB>;
        case OpcodeType::RSB: return &handler_register_instructions<OpcodeType::RSB>;
        case OpcodeType::ADD: return &handler_register_instructions<OpcodeType::ADD>;
        case OpcodeType::ADC: return &handler_register_instructions<OpcodeType::ADC>;
        case OpcodeType::SBC: return &handler_register_instructions<OpcodeType::SBC>;
        case OpcodeType::RSC: return &handler_register_instructions<OpcodeType::RSC>;
        case OpcodeType::TST: return &handler_register_instructions<OpcodeType::TST>;
        case OpcodeType::TEQ: return &handler_register_instructions<OpcodeType::TEQ>;
        case OpcodeType::CMP: return &handler_register_instructions<OpcodeType::CMP>;
        case OpcodeType::CMN: return &handler_register_instructions<OpcodeType::CMN>;
        case OpcodeType::ORR: return &handler_register_instructions<OpcodeType::ORR>;
        case OpcodeType::MOV: return &handler_register_instructions<OpcodeType::MOV>;
        case OpcodeType::BIC: return &handler_register_instructions<OpcodeType::BIC>;
        case OpcodeType::MVN: return &handler_register_instructions<OpcodeType::MVN>;

        default: return &ArmCpu::fallback_unimplemented;
    }
}

// ---------------------------------------------------------------------------
// Explicit instantiations of execute_alu (so other files, e.g. the tests, can link to it).
// This must stay AFTER the definition of execute_alu.
// ---------------------------------------------------------------------------
#define FOR_EACH_OPCODE(X) \
    X(AND) X(EOR) X(SUB) X(RSB) X(ADD) X(ADC) X(SBC) X(RSC) \
    X(TST) X(TEQ) X(CMP) X(CMN) X(ORR) X(MOV) X(BIC) X(MVN)

#define INSTANTIATE_ALU(op) \
    template DataProcessing::ALUResult \
    DataProcessing::execute_alu<DataProcessing::OpcodeType::op>(uint32_t, uint32_t, bool, bool, bool);

FOR_EACH_OPCODE(INSTANTIATE_ALU)