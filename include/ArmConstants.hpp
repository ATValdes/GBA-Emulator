#pragma once

#include <cstdint>
#include <iostream>

namespace ArmConstants {
    constexpr uint8_t REG_PROGRAM_COUNTER = 15;
    constexpr uint8_t REG_LINK_REGISTER = 14;
    constexpr uint8_t REG_STACK_POINTER = 13;

    constexpr uint8_t UNDEFINED_MODE = 0;
    constexpr uint8_t USER_MODE = 1;
    constexpr uint8_t FIQ_MODE = 2;
    constexpr uint8_t SUPERVISOR_MODE = 3;
    constexpr uint8_t IRQ_MODE = 4;
}