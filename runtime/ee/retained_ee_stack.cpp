#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "retained_ee_stack.h"
#include <exception>
#include <stdexcept>
#include <utility>

struct RetainedEeStack::Impl {
    struct Cancel {};
    std::function<void()> entry;
    void* fiber = nullptr;
    void* parent = nullptr;
    DWORD owner = GetCurrentThreadId();
    State state = State::Ready;
    bool cancel = false;
    std::exception_ptr error;
    static void WINAPI run(void* pointer) noexcept {
        auto& self = *static_cast<Impl*>(pointer);
        try {
            self.entry();
            self.state = State::Completed;
        } catch (const Cancel&) {
            self.state = State::Cancelled;
        } catch (...) {
            self.error = std::current_exception();
            self.state = State::Failed;
        }
        SwitchToFiber(self.parent);
        // A terminal stack must never be scheduled again.
        std::terminate();
    }
};
RetainedEeStack::RetainedEeStack(std::function<void()> entry) : impl_(std::make_unique<Impl>()) {
    if (!entry) throw std::invalid_argument("Missing EE stack entry");
    impl_->entry = std::move(entry);
    impl_->fiber = CreateFiberEx(64 * 1024, 4 * 1024 * 1024, FIBER_FLAG_FLOAT_SWITCH, Impl::run, impl_.get());
    if (!impl_->fiber) throw std::runtime_error("Cannot allocate retained EE stack");
}
RetainedEeStack::~RetainedEeStack() {
    if (impl_->owner != GetCurrentThreadId() || impl_->state == State::Running) std::terminate();
    if (impl_->state == State::Waiting) {
        impl_->cancel = true;
        try { resume(); } catch (...) { std::terminate(); }
        if (impl_->state != State::Cancelled) std::terminate();
    }
    DeleteFiber(impl_->fiber);
}
void RetainedEeStack::resume() {
    auto& self = *impl_;
    if (self.owner != GetCurrentThreadId() || (self.state != State::Ready && self.state != State::Waiting))
        throw std::logic_error("Invalid retained EE dispatch");
    const bool converted = !IsThreadAFiber();
    self.parent = converted ? ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH) : GetCurrentFiber();
    if (!self.parent) throw std::runtime_error("Cannot retain EE parent stack");
    self.state = State::Running;
    SwitchToFiber(self.fiber);
    if (converted && !ConvertFiberToThread()) std::terminate();
    if (self.error) std::rethrow_exception(self.error);
}
void RetainedEeStack::yield() {
    auto& self = *impl_;
    if (self.owner != GetCurrentThreadId() || self.state != State::Running || GetCurrentFiber() != self.fiber)
        throw std::logic_error("EE yield outside owned stack");
    self.state = State::Waiting;
    SwitchToFiber(self.parent);
    if (self.cancel) throw Impl::Cancel{};
}
RetainedEeStack::State RetainedEeStack::state() const { return impl_->state; }
