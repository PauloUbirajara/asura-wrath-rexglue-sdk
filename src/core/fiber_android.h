/**
 * @file        rex/core/fiber_android.h
 * @brief       Per-architecture stack switch behind the Android fibers
 *
 * Bionic has no getcontext/makecontext/swapcontext, so fibers switch stacks
 * directly. fiber_android.cpp is the Fiber logic; the CPU-specific part (the
 * switch and a new fiber's first frame) is in fiber_android_<arch>.cpp, which
 * CMake picks by ABI.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#pragma once

#include <cstddef>

// Saves the callee-saved registers on the current stack, stores the stack
// pointer in *save_sp, then switches to load_sp and restores the registers
// saved there (by an earlier switch, or by FiberInitialStack).
extern "C" void rex_fiber_switch(void** save_sp, void* load_sp);

namespace rex::thread::detail {

// Lays out a frame at the top of [stack, stack + size) that the first
// rex_fiber_switch to it "returns" into `entry` (which never returns), with
// the stack aligned as a function entry expects. Returns its stack pointer.
void* FiberInitialStack(void* stack, size_t size, void (*entry)());

}  // namespace rex::thread::detail
