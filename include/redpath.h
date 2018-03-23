//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _REDPATH_H_
#define _REDPATH_H_
#include <stdbool.h>

typedef struct _redPathNode
{
    bool isRoot;
    bool isIndex;

    char* version;
    char* nodeName;
    size_t index;
    char* op;
    char* propName;
    char* value;

    struct _redPathNode* next;
} redPathNode;

redPathNode* parseRedPath(const char* path);
void cleanupRedPath(redPathNode* node);

#endif
