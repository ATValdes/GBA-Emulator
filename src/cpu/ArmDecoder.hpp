#pragma once

#include <array>
#include <cstdint>

#include "InstructionTypes.hpp"

class ArmDecoder {
public:

    static constexpr size_t LUT_SIZE = 4096;

    ArmDecoder();

    static constexpr uint16_t get_index(uint32_t opcode);
    void initialize_lut();
    void execute(ArmCpu& cpu, uint32_t instruction) const;

private:
    std::array<ArmInstructionHandler, LUT_SIZE> arm_lut;
};