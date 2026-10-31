#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

inline constexpr std::size_t BIOS_SIZE = 0x4000;        // 16 KB
inline constexpr std::size_t BOARD_WRAM_SIZE = 0x40000; // 256 KB
inline constexpr std::size_t CHIP_WRAM_SIZE = 0x8000;   // 32 KB
inline constexpr std::size_t VRAM_SIZE = 0x18000;       // 96 KB


class Bus {
    public:
        ~Bus();
        std::array<uint8_t, BIOS_SIZE> rom;
        std::array<uint8_t, BOARD_WRAM_SIZE> ram;
        std::array<uint8_t, VRAM_SIZE> video_ram;
    private:
        Bus();
};