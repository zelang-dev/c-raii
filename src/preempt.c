/*
 * Modified from https://github.com/sysprog21/concurrent-programs/blob/master/preempt_sched/task_sched.c
 *
 * TODO: Preemptive multitasking in userspace based on SIGALRM signal or SetTimer under Windows,
 * simulating an OS timer interrupt.
 *
 * Execution of each coroutine can be preempted by SetTimer/SIGALRM signal,
 * each coroutine is an execution context, which can do a voluntary scheduling or be preempted by a timer,
 * and in that case nonvoluntary scheduling occurs.
 *
 * The default time slice is 10ms, that means that each 10ms SIGALRM/SetTimer fires and
 * next context is scheduled by round robin algorithm.
 */
#include "raii.h"

#ifdef _WIN32
static CRITICAL_SECTION signal_ctrl = {0};
static RAII_INLINE void signal_block(void) {
    EnterCriticalSection(&signal_ctrl);
}

static RAII_INLINE void signal_unblock(void) {
    LeaveCriticalSection(&signal_ctrl);
}
#else
static sigset_t signal_ctrl;
static RAII_INLINE void signal_block(void) {
    sigset_t block_set;
    sigfillset(&block_set);
    sigdelset(&block_set, SIGINT);
    sigprocmask(SIG_BLOCK, &block_set, &signal_ctrl);
}

static RAII_INLINE void signal_unblock(void) {
    sigprocmask(SIG_SETMASK, &signal_ctrl, NULL);
}
#endif

static volatile sig_atomic_t preempt_count = 0;
static void schedule(void) {
}

RAII_INLINE void preempt_disable(void) {
    preempt_count++;
}

RAII_INLINE void preempt_enable(void) {
    preempt_count--;
}

#ifdef _WIN32
static UINT TimerId;
static uintptr_t hThreadId;
static VOID CALLBACK preempt_handler(HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime)
#else
static void preempt_handler(int signo, siginfo_t *info)
#endif
{
    if (preempt_count) /* once preemption is disabled */
        return;

    signal_block();
    /* We can schedule directly from sighandler because Linux kernel cares only
     * about proper sigreturn frame in the stack.
     */
    schedule();
    signal_unblock();
}

#ifdef _WIN32
static int preempt_thread(void *args) {
    u32 ms = *(u32 *)args;
    MSG Msg;

    if (SetUserObjectInformationW(GetCurrentProcess(), UOI_TIMERPROC_EXCEPTION_SUPPRESSION, FALSE, 0)
        && (TimerId = SetTimer(NULL, 0, ms, (TIMERPROC)&preempt_handler)))
        while (GetMessage(&Msg, NULL, 0, 0))
            DispatchMessage(&Msg);

    DeleteCriticalSection(&signal_ctrl);
    return TimerId ? 0 : 16;
}

void preempt_init(u32 usecs) {
    InitializeCriticalSection(&signal_ctrl);
    hThreadId = _beginthread((_beginthread_proc_type)preempt_thread, 0, &usecs);
}

void preempt_stop(void) {
    int exit = KillTimer(NULL, TimerId);
    PostQuitMessage(exit ? 0 : GetLastError());
}
#else
void preempt_init(u32 usecs) {
    struct itimerval timer;
    struct sigaction sa = {
        .sa_handler = (void (*)(int))preempt_handler,
        .sa_flags = SA_SIGINFO,
    };
    sigfillset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    /* Configure the timer to expire after `usecs` msec... */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = usecs * 1000;
    /* ... and every `usecs` msec after that. */
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = usecs * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);
}

void preempt_stop(void) {
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
}
#endif
