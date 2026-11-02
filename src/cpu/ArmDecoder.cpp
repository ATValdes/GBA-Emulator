#include "ArmDecoder.hpp"

#include "ArmCpu.hpp"
#include "DataProcessing.hpp"

static constexpr uint16_t get_index(uint32_t opcode) {
    // Extract bits 27-20
    uint16_t op_high = (opcode >> 16) & 0x0FF0; 
    
    // Extract bits 7-4
    uint16_t op_low = (opcode >> 4) & 0x000F;
    
    // Merge them into a single 12-bit identifier (0x000 to 0xFFF)
    return op_high | op_low;
}

void ArmDecoder::initialize_lut() {
    arm_lut.fill(&ArmCpu::fallback_unimplemented);

    // ALU - Registers Data Processing Instructions
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
                if ((bits_7_4 & 0x9)== 0x9)
                    continue;

                // Bits 27-26 son 0, Bit 25 es 0.
                uint16_t index = (static_cast<uint16_t>(bits_24_20) << 4) | bits_7_4;

                // Asignar dinámicamente usando metaprogramación de plantillas (templates)
                arm_lut[index] = DataProcessing::get_data_proc_handler(static_cast<DataProcessing::OpcodeType>(opcode), 0);
            }
        }
    }

    // ALU - Immediate Data Processing Instructions
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
                if ((bits_7_4 & 0x9)== 0x9)
                    continue;

                // Bits 27-26 son 0, Bit 25 es 1.
                uint16_t index = 1 << 9 | (static_cast<uint16_t>(bits_24_20) << 4) | bits_7_4;

                // Asignar dinámicamente usando metaprogramación de plantillas (templates)
                arm_lut[index] = DataProcessing::get_data_proc_handler(static_cast<DataProcessing::OpcodeType>(opcode), 1);
            }
        }
    }
}