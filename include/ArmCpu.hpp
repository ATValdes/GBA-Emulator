#pragma once

#include <array>
#include <cstdint>
#include <cstddef>


class Bus; //Forward Declaration

// Register index constants
inline constexpr std::size_t REG_LINK_REGISTER = 14;    // Link register number
inline constexpr std::size_t REG_PROGRAM_COUNTER = 15;  // Program counter register number

using ArmInstructionHandler = void(*)(ArmCpu&, uint32_t);

enum class OpcodeType : uint8_t {
    AND = 0x0, EOR = 0x1, SUB = 0x2, RSB = 0x3,
    ADD = 0x4, ADC = 0x5, SBC = 0x6, RSC = 0x7,
    TST = 0x8, TEQ = 0x9, CMP = 0xA, CMN = 0xB,
    ORR = 0xC, MOV = 0xD, BIC = 0xE, MVN = 0xF
};


class ArmCpu {
    public: 
        explicit ArmCpu(Bus& bus);

        void decodeArmInstruction(uint32_t opcode);
        std::array<uint32_t, 16> registers; // 16 registers or 32 bits
        std::uint8_t cpsr; //flags and CPU control bits are stored in this register
        std::uint8_t spsr; // When an exception arises, CPSR is saved in this register.
        Bus& bus;
        
        private:
        ~ArmCpu();
        std::array<ArmInstructionHandler, 4096> arm_lut;
        void fallback_unimplemented(uint32_t opcode);
        void decodeArmInstruction(uint32_t opcode);
        void initialize_lut();
        ArmInstructionHandler ArmCpu::get_data_proc_handler(OpcodeType operation);

        uint32_t shift_handler(bool bit_4, uint8_t shift_amount, uint8_t register_to_shift, uint8_t shift_type, bool& carry);
        uint32_t shift(uint32_t result, uint8_t shift_type, uint8_t shift_amount, bool& carry);
        uint32_t special_shift(uint32_t rm_value, uint8_t shift_type, bool& cpsr_carry);
};