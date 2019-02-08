//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017-2019 DMTF. All rights reserved.
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

typedef enum _RedPathOp {
    REDPATH_OP_EQUAL=0,
    REDPATH_OP_NOTEQUAL,
    REDPATH_OP_LESS,
    REDPATH_OP_GREATER,
    REDPATH_OP_LESS_EQUAL,
    REDPATH_OP_GREATER_EQUAL,
    REDPATH_OP_EXISTS,
    REDPATH_OP_ANY,
    REDPATH_OP_LAST,

    REDPATH_OP_ERROR
} RedPathOp;

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
    RedPathOp op;
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
