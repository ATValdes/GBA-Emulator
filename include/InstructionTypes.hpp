#pragma once

#include <cstdint>
#include <functional>

class ArmCpu;

using ArmInstructionHandler = void(*)(ArmCpu&, uint32_t);
// using ArmInstructionHandler = std::function<void(ArmCpu&, uint32_t)>;