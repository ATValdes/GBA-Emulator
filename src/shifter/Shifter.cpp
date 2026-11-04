#include "Shifter.hpp"

Shifter::ShiftResult Shifter::shift(uint32_t register_value, uint8_t shift_type, uint8_t shift_amount, bool carry_in) {
    // If shift amount is 0, this function shouldn't handle it (handled by special_shift)
    // or if a Register Shift evaluates to 0, it behaves like LSL#0 (no change to register_value or carry)
    if (shift_amount == 0)
        return {register_value, carry_in};

    switch (shift_type & 3)
    {
    case 0x0: // LSL (Logical Shift Left)
        if (shift_amount < 32) {
            // Carry becomes the last bit shifted out from the left (Bit 32 - shift_amount)
            carry_in = (register_value >> (32 - shift_amount)) & 1;
            return {
                register_value << shift_amount, // register_value
                carry_in                   // carry_out
            };

        } else if (shift_amount == 32) {
            carry_in = register_value & 1; // Carry becomes bit 0
            return {0, carry_in};

        } else { // shift_amount > 32
            return {0, 0};
        }
    
    case 0x1: // LSR (Logical Shift Right)
        if (shift_amount < 32) {
            // Carry becomes the last bit shifted out from the right
            carry_in = (register_value >> (shift_amount - 1)) & 1;
            return {
                register_value >> shift_amount, //register_value
                carry_in                   //carry_out
            };

        } else if (shift_amount == 32) {
            carry_in = (register_value >> 31) & 1; // Carry becomes bit 31
            return {0, carry_in};

        } else { // shift_amount > 32
            return {0, 0};
        }
    
    case 0x2: // ASR (Arithmetic Shift Right)
        if (shift_amount < 32) {
            carry_in = (register_value >> (shift_amount - 1)) & 1;
            return {
                static_cast<uint32_t>(static_cast<int32_t>(register_value) >> shift_amount),    // register_value
                carry_in                                                                   // carry_out
            };
        } else { 
            // ASR >= 32 saturates the register with the sign bit (bit 31)
            carry_in = (register_value >> 31) & 1;
            return {
                (carry_in) ? 0xFFFFFFFF : 0,   // register_value
                carry_in                       // carry_out
            };
        }
    
    default: // ROR (Rotate Right)
    {
        // ROR by amounts >= 32 simply wraps around (e.g., ROR #33 is identical to ROR #1)
        const uint8_t rotate = shift_amount & 31u;
        if (rotate == 0)
            return {register_value, static_cast<bool>(register_value >> 31)};
        
        // Carry becomes the last bit rotated out of the right side, 
        // which ends up becoming the new sign bit (bit 31) of the register_value.
        uint32_t shifted_result = (register_value >> rotate) | (register_value << (32u - rotate));
        carry_in = (shifted_result >> 31) & 1;
        return {shifted_result, carry_in};
    }
    }
}

Shifter::ShiftResult Shifter::special_shift(uint32_t register_value, uint8_t shift_type, bool carry_in) {
    // Special shift when Zero Shift Amount (Shift Register by Immediate, with Immediate=0)
    /*
    LSL#0: No shift performed, ie. directly Op2=Rm, the C flag is NOT affected.
    LSR#0: Interpreted as LSR#32, ie. Op2 becomes zero, C becomes Bit 31 of Rm.
    ASR#0: Interpreted as ASR#32, ie. Op2 and C are filled by Bit 31 of Rm.
    ROR#0: Interpreted as RRX#1 (RCR), like ROR#1, but Op2 Bit 31 set to old C
    */

    switch (shift_type) {
        case 0x0: // LSL #0
            // Op2 = Rm, Carry flag is completely unaffected
            return {
                register_value,   // result
                carry_in  // carry_out
            };

        case 0x1: // LSR #0 (Interpreted as LSR #32)
            carry_in = (register_value >> 31) & 1;
            return {0, carry_in}; // register_value becomes 0

        case 0x2: // ASR #0 (Interpreted as ASR #32)
            carry_in = (register_value >> 31) & 1;
            return {
                (carry_in) ? 0xFFFFFFFF : 0,  // result
                carry_in                      // carry_out
            };

        case 0x3: // ROR #0 (Interpreted as RRX #1)
        {
            bool old_carry = carry_in;
            carry_in = register_value & 1;
            
            // Slide Rm right by 1, and place the old carry into bit 31
            return {
                (register_value >> 1) | (static_cast<uint32_t>(old_carry) << 31), // result
                carry_in                                                  // carry_out
            };
        }
        default:
            return {
                register_value,   // result
                carry_in  // carry_out
            };
    }
}

Shifter::ShiftResult Shifter::rotate_immediate(uint8_t immediate, uint8_t rotate_amount, bool carry_in) {
    uint8_t rotation = rotate_amount * 2;

    uint32_t result = immediate;

    if (rotation != 0)
        result = (result >> rotation) | (result << (32 - rotation));

    bool carry_out = (rotation == 0) ? carry_in : ((result >> 31) & 1);

    return { result, carry_out };
}

Shifter::ShiftResult Shifter::shift_by_immediate(uint32_t register_value, uint8_t shift_ammount, uint8_t shift_type, bool carry_in){
    // Shift by Immediate
    if (shift_ammount != 0)
        return shift(register_value, shift_type, shift_ammount, carry_in);

    // In case Zero Shift Amount (Shift Register by Immediate, with Immediate=0)
    return special_shift(register_value, shift_type, carry_in);
}