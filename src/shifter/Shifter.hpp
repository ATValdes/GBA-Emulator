#pragma once

#include <cstdint>

namespace Shifter {

    struct ShiftResult {
        uint32_t result;
        bool carry_out;
    };

    ShiftResult shift_by_immediate(uint32_t register_value, uint8_t shift_ammount, uint8_t shift_type, bool carry_in);
    ShiftResult shift(uint32_t result, uint8_t shift_type, uint8_t shift_amount, bool carry_in);
    ShiftResult special_shift(uint32_t rm_value, uint8_t shift_type, bool carry_in);
    ShiftResult rotate_immediate(uint8_t immediate, uint8_t rotate_amount, bool carry_in);
}
