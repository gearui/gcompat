/* SPDX-License-Identifier: NCSA
 * AArch64 glibc static recursive/errorcheck initializer adaptation.
 * Dynamic initialization already goes through musl pthread entry points.
 * This is not a general bridge for glibc robust/process-shared mutex layouts.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#if !defined(__aarch64__)
#error Static mutex adaptation requires independent platform validation
#endif
static int adaptation_lock;
static int prepare(pthread_mutex_t *mutex)
{
    /* Read no more than musl's public object size. glibc kind is byte 16;
     * all other bytes of these static initializers are zero. musl puts
     * type at byte zero. Never reinterpret an already active object. */
    while (__atomic_exchange_n(&adaptation_lock, 1, __ATOMIC_ACQUIRE)) sched_yield();
    unsigned char bytes[sizeof *mutex];
    for (size_t i=0; i<sizeof bytes; ++i)
        bytes[i]=__atomic_load_n((unsigned char *)mutex+i,__ATOMIC_RELAXED);
    int kind; memcpy(&kind, bytes + 16, sizeof kind);
    int candidate = kind == 1 || kind == 2;
    for (size_t i=0; i<sizeof bytes; ++i)
        if ((i<16 || i>=20) && bytes[i]) candidate=0;
    int result=0;
    if (candidate) {
        pthread_mutexattr_t attr;
        result=pthread_mutexattr_init(&attr);
        if (!result) {
            result=pthread_mutexattr_settype(&attr,kind==1?PTHREAD_MUTEX_RECURSIVE:PTHREAD_MUTEX_ERRORCHECK);
            if (!result) result=pthread_mutex_init(mutex,&attr);
            pthread_mutexattr_destroy(&attr);
        }
    }
    __atomic_store_n(&adaptation_lock,0,__ATOMIC_RELEASE);
    return result;
}
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    int (*real)(pthread_mutex_t *)=dlsym(RTLD_NEXT,"pthread_mutex_lock");
    if (!real) return ENOSYS;
    int result=prepare(mutex);return result?result:real(mutex);
}
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    int (*real)(pthread_mutex_t *)=dlsym(RTLD_NEXT,"pthread_mutex_trylock");
    if (!real) return ENOSYS;
    int result=prepare(mutex);return result?result:real(mutex);
}
int pthread_mutex_timedlock(pthread_mutex_t *mutex,const struct timespec *deadline)
{
    int (*real)(pthread_mutex_t *,const struct timespec *)=dlsym(RTLD_NEXT,"pthread_mutex_timedlock");
    if (!real) return ENOSYS;
    int result=prepare(mutex);return result?result:real(mutex,deadline);
}
