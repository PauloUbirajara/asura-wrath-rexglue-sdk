/**
 * @file        rex/core/fiber_android.cpp
 * @brief       Android backend for rex::thread::Fiber
 *
 * The stack switch is per CPU (fiber_android.h).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/platform.h>
#if REX_PLATFORM_ANDROID

#include <rex/thread/fiber.h>

#include <cassert>
#include <cstdlib>

#include "fiber_android.h"

namespace rex::thread {

thread_local Fiber* Fiber::tls_current_ = nullptr;

Fiber* Fiber::ConvertCurrentThread() {
  auto* f = new Fiber();
  f->is_thread_fiber_ = true;  // sp_ is filled by the first SwitchTo away
  tls_current_ = f;
  return f;
}

Fiber* Fiber::Create(size_t stack_size, void (*entry)(void*), void* arg) {
  auto* f = new Fiber();
  f->entry_ = entry;
  f->arg_ = arg;
  f->stack_.resize(stack_size);
  f->sp_ = detail::FiberInitialStack(f->stack_.data(), f->stack_.size(), &Fiber::Trampoline);
  return f;
}

/*static*/ void Fiber::Trampoline() {
  // tls_current_ was set by SwitchTo before the stack switch.
  Fiber* f = tls_current_;
  f->entry_(f->arg_);
  // Fiber entries must not return (as on the other backends, nothing to go to).
  std::abort();
}

void Fiber::SwitchTo(Fiber* target) {
  Fiber* from = tls_current_;
  tls_current_ = target;
  rex_fiber_switch(&from->sp_, target->sp_);
}

void Fiber::Destroy() {
  if (is_thread_fiber_) {
    tls_current_ = nullptr;
  } else {
    assert(this != tls_current_ && "Destroy called on the currently running fiber");
  }
  delete this;
}

}  // namespace rex::thread

#endif  // REX_PLATFORM_ANDROID
