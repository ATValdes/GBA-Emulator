#include "DataProcessing.hpp"
#include "ArmConstants.hpp"
#include "Shifter.hpp"

#include <iostream>
#include <cstdint>
#include <array>

template <DataProcessing::OpcodeType Operation>
DataProcessing::ALUResult DataProcessing::execute_alu(uint32_t op1, uint32_t op2, bool carry_in, bool old_overflow) {
    uint32_t result = 0;
    bool carry = carry_in;
    bool overflow = old_overflow;

    if constexpr (Operation == OpcodeType::AND) {result = op1 & op2; }
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
        result = op1 + op2 + carry_in;
        carry  = (uint64_t(op1) + op2 + carry_in) > 0xFFFFFFFF;
        overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::SBC) {
        result = op1 + ~op2 + carry_in;
        carry  = (uint64_t(op1) + ~op2 + carry_in) > 0xFFFFFFFF;
        overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::RSC) {
        result = op2 + ~op1 + carry_in;
        carry  = (uint64_t(op2) + ~op1 + carry_in) > 0xFFFFFFFF;
        overflow = ((op1 ^ op2) & (op2 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::TST) { result = op1 & op2; }
    else if constexpr (Operation == OpcodeType::TEQ) { result = op1 ^ op2; }
    else if constexpr (Operation == OpcodeType::ORR) { result = op1 | op2; }
    else if constexpr (Operation == OpcodeType::MOV) { result = op2; }
    else if constexpr (Operation == OpcodeType::BIC) { result = op1 & (~op2); }
    else if constexpr (Operation == OpcodeType::MVN) { result = ~op2; }

    return {result, carry, overflow};
}

template <DataProcessing::OpcodeType Operation>
void DataProcessing::apply_alu_result(ArmCpu& cpu, uint8_t Rd, bool set_flags, const ALUResult& alu) {

    constexpr bool is_test_instruction = (Operation == OpcodeType::TST || Operation == OpcodeType::TEQ || 
                                          Operation == OpcodeType::CMP || Operation == OpcodeType::CMN);



    // TST, TEQ, CMP, and CMN are "test" instructions; they update flags but don't save results.
    if constexpr (is_test_instruction) {
        cpu.set_nzcv(alu.result, alu.carry, alu.overflow);
        return;
    }
    
    // Special case: destination is PC.
    if (Rd == ArmConstants::REG_PROGRAM_COUNTER) [[unlikely]] {
        if (set_flags)
            cpu.exception_return(alu.result); // Restore CPSR from SPSR and branch.
        else
            cpu.write_pc(alu.result); // Normal branch.

        return;
    }
        // if (Rd == 15) [[unlikely]] {
        // // Exception return: S=1 && Rd=15 restores CPSR from SPSR.
        // // (User mode has no SPSR — hardware is unpredictable; just skip.)
        // if (set_condition_codes && cpu.current_mode() != Mode::User) {
        //     cpu.cpsr = cpu.spsr[cpu.current_mode()];   // restores T bit too
        //     // If your register file is banked, re-bank here based on new mode.
        // }

        // cpu.registers[15] = result & ~3u;  // ARM: word-align; the T bit comes from SPSR
        // cpu.pipeline_flush();              // refill from new PC — do NOT fall through to += 4
        // return;
    // }

    cpu.write_register(Rd, alu.result);

    if (set_flags)
        cpu.set_nzcv(alu.result, alu.carry, alu.overflow);
}

template <DataProcessing::OpcodeType Operation>
void DataProcessing::handler_register_instructions(ArmCpu& cpu, uint32_t opcode) {
    bool set_condition_codes = (opcode >> 20) & 0x1;

    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;
    uint8_t Rm = opcode & 0xF;

    bool shift_by_register = (opcode >> 4) & 0x1;
    uint8_t shift_type = (opcode >> 5) & 0x3;

    uint32_t carry_in = (cpu.get_cpsr() >> 29) & 1;
    bool carry = carry_in;
    bool overflow = (cpu.get_cpsr() >> 28) & 1; // default to current V flag

    uint32_t op1 = cpu.read_register(Rn);
    uint32_t rm_value = cpu.read_register(Rm);
    uint32_t op2;

    Shifter::ShiftResult shift_result;

    if (shift_by_register) {
        uint8_t Rs = (opcode >> 8) & 0xF;
        uint8_t shift_amount = cpu.read_register(Rs) & 0xFF;
        shift_result = Shifter::shift(rm_value, shift_type, shift_amount, carry_in);
    } else {
        uint8_t shift_ammount = (opcode >> 7) & 0x1F;
        shift_result = Shifter::shift_by_immediate(rm_value, shift_ammount, shift_type, carry_in);
    }

    auto alu_result = execute_alu<Operation>(op1, shift_result.result, carry_in, shift_result.carry_out, overflow);
    apply_alu_result<Operation>(cpu, Rd, set_condition_codes, alu_result);

}

template <DataProcessing::OpcodeType Operation>
uint32_t handler_immediate_instructions(ArmCpu& cpu, uint32_t opcode) {
    bool set_condition_codes = (opcode >> 20) & 0x1;

    uint32_t immediate = opcode & 0xFF; // Decode immediate Operand2.
    uint8_t rotate_amount = ((opcode >> 8) & 0xF) * 2; // Rotation is done in steps of 2 from 0 to 30.
    
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;

    bool carry_in = (cpu.get_cpsr() >> 29) & 1;
    bool overflow = (cpu.get_cpsr() >> 28) & 1;

    uint32_t op1 = cpu.read_register(Rn);

    auto shift_result = Shifter::rotate_immediate(immediate, rotate_amount, carry_in);

    ALUResult alu_result = execute_alu<Operation>(op1, shift_result.result, carry_in, shift_result.carry_out, overflow);
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