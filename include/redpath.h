//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file redpath.h
 * @author Patrick Boyd
 * @brief File containing the interface for the "redpath" notation.
 *
 * This file explains the interface for creating a structured representation of a redpath string.
 *
 * @see https://github.com/DMTF/libredfish
 */
#ifndef _REDPATH_H_
#define _REDPATH_H_
#include <stdbool.h>

/**
 * @brief A redpath node.
 *
 * A strcuture defining a redpath node.
 */
typedef struct _redPathNode
{
    /** Does this node represent the service root **/
    bool isRoot;
    /** Does this node represent an index in a collection **/
    bool isIndex;

    /** The version of Redfish being requested. Only valid if isRoot is true **/
    char* version;
    /** The nodeName to be examined **/
    char* nodeName;
    /** The index requested. Only valid if isIndex is true **/
    size_t index;
    /** The operation to perform **/
    char* op;
    /** The property to perform the operation on **/
    char* propName;
    /** The value of the operation **/
    char* value;

    /** The next redpath node or NULL if the end **/
    struct _redPathNode* next;
} redPathNode;

/**
 * @brief Parse a redpath string.
 *
 * Take a string representation of redpath and convert it to a redPathNode version.
 *
 * @param path The string representation.
 * @return NULL if the string could not be parsed. A redPathNode otherwise
 */
redPathNode* parseRedPath(const char* path);
/**
 * @brief Free the redPathNode.
 *
 * Free the passed in node and any nodes after it.
 *
 * @param node The node list to free.
 */
void cleanupRedPath(redPathNode* node);

#endif
