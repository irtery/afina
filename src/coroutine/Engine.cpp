#include <afina/coroutine/Engine.h>

#include <algorithm>
#include <cstring>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

/**
 * Save stack of the current coroutine in the given context
 */
void Engine::Store(context &ctx) {
    char current_stack_address;

    ctx.Low = std::min(&current_stack_address, StackBottom);
    ctx.High = std::max(&current_stack_address, StackBottom);

    char *&stack = std::get<0>(ctx.Stack);
    auto &allocated_size = std::get<1>(ctx.Stack);
    auto required_size = ctx.High - ctx.Low;

    if (allocated_size < required_size) {
        delete[] stack;
        stack = new char[required_size];
        std::memcpy(stack, ctx.Low, required_size);
        ctx.Stack = std::make_tuple(stack, required_size);
    }
}

/**
 * Restore stack of the given context and pass control to coroutine
 */
void Engine::Restore(context &ctx) {
    char current_stack_address;

    if ((ctx.Low <= &current_stack_address) && (&current_stack_address <= ctx.High)) {
        Restore(ctx);
    }

    memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    cur_routine = &ctx;
    longjmp(ctx.Environment, 1);
}

/**
 * Gives up current routine execution and let engine to schedule other one. It is not defined when
 * routine will get execution back, for example if there are no other coroutines then executing could
 * be transferred back immediately (yield turns to be noop).
 *
 * Also there are no guarantee what coroutine will get execution, it could be caller of the current one or
 * any other which is ready to run
 */
void Engine::yield() {
    context *new_routine = alive;

    if (new_routine && (new_routine == cur_routine)) {
        new_routine = new_routine->next;
    }

    if (new_routine) {
        sched(new_routine);
    }
}

/**
 * Suspend current coroutine execution and execute given context
 */
void Engine::Enter(context &ctx) {
    if (cur_routine && (cur_routine != idle_ctx)) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }

        Store(*cur_routine);
    }

    Restore(ctx);
}

/**
 * Suspend current routine and transfers control to the given one, resumes its execution from the point
 * when it has been suspended previously.
 *
 * If routine to pass execution to is not specified runtime will try to transfer execution back to caller
 * of the current routine, if there is no caller then this method has same semantics as yield
 */
void Engine::sched(void *routine_) {
    if (cur_routine == routine_) {
        return;
    }

    if (routine_) {
        Enter(*(static_cast<context *>(routine_)));
    } else {
        yield();
    }
}

} // namespace Coroutine
} // namespace Afina
