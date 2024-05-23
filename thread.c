#include "thread.h"

int create_thread(unsigned long int *thread_id, void **h_thread, void* (*thread_func)(), void *args) {
    int rv = 0;
#ifdef _WIN32
    HANDLE handle_thread;
    handle_thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)thread_func, 0, 0, thread_id);
    if(handle_thread != NULL) {
        *h_thread = handle_thread;
        rv = 1;
    }
#else
    rv = pthread_create(thread_id, NULL, thread_func, NULL);
    if(rv == 0)
        rv = 1;
    else
        rv = 0;
#endif
    return rv;
}