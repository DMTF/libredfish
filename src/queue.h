//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file queue.h
 * @author Patrick Boyd
 * @brief File containing the interface for the internal queue.
 *
 * This file explains the interface for the internal queue used to process async HTTP calls
 */
#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdbool.h>

#ifdef _MSC_VER
//Windows
#include <windows.h>
/** A mutex, in the cae of Windows systems a Shared R/W Lock is used **/
#define mutex             SRWLOCK
/** A condition, in the case of Windows systems a Windows condition variable is used **/
#define condition         CONDITION_VARIABLE
/** A thread, the OS specific representation of a thread **/
#define thread            HANDLE
/** A thread return type, the OS specific return type expected by a thread **/
#define threadRet         DWORD

/** Initialize a mutex **/
#define mutex_init        InitializeSRWLock
/** Lock a mutex **/
#define mutex_lock        AcquireSRWLockExclusive
/** Unlock a mutex **/
#define mutex_unlock      ReleaseSRWLockExclusive
/** Free/Destroy a mutex **/
#define mutex_destroy(m)

/** Initialize a condition **/
#define cond_init         InitializeConditionVariable
/** Signal a condition **/
#define cond_signal       WakeConditionVariable
/** Broadcast a condition **/
#define cond_broadcast    WakeAllConditionVariable
/** Wait for a condition signal/broadcast **/
#define cond_wait(c, m)   SleepConditionVariableSRW((c), (m), INFINITE, 0)
/** Free/Destroy a condition **/
#define cond_destroy(c)

/**
 * @brief Compare and swap.
 *
 * This is an atomic operation that compares the content of @ptr 
 * with @comp. If the two are the same then the content of @ptr is 
 * replaced with @replace.
 *
 * @return true if the value was replaced. false otherwise
 */
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
/** A mutex, in the cae of POSIX systems a pthread mutex is used **/
#define mutex             pthread_mutex_t
/** A condition, in the case of POSIX systems a pthread condition variable is used **/
#define condition         pthread_cond_t
/** A thread, the OS specific representation of a thread **/
#define thread            pthread_t
/** A thread return type, the OS specific return type expected by a thread **/
#define threadRet         void*

/** Initialize a mutex **/
#define mutex_init(m)     pthread_mutex_init((m), NULL)
/** Lock a mutex **/
#define mutex_lock        pthread_mutex_lock
/** Unlock a mutex **/
#define mutex_unlock      pthread_mutex_unlock
/** Free/Destroy a mutex **/
#define mutex_destroy     pthread_mutex_destroy

/** Initialize a condition **/
#define cond_init(c)      pthread_cond_init((c), NULL)
/** Broadcast a condition **/
#define cond_broadcast    pthread_cond_broadcast
/** Wait for a condition signal/broadcast **/
#define cond_wait         pthread_cond_wait
/** Free/Destroy a condition **/
#define cond_destroy      pthread_cond_destroy

/**
 * @brief Compare and swap.
 *
 * This is an atomic operation that compares the content of the first param
 * with the second param. If the two are the same then the content of the first param is 
 * replaced with the third param.
 *
 * @return true if the value was replaced. false otherwise
 */
#define cas               __sync_bool_compare_and_swap 
#endif

/**
 * @brief A simple linked list node.
 *
 * A strcuture defining the single linked list node for the queue structure.
 *
 * @sa _queue
 */
typedef struct _queue_node
{
    /** The value for this linked list element **/
    void*               value;
    /** The next node or NULL if none **/
    struct _queue_node* next;
} queueNode;


/**
 * @brief A mostly lock free queue.
 *
 * A queue where the producer side shares a lock and the consumer side shares a lock. This means that as long as
 * you only have one producer or one consumer you should not have any lock contention issues. The locks are also used
 * in order to signal the consumer on new data rather than doing a busy wait.
 *
 * @sa _queue_node
 */
typedef struct _queue
{
    /**
     * @brief The first element in the queue 
     *
     * The first element in the queue or the same as divider and last if the queue is empty.
     */
    queueNode* first;
    /**
     * @brief The division between the producer and consumer side of the queue. 
     *
     * This points to a fake element in the list. Everything on the left side of the element has been read
     * by the consumer and is ready to be freed. Everything on the right side of the queue is ready to be read.
     * Bascially, this element ensures that nothing is being free'd while it's being read and is the "magic" that
     * lets the producer and consumer sides not share a lock.
     */
    queueNode* divider;
    /**
     * @brief The last element in the queue 
     *
     * The last element in the queue or the same as divider and first if the queue is empty.
     */
    queueNode* last;

    /**
     * @brief The producer lock
     *
     * The lock aquired whenever an element is pushed onto the queue. This is normally only obtained by the
     * producer except both this lock and popLock are obtained by any call to freeQueue().
     */
    mutex      pushLock;
    /**
     * @brief The consumer lock
     *
     * The lock aquired whenever an element is popped off the queue. This is normally only obtained by the
     * consumer except both this lock and pushLock are obtained by any call to freeQueue().
     */
    mutex      popLock;

    /**
     * @brief The pushed condition variable
     *
     * The condition that is signalled anytime anything is pushed onto the queue.
     */
    condition  pushed;
} queue;

/**
 * @brief Create a new queue.
 *
 * Create a new queue ready to be pushed and popped from.
 *
 * @return A new queue data structure.
 * @see freeQueue
 */
queue* newQueue();
/**
 * @brief Free the queue and any remaining elements.
 *
 * Free the queue and any remaining nodes on the queue.
 *
 * @param q The queue to free.
 * @see newQueue
 */
void freeQueue(queue* q);
/**
 * @brief Add an element to the end of the queue.
 *
 *  Add an element to the end of the queue.
 *
 * @param q The queue to add to.
 * @param value The value to add
 * @return 0 on success, non-zero on failure
 * @see queuePop
 * @see queuePopNoWait
 */
unsigned int queuePush(queue* q, void* value);
/**
 * @brief Remove an element from the queue.
 *
 *  Wait for an element to be available and then remove it from the queue.
 *
 * @param q The queue to remove from.
 * @param value A pointer to the value obtained
 * @return 0 on success, non-zero on failure
 * @see queuePush
 * @see queuePopNoWait
 */
unsigned int queuePop(queue* q, void** value);
/**
 * @brief Remove an element from the queue.
 *
 *  Remove an element from the queue or fail instantly if no element is present.
 *
 * @param q The queue to remove from.
 * @param value A pointer to the value obtained
 * @return 0 on success, non-zero on failure or no element present
 * @see queuePush
 * @see queuePop
 */
unsigned int queuePopNoWait(queue* q, void** value);

#endif
/* vim: set tabstop=4 shiftwidth=4 expandtab: */
