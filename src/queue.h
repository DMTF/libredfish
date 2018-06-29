//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _QUEUE_H_
#define _QUEUE_H_

#ifdef _MSC_VER
//Windows
#include <windows.h>
#define mutex             SRWLOCK
#define condition         CONDITION_VARIABLE
#define thread            HANDLE
#define threadRet         DWORD

#define mutex_init        InitializeSRWLock
#define mutex_lock        AcquireSRWLockExclusive
#define mutex_unlock      ReleaseSRWLockExclusive
#define mutex_destroy(m)

#define cond_init         InitializeConditionVariable
#define cond_signal       WakeConditionVariable
#define cond_broadcast    WakeAllConditionVariable
#define cond_wait(c, m)   SleepConditionVariableSRW((c), (m), INFINITE, 0)
#define cond_destroy(c)

inline bool cas(void* ptr, size_t comp, size_t replace)
{
#if _WIN64
    return (InterlockedCompareExchange64(ptr, replace, comp) == comp);
#else
    return (InterlockedCompareExchange(ptr, replace, comp) == comp);
#endif
}
#else
#include <pthread.h>
#define mutex             pthread_mutex_t
#define condition         pthread_cond_t
#define thread            pthread_t
#define threadRet         void*

#define mutex_init(m)     pthread_mutex_init((m), NULL)
#define mutex_lock        pthread_mutex_lock
#define mutex_unlock      pthread_mutex_unlock
#define mutex_destroy     pthread_mutex_destroy

#define cond_init(c)      pthread_cond_init((c), NULL)
#define cond_broadcast    pthread_cond_broadcast
#define cond_wait         pthread_cond_wait
#define cond_destroy      pthread_cond_destroy

#define cas               __sync_bool_compare_and_swap 
#endif

typedef struct _queue_node
{
    void*               value;
    struct _queue_node* next;
} queueNode;

typedef struct _queue
{
    queueNode* first;
    queueNode* divider;
    queueNode* last;

    mutex      pushLock;
    mutex      popLock;

    condition  pushed;
} queue;

queue* newQueue();
void freeQueue(queue* q);
unsigned int queuePush(queue* q, void* value);
unsigned int queuePop(queue* q, void** value);
unsigned int queuePopNoWait(queue* q, void** value);

#endif
/* vim: set tabstop=4 shiftwidth=4 expandtab: */
