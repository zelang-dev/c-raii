cmake_minimum_required(VERSION 2.8...3.14)

set(TARGET_LIST defer_thread
 throw_no_try_protected
 throw_no_try
 try_catch_any
 try_catch_if
 try_catch_if_sigsegv
 try_catch_raise
 try_catch
 try_double_rethrow
 try_finally
 try_free_protected
 try_protect
 try_protected
 try_rethrow
 try_signal_sigfpe
 try_signal_sigsegv
 try_unprotected
 work-steal
 thrd_async
 # thrd_spawn_fib
 benchmark
 map_insert
 go_reflection
 go_multi_args
 go_panic
 go_sleepfor
 go_waitgroup
 go_channel
 go_select
 go_future_wait
 go_slice_ref
 generator
 fib )
foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c )
    target_link_libraries(${TARGET} raii)
endforeach()
