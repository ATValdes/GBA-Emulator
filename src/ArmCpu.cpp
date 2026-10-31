#include "ArmCpu.hpp"

#include <iostream>
#include <cstdint>
#include <array>
#include <bit>

#ifdef __GNUC__ // GCC, Clang, ICC

#define unreachable() (__builtin_unreachable())

#elifdef _MSC_VER // MSVC

#define unreachable() (__assume(false))

#else

[[noreturn]] inline void unreachable_impl() {}
#define unreachable() (unreachable_impl())

#endif


ArmCpu::ArmCpu(Bus& bus) : bus(bus) {
}


void ArmCpu::fallback_unimplemented(uint32_t opcode) {
    std::cout << "Unimplemented ARM instruction! Opcode: 0x"
              << std::hex << opcode << " (Index: " << get_arm_index(opcode) << ")\n";
    registers[15] += 4;
}

static constexpr uint16_t get_arm_index(uint32_t opcode) {
    // Extract bits 27-20
    uint16_t op_high = (opcode >> 16) & 0x0FF0; 
    
    // Extract bits 7-4
    uint16_t op_low = (opcode >> 4) & 0x000F;
    
    // Merge them into a single 12-bit identifier (0x000 to 0xFFF)
    return op_high | op_low;
}
void ArmCpu::decodeArmInstruction(uint32_t opcode) {

    uint16_t lut_index = get_arm_index(opcode);
    
}

static ArmInstructionHandler handler_for_alu_opcode(uint8_t op) noexcept {
    switch (op) {
        case 0x0: return &handler_data_processing<OpcodeType::AND>;
        case 0x1: return &handler_data_processing<OpcodeType::EOR>;
        case 0x2: return &handler_data_processing<OpcodeType::SUB>;
        case 0x4: return &handler_data_processing<OpcodeType::ADD>;
        case 0x8: return &handler_data_processing<OpcodeType::TST>;
        case 0xA: return &handler_data_processing<OpcodeType::CMP>;
        case 0xC: return &handler_data_processing<OpcodeType::ORR>;
        case 0xD: return &handler_data_processing<OpcodeType::MOV>;
        case 0xE: return &handler_data_processing<OpcodeType::BIC>;
        case 0xF: return &handler_data_processing<OpcodeType::MVN>;
        default:  return nullptr;
    }
}

void ArmCpu::initialize_lut() {
    // 1. Inicializar toda la tabla con el manejador fallback por defecto
    // Esto evita crashes si el juego ejecuta un opcode inválido o no implementado.
    arm_lut.fill(&ArmCpu::fallback_unimplemented);

    // 2. Mapear instrucciones de Data Processing (Procesamiento de Datos)
    // El ARM7TDMI tiene dos grandes variantes para Data Processing:
    // Variante A: El segundo operando es un Registro Desplazado (ej. Rm LSL #2 o Rm LSL Rs)
    // Variante B: El segundo operando es un Valor Inmediato Rotado.

    // --- VARIANTE A: DATA PROCESSING CON REGISTRO (Bits 27-26 = 00, Bit 25 = 0) ---
    // Estructura del Opcode para el índice:
    // Bits 27-26 always 00b.
    // Bit 25 = 0 (indicates that 2nd operand is a register).
    // Bits 24-21 are the opcode (0x0 a 0xF).
    // Bit 20 is the S-bit (0 o 1).
    // Bits 7-4 varían dependiendo de si el desplazamiento es por inmediato o por registro.

    // Loops through all 16 opcodes (0-F)
    for (uint8_t opcode = 0; opcode < 16; ++opcode) {
        for (uint8_t s_bit = 0; s_bit < 2; ++s_bit) {
            // Combine bits 24-21 (opcode) and bit 20 (s_bit)
            uint8_t bits_24_20 = (opcode << 1) | s_bit;

            // S must be 1 for opcodes 8-B
            if (opcode >= 0x8 && opcode <= 0xB)
                continue;

            // Variations of bits 7-4
            for (uint8_t bits_7_4 = 0; bits_7_4 < 16; ++bits_7_4) {
                
                // If bit 4 is 1, bit 7 must be 1 too, otherwise instruction is multiply or LDREX or undefined
                if (bits_7_4 == 0x9)
                    continue;

                // Bits 27-26 son 0, Bit 25 es 0.
                uint16_t index = (static_cast<uint16_t>(bits_24_20) << 4) | bits_7_4;

                // Asignar dinámicamente usando metaprogramación de plantillas (templates)
                arm_lut[index] = get_data_proc_handler(static_cast<OpcodeType>(opcode));
            }
        }
    }
}

// 3. Helper de asignación en tiempo de compilación para resolver el puntero de función de la plantilla
ArmInstructionHandler ArmCpu::get_data_proc_handler(OpcodeType operation) {
    switch (operation) {
        case OpcodeType::AND: return &handler_data_processing<OpcodeType::AND>;
        case OpcodeType::EOR: return &handler_data_processing<OpcodeType::EOR>;
        case OpcodeType::SUB: return &handler_data_processing<OpcodeType::SUB>;
        case OpcodeType::RSB: return &handler_data_processing<OpcodeType::RSB>;
        case OpcodeType::ADD: return &handler_data_processing<OpcodeType::ADD>;
        case OpcodeType::ADC: return &handler_data_processing<OpcodeType::ADC>;
        case OpcodeType::SBC: return &handler_data_processing<OpcodeType::SBC>;
        case OpcodeType::RSC: return &handler_data_processing<OpcodeType::RSC>;
        case OpcodeType::TST: return &handler_data_processing<OpcodeType::TST>;
        case OpcodeType::TEQ: return &handler_data_processing<OpcodeType::TEQ>;
        case OpcodeType::CMP: return &handler_data_processing<OpcodeType::CMP>;
        case OpcodeType::CMN: return &handler_data_processing<OpcodeType::CMN>;
        case OpcodeType::ORR: return &handler_data_processing<OpcodeType::ORR>;
        case OpcodeType::MOV: return &handler_data_processing<OpcodeType::MOV>;
        case OpcodeType::BIC: return &handler_data_processing<OpcodeType::BIC>;
        case OpcodeType::MVN: return &handler_data_processing<OpcodeType::MVN>;

        default: return &ArmCpu::fallback_unimplemented;
    }
}

// void ArmCpu::populate_lut() {
//     arm_lut.fill(&ArmCpu::fallback_unimplemented);

//     uint16_t low_bits;
//     uint16_t lut_index; // The index represents bits 27-21 and 7-4 of the opcode, making a 12 bit identifier (0x000 to 0xFFF)

//     bool undefined;

//     /*
//     ALU - Data Processing codes

//     Bits 27-26 are 00b for ALU operations
//     Bit 25 Immediate flag (0=Register, 1=Immediate)
//     Bits 24-21 opcode

//     Non immediate opcodes:
//         -Bit 25 must be 0
//         -If bit 4 is 1 then bit 7 must be 0 (otherwise multiply or LDREX or undefined)
//     */

//     arm_lut[get_index_for_and(i)] = &handler_data_processing<OpcodeType::AND>;
//     arm_lut[get_index_for_add(i)] = &handler_data_processing<OpcodeType::ADD>;
//     arm_lut[get_index_for_cmp(i)] = &handler_data_processing<OpcodeType::CMP>;
//     arm_lut[get_index_for_mov(i)] = &handler_data_processing<OpcodeType::MOV>;

//     uint16_t and_index;
//     uint16_t xor_index;
//     uint16_t sub_index;
//     uint16_t rsb_index;
//     uint16_t add_index;
//     uint16_t adc_index;
//     uint16_t sbc_index;
//     uint16_t rsc_index;
//     uint16_t tst_index;
//     uint16_t teq_index;
//     uint16_t cmp_index;
//     uint16_t cmn_index;
//     uint16_t orr_index;
//     uint16_t mov_index;
//     uint16_t bic_index;
//     uint16_t mvn_index;
    
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         and_index = (0x0 << 4) | low_bits; // AND = 00 0 0000

//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         if (!undefined)
//             arm_lut[lut_index] = &ArmCpu::arm_instruction_AND;
//     }
    
//     // EOR = 00 0 0001
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x1 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_XOR;
//     }
    
//     // SUB = 00 0 0010
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x2 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_SUB;
//     }

//     // RSB = 00 0 0011
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x3 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_RSB;
//     }

//     // ADD = 00 0 0100
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x4 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_ADD;
//     }

//     // ADC = 00 0 0101
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x5 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_ADC;
//     }

//     // SBC = 00 0 0110
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x6 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_SBC;
//     }

//     // RSC = 00 0 0111
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x7 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_RSC;
//     }
    
//     // TST = 00 0 1000
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x8 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_TST;
//     }

//     // TEQ = 00 0 1001
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0x9 << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_TEQ;
//     }

//     // CMP = 00 0 1010
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0xA << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_CMP;
//     }

//     // CMN = 00 0 1011
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0xB << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_CMN;
//     }

//     // ORR = 00 0 1100
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0xC << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_ORR;
//     }

//     // MOV = 00 0 1101
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0xD << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_MOV;
//     }

//     // BIC = 00 0 1110
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0xE << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_BIC;
//     }

//     // MVN = 00 0 1111
//     for (low_bits = 0; low_bits < 16; ++low_bits) {
//         lut_index = (0xF << 4) | low_bits;
//         undefined = ((lut_index & 0x9) == 0x9); // Checks if bites 7 and 4 are 1 at the same time
//         // if (!undefined)
//         //     arm_lut[low_bits] = &ArmCpu::armInstruction_MVN;
//     }

//     //ALU Immediate
// }

//////////////////

uint32_t ArmCpu::shift(uint32_t result, uint8_t shift_type, uint8_t shift_amount, bool& carry) {
    // If shift amount is 0, this function shouldn't handle it (handled by special_shift)
    // or if a Register Shift evaluates to 0, it behaves like LSL#0 (no change to value or carry)
    if (shift_amount == 0) {
        return result;
    }

    switch (shift_type)
    {
    case 0x0: // LSL (Logical Shift Left)
        if (shift_amount < 32) {
            // Carry becomes the last bit shifted out from the left (Bit 32 - shift_amount)
            carry = (result >> (32 - shift_amount)) & 1;
            return result << shift_amount;
        } else if (shift_amount == 32) {
            carry = result & 1; // Carry becomes bit 0
            return 0;
        } else { // shift_amount > 32
            carry = 0;
            return 0;
        }
    
    case 0x1: // LSR (Logical Shift Right)
        if (shift_amount < 32) {
            // Carry becomes the last bit shifted out from the right
            carry = (result >> (shift_amount - 1)) & 1;
            return result >> shift_amount;
        } else if (shift_amount == 32) {
            carry = (result >> 31) & 1; // Carry becomes bit 31
            return 0;
        } else { // shift_amount > 32
            carry = 0;
            return 0;
        }
    
    case 0x2: // ASR (Arithmetic Shift Right)
        if (shift_amount < 32) {
            carry = (result >> (shift_amount - 1)) & 1;
            return static_cast<uint32_t>(static_cast<int32_t>(result) >> shift_amount);
        } else { 
            // ASR >= 32 saturates the register with the sign bit (bit 31)
            carry = (result >> 31) & 1;
            return (carry) ? 0xFFFFFFFF : 0;
        }
    
    case 0x3: // ROR (Rotate Right)
    {
        // ROR by amounts >= 32 simply wraps around (e.g., ROR #33 is identical to ROR #1)
        const uint8_t rotate = shift_amount & 31u;
        if (rotate == 0) {
            carry = (result >> (rotate - 1)) & 1; // If it lands perfectly on a multiple of 32, carry is bit 31
            return result;
        }
        
        // Carry becomes the last bit rotated out of the right side, 
        // which ends up becoming the new sign bit (bit 31) of the result.
        uint32_t shifted_result = (result >> rotate) | (result << (32u - rotate));
        carry = (shifted_result >> 31) & 1;
        return shifted_result;
    }

    default:
        unreachable();
    }
}

uint32_t ArmCpu::special_shift(uint32_t rm_value, uint8_t shift_type, bool& cpsr_carry) {
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
            return rm_value;

        case 0x1: // LSR #0 (Interpreted as LSR #32)
            cpsr_carry = (rm_value >> 31) & 1; // Carry gets bit 31
            return 0; // Value becomes 0

        case 0x2: // ASR #0 (Interpreted as ASR #32)
            cpsr_carry = (rm_value >> 31) & 1; // Carry gets bit 31
            // If bit 31 is 1, return 0xFFFFFFFF, else return 0
            return (cpsr_carry) ? 0xFFFFFFFF : 0;

        case 0x3: // ROR #0 (Interpreted as RRX #1)
        {
            bool old_carry = cpsr_carry;
            cpsr_carry = rm_value & 1;// Carry gets bit 0
            
            // Slide Rm right by 1, and place the old carry into bit 31
            return (rm_value >> 1) | (static_cast<uint32_t>(old_carry) << 31);
        }
        default:
            return rm_value;
    }
}

uint32_t ArmCpu::shift_handler(bool bit_4, uint8_t Rs_or_imm, uint8_t register_to_shift, uint8_t shift_type, bool& carry){
    uint32_t register_value = registers[register_to_shift];
    if (bit_4) // 1 = Shift by Register 
    {
        uint8_t shift_amout_from_register = registers[Rs_or_imm] & 0xFF; // only lower 8bit 0-255 used
        return shift(register_value, shift_type, shift_amout_from_register, carry);
    }

    // Shift by Immediate
    if (Rs_or_imm != 0)
        return shift(register_value, shift_type, Rs_or_imm, carry);

    // In case Zero Shift Amount (Shift Register by Immediate, with Immediate=0)
    return special_shift(register_value, shift_type, carry);
}

//////////////////

uint32_t decode_immediate_operand(uint32_t opcode, bool& carry, bool update_carry) {
    uint32_t imm8 = opcode & 0xFF;
    uint8_t rotate = ((opcode >> 8) & 0xF) * 2;

    if (rotate == 0) {
        // If rotate is 0, carry is left untouched by the shifter 
        // (unless overridden later by an arithmetic operation)
        return imm8;
    }

    // Perform 32-bit Right Rotate (ROR)
    uint32_t op2 = (imm8 >> rotate) | (imm8 << (32 - rotate));

    // If the instruction updates flags, update the carry flag to bit 31
    if (update_carry) {
        carry = (op2 >> 31) & 1;
    }

    return op2;
}
// A template function generates distinct, optimized code per operation at compile-time!
template <OpcodeType Operation>
static void handler_data_processing(ArmCpu& cpu, uint32_t opcode) {
    bool set_condition_codes = (opcode >> 20) & 0x1;
    uint8_t Rn = (opcode >> 16) & 0xF;
    uint8_t Rd = (opcode >> 12) & 0xF;
    uint8_t Rm = opcode & 0xF;
    bool shift_by_register = (opcode >> 4) & 0x1;
    uint8_t shift_type = (opcode >> 5) & 0x3;

    bool carry = (cpu.cpsr >> 29) & 1;
    uint32_t carry_in = (cpu.cpsr >> 29) & 1;
    bool carry_out;
    uint32_t op1 = cpu.registers[Rn];
    uint32_t op2;

    if (shift_by_register) {
        uint8_t Rs = (opcode >> 8) & 0xF;
        op2 = cpu.shift_handler(shift_by_register, Rs, Rm, shift_type, carry);
    } else {
        uint8_t Is = (opcode >> 7) & 0x1F;
        op2 = cpu.shift_handler(shift_by_register, Is, Rm, shift_type, carry);
    }

    uint32_t result = 0;
    bool overflow = (cpu.cpsr >> 28) & 1; // default to current V flag

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
        overflow = ((op1 ^ result) & (op2 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::SBC) {
        result = op1 + ~op2 + carry_in - 1;
        carry  = (uint64_t(op1) + ~op2 + carry_in) > 0xFFFFFFFF;
        overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::RSC) {
        result = op2 + ~op1 + carry_in - 1;
        carry  = (uint64_t(op2) + ~op1 + carry_in) > 0xFFFFFFFF;
        overflow = ((op1 ^ op2) & (op2 ^ result)) >> 31;
    }
    else if constexpr (Operation == OpcodeType::TST) { result = op1 & op2; }
    else if constexpr (Operation == OpcodeType::TEQ) { result = op1 ^ op2; }
    else if constexpr (Operation == OpcodeType::ORR) { result = op1 | op2; }
    else if constexpr (Operation == OpcodeType::MOV) { result = op2; }
    else if constexpr (Operation == OpcodeType::BIC) { result = op1 & (~op2); }
    else if constexpr (Operation == OpcodeType::MVN) { result = ~op2; }

    // TST, TEQ, CMP, and CMN are "test" instructions; they update flags but don't save results.
    constexpr bool is_test_instruction = (Operation == OpcodeType::TST || Operation == OpcodeType::TEQ || 
                                          Operation == OpcodeType::CMP || Operation == OpcodeType::CMN);

    if constexpr (!is_test_instruction) { 
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

    cpu.registers[Rd] = result;
    }

    // make an inline function with this ->
    if (set_condition_codes || is_test_instruction) {
        bool n_flag = (result >> 31) & 1;
        bool z_flag = (result == 0);
        
        cpu.cpsr &= 0x0FFFFFFF; // Clear N, Z, C, V bits
        cpu.cpsr |= (static_cast<uint32_t>(n_flag) << 31);
        cpu.cpsr |= (static_cast<uint32_t>(z_flag) << 30);
        cpu.cpsr |= (static_cast<uint32_t>(carry) << 29);
        cpu.cpsr |= (static_cast<uint32_t>(overflow) << 28);
    }
}