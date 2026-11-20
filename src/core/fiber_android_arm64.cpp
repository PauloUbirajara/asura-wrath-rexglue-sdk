/**
 * @file        rex/core/fiber_android_arm64.cpp
 * @brief       arm64 stack switch for the Android fibers (fiber_android.h)
 *
 * The callee-saved registers (AAPCS64: x19-x30, d8-d15) are pushed on the
 * current stack, the stack pointer is swapped, and the target's are popped.
 * x18 is Android's platform register (shadow call stack) and stays untouched.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include "fiber_android.h"

#include <cstdint>
#include <cstring>

asm(R"(
  .text
  .p2align 4
  .global rex_fiber_switch
  .hidden rex_fiber_switch
  .type rex_fiber_switch, %function
rex_fiber_switch:
  sub sp, sp, #160
  stp x19, x20, [sp, #0]
  stp x21, x22, [sp, #16]
  stp x23, x24, [sp, #32]
  stp x25, x26, [sp, #48]
  stp x27, x28, [sp, #64]
  stp x29, x30, [sp, #80]
  stp d8, d9, [sp, #96]
  stp d10, d11, [sp, #112]
  stp d12, d13, [sp, #128]
  stp d14, d15, [sp, #144]
  mov x2, sp
  str x2, [x0]
  mov sp, x1
  ldp x19, x20, [sp, #0]
  ldp x21, x22, [sp, #16]
  ldp x23, x24, [sp, #32]
  ldp x25, x26, [sp, #48]
  ldp x27, x28, [sp, #64]
  ldp x29, x30, [sp, #80]
  ldp d8, d9, [sp, #96]
  ldp d10, d11, [sp, #112]
  ldp d12, d13, [sp, #128]
  ldp d14, d15, [sp, #144]
  add sp, sp, #160
  ret
  .size rex_fiber_switch, .-rex_fiber_switch
)");

namespace rex::thread::detail {

void* FiberInitialStack(void* stack, size_t size, void (*entry)()) {
  constexpr size_t kFrameSize = 160;  // as pushed by rex_fiber_switch
  uintptr_t top = (reinterpret_cast<uintptr_t>(stack) + size) & ~uintptr_t(15);
  auto* frame = reinterpret_cast<uint64_t*>(top - kFrameSize);
  std::memset(frame, 0, kFrameSize);
  frame[10] = 0;                                    // x29: end of the frame chain
  frame[11] = reinterpret_cast<uint64_t>(entry);  // x30: where the ret goes
  return frame;  // sp is 16-byte aligned after the pop, as entry expects
}

}  // namespace rex::thread::detail
