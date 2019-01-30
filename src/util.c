//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <stdlib.h>
#include "util.h"

char* safeStrdup(const char* str)
{
    if(str == NULL)
    {
        return NULL;
    }
#ifdef _MSC_VER
	return _strdup(str);
#else
    return strdup(str);
#endif
}

char* getStringTill(const char* string, const char* terminator, char** retEnd)
{
    char* ret;
    char* end;
    end = strstr((char*)string, terminator);
    if(retEnd)
    {
        *retEnd = end;
    }
    if(end == NULL)
    {
        //No terminator
        return safeStrdup(string);
    }
    ret = (char*)malloc((end-string)+1);
    memcpy(ret, string, (end-string));
    ret[(end-string)] = 0;
    return ret;
}
