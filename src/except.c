#if defined(__GNUC__) || !defined(_WIN32)
#undef _FORTIFY_SOURCE
#endif
#include "raii.h"

/* Some common exception */
EX_EXCEPTION(invalid_type);
EX_EXCEPTION(range_error);
EX_EXCEPTION(divide_by_zero);
EX_EXCEPTION(logic_error);
EX_EXCEPTION(future_error);
EX_EXCEPTION(system_error);
EX_EXCEPTION(domain_error);
EX_EXCEPTION(length_error);
EX_EXCEPTION(out_of_range);
EX_EXCEPTION(invalid_argument);
EX_EXCEPTION(division_by_zero);
EX_EXCEPTION(out_of_memory);
EX_EXCEPTION(panic);

EX_EXCEPTION(_if);

/* Some signal exception */
EX_EXCEPTION(sig_int);
EX_EXCEPTION(sig_abrt);
EX_EXCEPTION(sig_alrm);
EX_EXCEPTION(sig_bus);
EX_EXCEPTION(sig_fpe);
EX_EXCEPTION(sig_ill);
EX_EXCEPTION(sig_quit);
EX_EXCEPTION(sig_segv);
EX_EXCEPTION(sig_term);
EX_EXCEPTION(sig_trap);
EX_EXCEPTION(sig_hup);
EX_EXCEPTION(sig_break);
EX_EXCEPTION(sig_winch);
EX_EXCEPTION(sig_info);
EX_EXCEPTION(access_violation);
EX_EXCEPTION(array_bounds_exceeded);
EX_EXCEPTION(breakpoint);
EX_EXCEPTION(datatype_misalignment);
EX_EXCEPTION(flt_denormal_operand);
EX_EXCEPTION(flt_divide_by_zero);
EX_EXCEPTION(flt_inexact_result);
EX_EXCEPTION(flt_invalid_operation);
EX_EXCEPTION(flt_overflow);
EX_EXCEPTION(flt_stack_check);
EX_EXCEPTION(flt_underflow);
EX_EXCEPTION(illegal_instruction);
EX_EXCEPTION(in_page_error);
EX_EXCEPTION(int_divide_by_zero);
EX_EXCEPTION(int_overflow);
EX_EXCEPTION(invalid_disposition);
EX_EXCEPTION(priv_instruction);
EX_EXCEPTION(single_step);
EX_EXCEPTION(stack_overflow);
EX_EXCEPTION(invalid_handle);
EX_EXCEPTION(bad_alloc);

thrd_local(ex_context_t, except, NULL)

static volatile sig_atomic_t got_signal = false;
static volatile sig_atomic_t got_uncaught_exception = false;
static volatile sig_atomic_t got_ctrl_c = false;
static volatile sig_atomic_t can_terminate = true;

ex_setup_func exception_setup_func = NULL;
ex_unwind_func exception_unwind_func = NULL;
ex_terminate_func exception_ctrl_c_func = NULL;
ex_terminate_func exception_terminate_func = NULL;
bool exception_signal_set = false;

static void ex_handler(int sig);
#if !defined(_WIN32)
static struct sigaction ex_sig_sa = {0}, ex_sig_osa = {0};
#endif

enum {
    max_ex_sig = 32
};

static struct {
    const char *ex;
    int sig;
#ifdef _WIN32
    DWORD seh;
#endif
} ex_sig[max_ex_sig];

ex_ptr_t ex_protect_ptr(ex_ptr_t *const_ptr, void *ptr, void (*func)(void *)) {
    ex_context_t *ctx = ex_init();
    const_ptr->next = ctx->stack;
    const_ptr->func = func;
    const_ptr->ptr = ptr;
    ctx->stack = const_ptr;
    const_ptr->type = ex_protected_st;
    return *const_ptr;
}

void ex_unprotected_ptr(ex_ptr_t *const_ptr) {
    if (!is_except_empty() && is_type(const_ptr, ex_protected_st)) {
        const_ptr->type = RAII_ERR;
        ex_local()->stack = const_ptr->next;
    }
}

static RAII_INLINE bool is_except_root(ex_context_t *ctx) {
#ifdef emulate_tls
    return ctx == ex_local();
#else
    return ctx == &thrd_except_buffer;
#endif
}

static void ex_unwind_stack(ex_context_t *ctx) {
    ex_ptr_t *p = ctx->stack;
    void *temp = NULL;

    ctx->unstack = 1;

    if (ctx->is_unwind && exception_unwind_func) {
        exception_unwind_func(ctx->data);
    } else if (ctx->is_unwind && ctx->is_raii && ctx->data == (void *)raii_local()) {
        raii_deferred_clean();
    } else {
        while (p && p->type == ex_protected_st) {
            if ((got_uncaught_exception = (temp == *p->ptr) || is_inaccessible(*p->ptr))) {
                atomic_flag_clear(&gq_result.is_errorless);
                break;
            }

            if (*p->ptr) {
                p->type = RAII_ERR;
                p->func(*p->ptr);
            }

            temp = *p->ptr;
            p = p->next;
        }
    }

    ctx->unstack = 0;
    ctx->stack = 0;
}

static void ex_print(ex_context_t *exception, const char *message) {
    fflush(stdout);
#ifndef USE_DEBUG
    fprintf(stderr, "\nFatal Error: %s in function(%s)\n\n",
                  ((void *)exception->panic != NULL ? exception->panic : exception->ex), exception->function);
#else
    fprintf(stderr, "\n%s: %s\n", message, (exception->panic != NULL ? exception->panic : exception->ex));
    if (exception->file != NULL) {
        if (exception->function != NULL) {
            fprintf(stderr, "    thrown in %s at (%s:%d)\n\n", exception->function, exception->file, exception->line);
        } else {
            fprintf(stderr, "    thrown at %s:%d\n\n", exception->file, exception->line);
        }
    }
#endif
    fflush(stderr);
}

void ex_trace_set(ex_context_t *ex, void_t ctx) {
#if defined(USE_DEBUG)
#if defined(_WIN32)
    if (ctx == NULL)
        RtlCaptureContext((CONTEXT *)ex->backtrace->ctx);
    else
        memcpy(ex->backtrace->ctx, (CONTEXT *)ctx, sizeof(CONTEXT));

    ex->backtrace->process = GetCurrentProcess();
    ex->backtrace->thread = GetCurrentThread();
#else
    ex->backtrace->size = backtrace(ex->backtrace->ctx, EX_MAX_NAME_LEN);
#endif
#endif
}

void ex_backtrace(ex_backtrace_t *ex) { //Prints stack trace based on context record
#if defined(USE_DEBUG)
#if defined(_WIN32)
    CONTEXT             ctx, *c_ctx = (CONTEXT *)ex->ctx;
    memcpy(&ctx, (CONTEXT *)ex->ctx, sizeof(CONTEXT));
    HANDLE              process = ex->process;
    HANDLE              thread = ex->thread;
    DWORD64             TempAddress = (DWORD64)0;
    BOOL                result;
    HMODULE             hModule;

    STACKFRAME64        stack;
    ULONG               frame;
    DWORD64             displacement;

    DWORD disp;
    IMAGEHLP_LINE64 *line;

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    char module[EX_MAX_NAME_LEN];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

    memset(&stack, 0, sizeof(STACKFRAME64));
    displacement = 0;
    DWORD imageType;
#ifdef _M_IX86
    imageType = IMAGE_FILE_MACHINE_I386;
    stack.AddrPC.Offset = (*c_ctx).Eip;
    stack.AddrPC.Mode = AddrModeFlat;
    stack.AddrFrame.Offset = (*c_ctx).Ebp;
    stack.AddrFrame.Mode = AddrModeFlat;
    stack.AddrStack.Offset = (*c_ctx).Esp;
    stack.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_X64) || defined(_M_AMD64)
    imageType = IMAGE_FILE_MACHINE_AMD64;
    stack.AddrPC.Offset = (*c_ctx).Rip;
    stack.AddrPC.Mode = AddrModeFlat;
    stack.AddrFrame.Offset = (*c_ctx).Rsp;
    stack.AddrFrame.Mode = AddrModeFlat;
    stack.AddrStack.Offset = (*c_ctx).Rsp;
    stack.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
    imageType = IMAGE_FILE_MACHINE_IA64;
    stack.AddrPC.Offset = (*c_ctx).StIIP;
    stack.AddrPC.Mode = AddrModeFlat;
    stack.AddrFrame.Offset = (*c_ctx).IntSp;
    stack.AddrFrame.Mode = AddrModeFlat;
    stack.AddrBStore.Offset = (*c_ctx).RsBSP;
    stack.AddrBStore.Mode = AddrModeFlat;
    stack.AddrStack.Offset = (*c_ctx).IntSp;
    stack.AddrStack.Mode = AddrModeFlat;
#elif _M_ARM64
    imageType = IMAGE_FILE_MACHINE_ARM64;
    stack.AddrPC.Offset = (*c_ctx).Pc;
    stack.AddrPC.Mode = AddrModeFlat;
    stack.AddrFrame.Offset = (*c_ctx).Fp;
    stack.AddrFrame.Mode = AddrModeFlat;
    stack.AddrStack.Offset = (*c_ctx).Sp;
    stack.AddrStack.Mode = AddrModeFlat;
#else
#error "Platform not supported!"
#endif

    SymInitialize(process, NULL, TRUE); //load symbols

    for (frame = 0; ; frame++) {
        //get next call from stack
        result = StackWalk64
        (
            imageType,
            process,
            thread,
            &stack,
            &ctx,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL
        );

        if (!result) break;

        //get symbol name for address
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbol->MaxNameLen = MAX_SYM_NAME;
        SymFromAddr(process, (ULONG64)stack.AddrPC.Offset, &displacement, pSymbol);

        line = (IMAGEHLP_LINE64 *)malloc(sizeof(IMAGEHLP_LINE64));
        line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        //try to get line
        if (SymGetLineFromAddr64(process, stack.AddrPC.Offset, &disp, line)) {
            fprintf(stderr, "\tat %s in %s:%lu: address: 0x%0zx\033[0K\n", pSymbol->Name, line->FileName, line->LineNumber, pSymbol->Address);
        } else {
            //failed to get line
            if (pSymbol->Address != TempAddress)
                fprintf(stderr, "\tat %s, address 0x%0zx.\033[0K\n", pSymbol->Name, pSymbol->Address);

            TempAddress = pSymbol->Address;
            hModule = NULL;
            lstrcpyA(module, "");
            GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              (LPCTSTR)(stack.AddrPC.Offset), &hModule);

            //at least print module name
            if (hModule != NULL)GetModuleFileNameA(hModule, module, EX_MAX_NAME_LEN);

            if (!is_str_empty(module))
                fprintf(stderr, "in %s\033[0K\n", module);
        }

        free(line);
        line = NULL;
    }
#else
    fprintf(stderr, "backtrace() returned %d addresses:\n", (int)ex->size);
    backtrace_symbols_fd(ex->ctx, ex->size, STDERR_FILENO);
#endif
#endif
}

RAII_INLINE void ex_unwind_set(ex_context_t *ctx, bool flag) {
    ctx->is_unwind = flag;
}

RAII_INLINE void ex_data_set(ex_context_t *ctx, void *data) {
    ctx->data = data;
}

RAII_INLINE void *ex_data_get(ex_context_t *ctx, void *data) {
    return ctx->data;
}

void ex_swap_set(ex_context_t *ctx, void *data) {
    ctx->prev = ctx->data;
    ctx->data = data;
}

RAII_INLINE void ex_swap_reset(ex_context_t *ctx) {
    ctx->data = ctx->prev;
}

void ex_flags_reset(void) {
    got_signal = false;
    got_ctrl_c = false;
}

RAII_INLINE ex_context_t *ex_local(void) {
    thrd_local_return(ex_context_t *, except)
}

void ex_update(ex_context_t *context) {
#ifdef emulate_tls
        if (tss_set(thrd_except_tss, context) != thrd_success)
            raii_panic("Except `tss_set` failed!");
#else
        thrd_except_tls = context;
#endif
}

ex_context_t *ex_init(void) {
    ex_context_t *context = NULL;
    if (is_empty(context = ex_local())) {
        ex_signal_block(all);
        context = except();
        context->is_rethrown = false;
        context->is_guarded = false;
        context->is_raii = false;
        context->ex = NULL;
        context->panic = NULL;
        memset(context->backtrace->ctx, 0, sizeof(context->backtrace->ctx));
        context->caught = RAII_ERR;
        context->type = ex_context_st;
        ex_signal_unblock(all);
    }
    return context;
}

int ex_uncaught_exception(void) {
    return ex_init()->unstack;
}

void ex_terminate(void) {
    if (ex_local()->is_raii)
        raii_destroy();

    if (ex_uncaught_exception() || got_uncaught_exception)
        ex_print(ex_local(), "\nException during stack unwinding leading to an undefined behavior");
    else
        ex_print(ex_local(), "\nExiting with uncaught exception");

    if (got_ctrl_c && exception_ctrl_c_func) {
        got_ctrl_c = false;
        exception_ctrl_c_func();
    }

    if (can_terminate && exception_terminate_func) {
        can_terminate = false;
        exception_terminate_func();
    }

    if (got_signal)
        _Exit(EXIT_FAILURE);
    else
        exit(EXIT_FAILURE);
}

void ex_throw(string_t exception, string_t file, int line, string_t function, string_t message, ex_backtrace_t *dump) {
    ex_context_t *ctx = ex_init();

    if (ctx->unstack)
        ex_terminate();

    ex_signal_block(all);
    if (is_empty(dump))
        ex_trace_set(ctx, NULL);

    ctx->ex = exception;
    ctx->file = file;
    ctx->line = line;
    ctx->is_unwind = false;
    ctx->function = function;
    ctx->panic = message;

    if (exception_setup_func)
        exception_setup_func(ctx, ctx->ex, ctx->panic);
    else if (ctx->is_raii)
        raii_unwind_set(ctx, ctx->ex, ctx->panic);

    ex_unwind_stack(ctx);
    ex_signal_unblock(all);

    if (is_except_root(ctx))
        ex_terminate();

#ifdef _WIN32
    RaiseException(EXCEPTION_PANIC, 0, 0, NULL);
#endif
    ex_longjmp(ctx->buf, ctx->state | ex_throw_st);
}

#ifdef _WIN32
int catch_seh(const char *exception, DWORD code, struct _EXCEPTION_POINTERS *ep) {
    ex_context_t *ctx = ex_init();
    const char *ex = 0;
    bool found = false, signaled = (int)code < 0;
    int i;

    ex_trace_set(ctx, (void_t)ep->ContextRecord);
    ctx->state = ex_throw_st;
    if (!is_str_eq(ctx->ex, exception) && is_empty((void *)ctx->panic))
        return EXCEPTION_EXECUTE_HANDLER;
    else if (is_empty((void *)ctx->ex) && signaled) {
        ctx->panic = NULL;
        for (i = 0; i < max_ex_sig; i++) {
            if (ex_sig[i].seh == code) {
                found = true;
                ctx->ex = ex_sig[i].ex;
                break;
            }
        }

        if (!found)
            return EXCEPTION_EXECUTE_HANDLER;
    } else if (!is_str_eq(ctx->panic, exception) && !is_str_eq(ctx->ex, exception)) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    for (i = 0; i < max_ex_sig; i++) {
        if (found || ex_sig[i].seh == code
            || ctx->caught == ex_sig[i].seh
            || is_str_eq(ctx->ex, exception)
            ) {
            ctx->state = ex_throw_st;
            ctx->is_rethrown = true;
            if (got_signal || signaled) {
                if (is_str_eq(ctx->ex, exception))
                    ctx->ex = exception;
                else if (!found)
                    ctx->ex = ex_sig[i].ex;
            }

            if (exception_setup_func)
                exception_setup_func(ctx, ctx->ex, ctx->panic);
            else if (ctx->is_raii)
                raii_unwind_set(ctx, ctx->ex, ctx->panic);
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

int catch_filter_seh(DWORD code, struct _EXCEPTION_POINTERS *ep) {
    ex_context_t *ctx = ex_init();
    const char *ex = 0;
    int i;

    ex_trace_set(ctx, (void_t)ep->ContextRecord);
    if (code == EXCEPTION_PANIC) {
        ctx->state = ex_throw_st;
        return EXCEPTION_EXECUTE_HANDLER;
    }

    for (i = 0; i < max_ex_sig; i++) {
        if (ex_sig[i].seh == code || ctx->caught == ex_sig[i].seh) {
            ctx->state = ex_throw_st;
            ctx->is_rethrown = true;
            ctx->ex = ex_sig[i].ex;
            ctx->panic = NULL;
            ctx->file = "unknown";
            ctx->line = 0;
            ctx->function = NULL;

            if (exception_setup_func)
                exception_setup_func(ctx, ctx->ex, ctx->panic);
            else if (ctx->is_raii)
                raii_unwind_set(ctx, ctx->ex, ctx->panic);

            if (!ctx->is_guarded || ctx->is_raii)
                ex_unwind_stack(ctx);
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void ex_signal_seh(DWORD sig, const char *ex) {
    int i;

    for (i = 0; i < max_ex_sig; i++)
        if (!ex_sig[i].ex || ex_sig[i].seh == sig)
            break;

    if (i == max_ex_sig)
        fprintf(stderr,
            "Cannot install exception handler for signal no %d (%s), "
            "too many signal exception handlers installed (max %d)\n",
            sig, ex, max_ex_sig);
    else
        ex_sig[i].ex = ex, ex_sig[i].seh = sig;
}
#endif

void ex_handler(int sig) {
    const char *ex = NULL;
    int i;

    got_signal = true;
    if (sig == SIGINT)
        got_ctrl_c = true;

#ifdef _WIN32
    /*
     * Make signal handlers persistent.
     */
    if (signal(sig, ex_handler) == SIG_ERR)
        fprintf(stderr, "Cannot reinstall handler for signal no %d (%s)\n",
                sig, ex);
#endif

    for (i = 0; i < max_ex_sig; i++) {
        if (ex_sig[i].sig == sig) {
            ex = ex_sig[i].ex;
            ex_init()->caught = sig;
            break;
        }
    }

    ex_throw(ex, "unknown", 0, NULL, NULL, NULL);
}

void ex_signal_reset(int sig) {
#if defined(_WIN32) || defined(_WIN64)
    if (signal(sig, SIG_DFL) == SIG_ERR)
        fprintf(stderr, "Cannot install handler for signal no %d\n",
                sig);
#else
    /*
     * Make signal handlers persistent.
     */
    ex_sig_sa.sa_handler = SIG_DFL;
    if (sigemptyset(&ex_sig_sa.sa_mask) != 0)
        fprintf(stderr, "Cannot setup handler for signal no %d\n", sig);
    else if (sigaction(sig, &ex_sig_sa, NULL) != 0)
        fprintf(stderr, "Cannot restore handler for signal no %d\n", sig);
#endif
    exception_signal_set = false;
}

void ex_signal(int sig, const char *ex) {
    int i;

    for (i = 0; i < max_ex_sig; i++) {
        if (!ex_sig[i].ex || ex_sig[i].sig == sig)
            break;
    }

    if (i == max_ex_sig) {
        fprintf(stderr,
                "Cannot install exception handler for signal no %d (%s), "
                "too many signal exception handlers installed (max %d)\n",
                sig, ex, max_ex_sig);
        return;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (signal(sig, ex_handler) == SIG_ERR)
        fprintf(stderr, "Cannot install handler for signal no %d (%s)\n",
                      sig, ex);
    else
        ex_sig[i].ex = ex, ex_sig[i].sig = sig;
#else
    /*
     * Make signal handlers persistent.
     */
    ex_sig_sa.sa_handler = ex_handler;
    ex_sig_sa.sa_flags = SA_RESTART;
    if (sigemptyset(&ex_sig_sa.sa_mask) != 0)
        fprintf(stderr, "Cannot setup handler for signal no %d (%s)\n",
                      sig, ex);
    else if (sigaction(sig, &ex_sig_sa, NULL) != 0)
        fprintf(stderr, "Cannot install handler for signal no %d (%s)\n",
                sig, ex);
    else
        ex_sig[i].ex = ex, ex_sig[i].sig = sig;

    exception_signal_set = true;
#endif
}

void ex_signal_setup(void) {
#ifdef _WIN32
    ex_signal_seh(EXCEPTION_ACCESS_VIOLATION, EX_NAME(sig_segv));
    ex_signal_seh(EXCEPTION_ARRAY_BOUNDS_EXCEEDED, EX_NAME(array_bounds_exceeded));
    ex_signal_seh(EXCEPTION_FLT_DIVIDE_BY_ZERO, EX_NAME(sig_fpe));
    ex_signal_seh(EXCEPTION_INT_DIVIDE_BY_ZERO, EX_NAME(divide_by_zero));
    ex_signal_seh(EXCEPTION_ILLEGAL_INSTRUCTION, EX_NAME(sig_ill));
    ex_signal_seh(EXCEPTION_INT_OVERFLOW, EX_NAME(int_overflow));
    ex_signal_seh(EXCEPTION_STACK_OVERFLOW, EX_NAME(stack_overflow));
    ex_signal_seh(EXCEPTION_INVALID_HANDLE, EX_NAME(invalid_handle));
    ex_signal_seh(EXCEPTION_PANIC, EX_NAME(panic));
    ex_signal_seh(CONTROL_C_EXIT, EX_NAME(sig_int));
#endif
#ifdef SIGSEGV
    ex_signal(SIGSEGV, EX_NAME(sig_segv));
#endif

#ifdef SIGABRT
    ex_signal(SIGABRT, EX_NAME(sig_abrt));
#endif

#ifdef SIGFPE
    ex_signal(SIGFPE, EX_NAME(sig_fpe));
#endif

#ifdef SIGILL
    ex_signal(SIGILL, EX_NAME(sig_ill));
#endif

#ifdef SIGBREAK
    ex_signal(SIGBREAK, EX_NAME(sig_break));
#endif

#ifdef SIGINT
    ex_signal(SIGINT, EX_NAME(sig_int));
#endif

#ifdef SIGTERM
    ex_signal(SIGTERM, EX_NAME(sig_term));
#endif

#if !defined(_WIN32)
#ifdef SIGQUIT
    ex_signal(SIGQUIT, EX_NAME(sig_quit));
#endif

#ifdef SIGHUP
    ex_signal(SIGHUP, EX_NAME(sig_hup));
#endif

#ifdef SIGWINCH
    ex_signal(SIGWINCH, EX_NAME(sig_winch));
#endif
#ifdef SIGTRAP
    ex_signal(SIGTRAP, EX_NAME(sig_trap));
#endif
#endif

#ifdef SIGALRM
    ex_signal(SIGALRM, EX_NAME(sig_alrm));
#endif

#ifdef SIGBUS
    ex_signal(SIGBUS, EX_NAME(sig_bus));
#elif SIG_BUS
    ex_signal(SIG_BUS, EX_NAME(sig_bus));
#endif
}

void ex_signal_default(void) {
#ifdef SIGSEGV
    ex_signal_reset(SIGSEGV);
#endif

#if defined(SIGABRT)
    ex_signal_reset(SIGABRT);
#endif

#ifdef SIGFPE
    ex_signal_reset(SIGFPE);
#endif

#ifdef SIGILL
    ex_signal_reset(SIGILL);
#endif

#ifdef SIGBREAK
    ex_signal_reset(SIGBREAK);
#endif

#ifdef SIGINT
    ex_signal_reset(SIGINT);
#endif

#ifdef SIGTERM
    ex_signal_reset(SIGTERM);
#endif

#if !defined(_WIN32)
#ifdef SIGQUIT
    ex_signal_reset(SIGQUIT);
#endif

#ifdef SIGHUP
    ex_signal_reset(SIGHUP);
#endif

#ifdef SIGWINCH
    ex_signal_reset(SIGWINCH);
#endif
#ifdef SIGTRAP
    ex_signal_reset(SIGTRAP);
#endif
#endif

#ifdef SIGALRM
    ex_signal_reset(SIGALRM);
#endif

#ifdef SIGBUS
    ex_signal_reset(SIGBUS);
#elif SIG_BUS
    ex_signal_reset(SIG_BUS);
#endif
}

RAII_INLINE ex_error_t *try_updating_err(ex_error_t *err, ex_context_t *ex_err) {
#ifdef _WIN32
    err->name = (!is_empty((void *)ex_err->panic)) ? ex_err->panic : ex_err->ex;
#else
    err->name = ex_err->ex;
#endif
    err->file = ex_err->file;
    err->backtrace = ex_err->backtrace;
    err->line = ex_err->line;
    err->is_caught = ex_err->state == ex_catch_st;
    if (coro_is_valid())
        coro_scope()->err = (void_t)err->name;
    else if (!is_raii_empty())
        raii_local()->err = (void_t)err->name;

    return err;
}

RAII_INLINE void try_rethrow(ex_context_t *ex_err) {
#ifdef _WIN32
    if (ex_err->caught > -1 || ex_err->is_rethrown) {
        ex_err->is_rethrown = false;
        ex_longjmp(ex_err->buf, ex_err->state | ex_throw_st);
    }
#endif

    if (!is_empty(ex_err->backtrace) && is_except_root(ex_err->next))
        ex_backtrace(ex_err->backtrace);

    ex_throw(ex_err->ex, ex_err->file, ex_err->line, ex_err->function, ex_err->panic, ex_err->backtrace);
}

ex_jmp_buf *try_start(ex_stage acquire, ex_error_t *err, ex_context_t *ex_err) {
    if (!exception_signal_set)
        ex_signal_setup();

    err->stage = acquire;
    err->is_caught = false;
    ex_err->next = ex_init();
    ex_err->stack = 0;
    ex_err->ex = 0;
    ex_err->unstack = 0;
    ex_err->is_rethrown = false;
    /* global context updated */
    ex_update(ex_err);
    /* return/save jump location */
    return &ex_err->buf;
}

void try_finish(ex_context_t *ex_err) {
    /* deallocate this block and promote its outer block, global context updated */
    if (ex_init() == ex_err)
        ex_update(ex_err->next);

#ifdef _WIN32
    ex_err->caught = -1;
    ex_err->is_rethrown = false;
#endif

    /* deallocate or propagate its exception */
    if ((ex_err->state & ex_throw_st) != 0)
        try_rethrow(ex_err);
}

bool try_next(ex_error_t *err, ex_context_t *ex_err) {
    /* advance the block to the next stage */
    err->stage++;

    /* carry on until the block is DONE */
    if (err->stage < ex_done_st)
        return true;

    try_finish(ex_err);

    /* get out of the loop */
    return false;
}

template_t *trying(void_t with, raii_func_t tryFunc, raii_func_t catchFunc, final_func_t finalFunc) {
    template_t *value = calloc_local(1, sizeof(template_t));
    try {
        value->object = tryFunc(with);
    } catch_any {
        if (catchFunc)
            value->object = catchFunc(with);
    } finally {
        if (finalFunc)
            finalFunc(value);
    }

    return value;
}

bool try_catching(string type, ex_error_t *err, ex_context_t *ex_err) {
    /* check if this CATCH block can handle current exception */
    bool status = false;
    if (ex_err->state == ex_throw_st) {
        try_updating_err(err, ex_err);
        if (is_str_eq("_if", type)) {
            status = true;
        } else if (is_empty((void_t)type) || (is_str_eq(err->name, type))) {
            ex_err->state = ex_catch_st;
            status = true;
        }
    }

    return status;
}

RAII_INLINE bool try_finallying(ex_error_t *err, ex_context_t *ex_err) {
    if (err->stage == ex_final_st) {
        try_updating_err(err, ex_err);
        /* global context updated */
        ex_update(ex_err->next);

        return true;
    }

    return false;
}
