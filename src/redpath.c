//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <stdlib.h>
#include <string.h>

#include <redpath.h>
#include "util.h"

static char* getVersion(const char* path, char** end);
static void parseNode(const char* path, redPathNode* node, redPathNode** end);

redPathNode* parseRedPath(const char* path)
{
    redPathNode* node;
    redPathNode* endNode;
    char* curPath;
    char* end;

    if(!path || strlen(path) == 0)
    {
        return NULL;
    }

    node = (redPathNode*)calloc(1, sizeof(redPathNode));
    if(!node)
    {
        return NULL;
    }
    if(path[0] == '/')
    {
        node->isRoot = true;
        if(path[1] == 'v')
        {
            node->version = getVersion(path+1, &curPath);
            if(curPath == NULL)
            {
                return node;
            }
            if(curPath[0] == '/')
            {
                curPath++;
            }
            node->next = parseRedPath(curPath);
        }
        else
        {
           node->next = parseRedPath(path+1);
        }
        return node;
    }
    node->isRoot = false;
    curPath = getStringTill(path, "/", &end);
    endNode = node;
    parseNode(curPath, node, &endNode);
    free(curPath);
    if(end != NULL)
    {
        endNode->next = parseRedPath(end+1);
    }
    return node;
}

void cleanupRedPath(redPathNode* node)
{
    if(!node)
    {
        return;
    }
    cleanupRedPath(node->next);
    node->next = NULL;
    if(node->version)
    {
        free(node->version);
    }
    if(node->nodeName)
    {
        free(node->nodeName);
    }
    if(node->propName)
    {
        free(node->propName);
    }
    if(node->value)
    {
        free(node->value);
    }
    free(node);
}

static char* getVersion(const char* path, char** end)
{
    return getStringTill(path, "/", end);
}

static void parseNode(const char* path, redPathNode* node, redPathNode** end)
{
    char* indexStart;
    char* index;
    char* indexEnd;
    char* nodeName = getStringTill(path, "[", &indexStart);
    size_t tmpIndex;
    char* opChars;

    if(strcmp(nodeName, "*") == 0)
    {
        node->op = REDPATH_OP_ANY;
        *end = node;
        free(nodeName);
        return;
    }
    node->nodeName = nodeName;
    if(indexStart == NULL)
    {
        *end = node;
        return;
    }
    node->next = (redPathNode*)calloc(1, sizeof(redPathNode));
    if(!node->next)
    {
        return;
    }
    //Skip past [
    indexStart++;
    *end = node->next;
    index = getStringTill(indexStart, "]", NULL);
    tmpIndex = (size_t)strtoull(index, &indexEnd, 0);
    if(indexEnd != index)
    {
        free(index);
        node->next->index = tmpIndex;
        node->next->isIndex = true;
        return;
    }
    opChars = strpbrk(index, "<>=!");
    if(opChars == NULL && index[0] == '*')
    {
        free(index);
        node->next->op = REDPATH_OP_ANY;
        return;
    }
    else if(opChars == NULL)
    {
        if(strncmp(index, "last()", 6) == 0)
        {
            node->next->op = REDPATH_OP_LAST;
        }
        else
        {
            //TODO handle position()
            node->next->op = REDPATH_OP_EXISTS;
            node->next->propName = index;
        }
        return;
    }
    node->next->propName = (char*)malloc((opChars - index)+1);
    memcpy(node->next->propName, index, (opChars - index));
    node->next->propName[(opChars - index)] = 0;

    switch(opChars[0])
    {
        case '=':
            tmpIndex = 1;
            node->next->op = REDPATH_OP_EQUAL;
            break;
        case '<':
            if(opChars[1] == '=')
            {
                node->next->op = REDPATH_OP_LESS_EQUAL;
                tmpIndex = 2;
            }
            else
            {
                node->next->op = REDPATH_OP_LESS;
                tmpIndex = 1;
            }
            break;
        case '>':
            if(opChars[1] == '=')
            {
                node->next->op = REDPATH_OP_GREATER_EQUAL;
                tmpIndex = 2;
            }
            else
            {
                node->next->op = REDPATH_OP_GREATER;
                tmpIndex = 1;
            }
            break;
        case '!':
            if(opChars[1] != '=')
            {
                node->next->op = REDPATH_OP_ERROR;
                return;
            }
            tmpIndex = 2;
            node->next->op = REDPATH_OP_NOTEQUAL;
            break;
        default:
            node->next->op = REDPATH_OP_ERROR;
            free(index);
            return;
    }

#ifdef _MSC_VER
	node->next->value = _strdup(opChars + tmpIndex);
#else
    node->next->value = strdup(opChars+tmpIndex);
#endif
    free(index);
}
/* vim: set tabstop=4 shiftwidth=4 ff=unix expandtab: */
