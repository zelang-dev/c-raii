#define USE_CORO
#include "raii.h"

void *worker(params_t args) {
    int wid = args[0].integer + 1;
    int id = coro_id();

    printf("Worker %d starting\n", wid);
    coro_info_active();
    sleepfor(1000);
    printf("Worker %d done\n", wid);
    coro_info_active();

    if (id == 4)
        return $$$(32);
    else if (id == 3)
        return "hello world";

    return 0;
}

int main(int argc, char **argv) {
    int cid[50], i;

    waitgroup_t wg = waitgroup();
    for (i = 0; i < 50; i++) {
        cid[i] = go(worker, 1, i);
    }
    waitresult_t wgr = waitfor(wg);

    printf("\nWorker # %d returned: %d\n", cid[3], get_result(cid[3]).integer);
    printf("Worker # %d returned: %s\n", cid[2], get_result(cid[2]).char_ptr);
    printf("\nindex # %d : %d\n", 1, wgr[1].integer);
    printf("index # %d : %s\n", 0, wgr[0].char_ptr);
    return 0;
}
