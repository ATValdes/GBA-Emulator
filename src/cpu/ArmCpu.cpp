#include "ArmCpu.hpp"
#include "ArmConstants.hpp"

#include <iostream>
#include <cstdint>
#include <array>


ArmCpu::ArmCpu(Bus& bus) : bus(bus) {}

ArmCpu::~ArmCpu() = default;

void ArmCpu::fallback_unimplemented(ArmCpu& cpu, uint32_t opcode) {
    std::cout << "Unimplemented ARM instruction! Opcode: 0x"
              << std::hex << opcode << " (Index: " ")\n";
    // cpu.registers[15] += 4;
}

void ArmCpu::set_nzcv(uint32_t result, bool carry, bool overflow) {
    bool n_flag = (result >> 31) & 1;
    bool z_flag = (result == 0);
    
    cpsr &= 0x0FFFFFFF; // Clear N, Z, C, V bits
    cpsr |= (static_cast<uint32_t>(n_flag) << 31);
    cpsr |= (static_cast<uint32_t>(z_flag) << 30);
    cpsr |= (static_cast<uint32_t>(carry) << 29);
    cpsr |= (static_cast<uint32_t>(overflow) << 28);
}


uint32_t ArmCpu::read_register(uint8_t register_number) const {
    return registers[register_number];
}

void ArmCpu::write_register(uint8_t register_number, uint32_t value) {
    registers[register_number] = value;
}

uint32_t ArmCpu::get_cpsr() const {
    return cpsr;
}

uint32_t ArmCpu::get_spsr() const {
    return spsr;
}

void ArmCpu::set_cpsr(uint32_t value) {
    cpsr = value;
    current_mode = value & 0x1F;            // mode bits; add register banking here later
}

uint8_t ArmCpu::get_current_mode() const {
    return current_mode;
}

void ArmCpu::write_pc(uint32_t address) {
    registers[15] = address & ~3u;          // ARM: word-aligned; add the pipeline flush here later
}

void ArmCpu::exception_return(uint32_t address) {
    // Check whether the current mode has an SPSR.
    if (get_current_mode() != ArmConstants::USER_MODE) {
        // Handle architecturally invalid/unpredictable usage.
        return;
    }

    // Save the return status before changing modes.
    uint32_t restored_cpsr = get_spsr();

    // Restore the complete CPSR.
    set_cpsr(restored_cpsr);

    // Branch to the return address.
    write_pc(address);

    // Reconfigure banked registers if necessary.
}