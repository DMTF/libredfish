//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <redfishEvent.h>

char* readstdin()
{
    int c;
    size_t p4kB = 4096, i = 0;
    void *newPtr = NULL;
    char *ptrString = malloc(p4kB * sizeof (char));

    while (ptrString != NULL && (c = getchar()) != EOF)
    {
        if (i == p4kB * sizeof (char))
        {
            p4kB += 4096;
            if ((newPtr = realloc(ptrString, p4kB * sizeof (char))) != NULL)
                ptrString = (char*) newPtr;
            else
            {
                free(ptrString);
                return NULL;
            }
        }
        ptrString[i++] = c;
    }

    if (ptrString != NULL)
    {
        ptrString[i] = '\0';
        ptrString = realloc(ptrString, strlen(ptrString) + 1);
    }
    else return NULL;

    return ptrString;
}

int main(int argc, char** argv)
{
    char* method = getenv("REQUEST_METHOD");
    char* auth = getenv("HTTP_AUTHORIZATION");
    char* body = readstdin();
    zsock_t* remote;
    char* msg;
    size_t size;

    (void)argc;
    (void)argv;

    if(!method || strcmp(method, "POST") != 0)
    {
        printf("Status: 405\n\n");
        if(body)
        {
            free(body);
        }
        return 0;
    }

    if(body == NULL)
    {
        printf("Status: 400\n\n");
        return 0;
    }

    remote = REDFISH_EVENT_0MQ_HELPER_NEW_SOCK;
    if(remote == NULL)
    {
        printf("Status: 500\n");
        printf("Content-type: text/html\n\n");
        printf("Failed to create socket!");
        free(body);
        return 0;
    }

    //The word "Authorization" is 13 characters + 1 space + 2 \n's + null terminator = 17
    size = strlen(body)+17;
    if(auth)
    {
        size += strlen(auth);
    }
    else
    {
        //The word "None" is 4 characters long
        size += 4;
    }

    msg = malloc(size);
    if(!msg)
    {
        zsock_destroy(&remote);
        free(body);
        printf("Status: 500\n\n");
        return 0;
    }
    if(auth)
    {
        snprintf(msg, size, "Authorization %s\n\n%s", auth, body);
    }
    else
    {
        snprintf(msg, size, "Authorization None\n\n%s", body);
    }

    if(zstr_send(remote, msg) == 0)
    {
        printf("Status: 200\n\n");
    }
    else
    {
        printf("Status: 500\n\n");
    }
    zsock_flush(remote);
    //If I destroy the socket I don't get the message on the other end...
    //Since the whole program is about to go out of scope this should be ok...
    //zsock_destroy(&remote);
    free(body);
    free(msg);
    return 0;
}
