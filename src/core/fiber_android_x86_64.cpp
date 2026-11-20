/**
 * @file        rex/core/fiber_android_x86_64.cpp
 * @brief       x86_64 stack switch for the Android fibers (fiber_android.h)
 *
 * The callee-saved registers (System V: rbx, rbp, r12-r15) are pushed on the
 * current stack, the stack pointer is swapped, and the target's are popped.
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
  .type rex_fiber_switch, @function
rex_fiber_switch:
  pushq %rbp
  pushq %rbx
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15
  movq %rsp, (%rdi)
  movq %rsi, %rsp
  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %rbx
  popq %rbp
  ret
  .size rex_fiber_switch, .-rex_fiber_switch
)");

namespace rex::thread::detail {

void* FiberInitialStack(void* stack, size_t size, void (*entry)()) {
  uintptr_t top = (reinterpret_cast<uintptr_t>(stack) + size) & ~uintptr_t(15);
  // From the top down: a null return address for entry (it never returns),
  // the address the switch's ret goes to, then the six registers it pops.
  // After the ret, rsp is 8 mod 16, as at any function's entry.
  auto* frame = reinterpret_cast<uint64_t*>(top - 8 * 8);
  std::memset(frame, 0, 8 * 8);
  frame[6] = reinterpret_cast<uint64_t>(entry);
  return frame;
}

}  // namespace rex::thread::detail
