// Bus.cpp
#include "Bus.hpp"

Bus::Bus() : rom{}, ram{}, video_ram{} {}   // zero-initialise the memory
Bus::~Bus() = default;