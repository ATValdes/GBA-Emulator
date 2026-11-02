#pragma once

#include <array>
#include <cstdint>
#include <cstddef>


class Bus; //Forward Declaration

class ArmCpu {
    public: 
        explicit ArmCpu(Bus& bus);
        ~ArmCpu();

        void decodeArmInstruction(uint32_t opcode);
        Bus& bus;

        uint32_t read_register(uint8_t register_number);
        void write_register(uint8_t register_number, uint32_t value);

        void write_pc(uint32_t address);
        void exception_return(uint32_t address);

        uint32_t get_cpsr();
        void set_nzcv(uint32_t result, bool carry, bool overflow);
        void set_cpsr(uint32_t value);

        uint32_t get_spsr();
        void set_spsr(uint32_t value);

        uint8_t get_current_mode();
        
        static void fallback_unimplemented(ArmCpu& cpu, uint32_t opcode);
        
        private:
        std::array<uint32_t, 16> registers; // 16 registers or 32 bits
        std::uint32_t cpsr; //flags and CPU control bits are stored in this register
        std::uint32_t spsr; // When an exception arises, CPSR is saved in this register.
        
        uint8_t current_mode;

        void decodeArmInstruction(uint32_t opcode);
        void initialize_lut();

};