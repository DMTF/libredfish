//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include "queue.h"
#include <stdlib.h>
#include <stdbool.h>

static queueNode* newQueueNode(void* value);

queue* newQueue()
{
    queue* ret = malloc(sizeof(queue));
    if(ret == NULL)
    {
        return NULL;
    }
    ret->first = ret->divider = ret->last = newQueueNode(NULL);
    if(ret->first == NULL)
    {
        free(ret);
        return NULL;
    }
    mutex_init(&ret->pushLock);
    mutex_init(&ret->popLock);

    cond_init(&ret->pushed);

    return ret;
}

void freeQueue(queue* q)
{
    queueNode* node;

    if(q == NULL)
    {
        return;
    }
    mutex_lock(&q->pushLock);
    mutex_lock(&q->popLock);
    while(q->first != NULL)
    {
        node = q->first;
        q->first = node->next;
        free(node);
    }
    cond_destroy(&q->pushed);
    mutex_unlock(&q->pushLock);
    mutex_destroy(&q->pushLock);
    mutex_unlock(&q->popLock);
    mutex_destroy(&q->popLock);
    free(q);
}

unsigned int queuePush(queue* q, void* value)
{
    queueNode* node = newQueueNode(value);
    if(node == NULL)
    {
        return 1;
    }
    mutex_lock(&q->pushLock);
    q->last->next = node;
    if(cas(&q->last, q->last, q->last->next) == false)
    {
        //Try once more...
        if(cas(&q->last, q->last, q->last->next) == false)
        {
            q->last = NULL;
            mutex_unlock(&q->pushLock);
            free(node);
            return 1;
        }
    }
    //Cleanup unused nodes...
    while(q->first != q->divider)
    {
        node = q->first;
        q->first = node->next;
        free(node);
    }
    cond_broadcast(&q->pushed);
    mutex_unlock(&q->pushLock);
    return 0;
}

unsigned int queuePop(queue* q, void** value)
{
    mutex_lock(&q->popLock);
    while(q->divider == q->last)
    {
        cond_wait(&q->pushed, &q->popLock);
    }
    *value = q->divider->next->value;
    if(cas(&q->divider, q->divider, q->divider->next) == false)
    {
        //Try once more...
        if(cas(&q->divider, q->divider, q->divider->next) == false)
        {
            //Didn't really pop...
            mutex_unlock(&q->popLock);
            return 1;
        }
    }

    mutex_unlock(&q->popLock);
    return 0;
}

unsigned int queuePopNoWait(queue* q, void** value)
{
    mutex_lock(&q->popLock);
    if(q->divider == q->last)
    {
        mutex_unlock(&q->popLock);
        return 1;
    }
    *value = q->divider->next->value;
    if(cas(&q->divider, q->divider, q->divider->next) == false)
    {
        //Try once more...
        if(cas(&q->divider, q->divider, q->divider->next) == false)
        {
            //Didn't really pop...
            mutex_unlock(&q->popLock);
            return 1;
        }
    }
    mutex_unlock(&q->popLock);
    return 0;
}

static queueNode* newQueueNode(void* value)
{
    queueNode* ret = malloc(sizeof(queueNode));
    if(ret == NULL)
    {
        return NULL;
    }
    ret->value = value;
    ret->next  = NULL;
    return ret;
}
/* vim: set tabstop=4 shiftwidth=4 ff=unix expandtab: */
