#ifndef THREAD_H
#define THREAD_H

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

int create_thread(unsigned long int *thread_id, void **h_thread, void* (*thread_func)(), void *args);

#endif