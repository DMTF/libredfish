//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>

#include <redfish.h>

volatile sig_atomic_t stop = 0;

static struct option long_options[] =
{
    {"help",       no_argument,       0,      '?'},
    {"version",    no_argument,       0,      'V'},
    {"host",       required_argument, 0,      'H'},
    {"method",     required_argument, 0,      'M'},
    {"file",       required_argument, 0,      'f'},
    {"events",     required_argument, 0,      'e'},
    {"workaround", required_argument, 0,      'W'},
    {"username",   required_argument, 0,      'u'},
    {"password",   required_argument, 0,      'p'},
    {"session",    no_argument,       0,      'S'},
    {0, 0, 0, 0}
};

void inthand(int signum)
{
    stop = 1;
}


void print_usage(const char* name)
{
    printf("Usage: %s [OPTIONS] [Query]\n\n", name);
    printf("Test libRedfish.\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -?, --help                 Display this usage message\n");
    printf("  -V, --version              Display the software version\n");
    printf("  -M, --method               The HTTP method to use (Default is GET if not specified)\n");
    printf("  -H, --host                 The host to query\n");
    printf("  -f, --file [filename]      The file to send as a POST payload\n");
    printf("  -e, --events [event URI]   Register for events and send them to the specified URI\n");
    printf("\nQuery:\n");
    printf(" Optional: /vXX - Where XX is the version to use. Defaults to v1.\n");
    printf(" /Name          - Where Name is the name of a JSON tag. If it contains an odata.id only\n");
    printf("                  the code will follow the ID\n");
    printf(" [Index]        - Where Index is a number. If the current node is an array or collection\n");
    printf("                  it will pick the member at the index\n");
    printf("Report bugs to Patrick_Boyd@Dell.com\n");
}

void print_version()
{
    printf("Dell libRedfish Test Tool\n");
    printf("Copyright (C) 2016 Dell, Inc.\n");
    printf("License: This software is liscensed under a non-disclosure agreement.\n");
    printf("         DO NOT REDISTRIBUTE WITHOUT EXPRESS WRITTEN PERMISSION OF DELL, INC.\n\n");
    printf("Written by Patrick Boyd.\n");
}

void printPayload(redfishPayload* payload)
{
    char* text = payloadToString(payload, true);
    if(text)
    {
        printf("%s\n", text);
    }
    else
    {
        printf("(null)\n");
    }
    free(text);
}

char* getFileContents(const char* filename)
{
    FILE*  fp = fopen(filename, "r");
    size_t size;
    char*  ret;

    if(!fp)
    {
        return NULL;
    }

    //Get file length
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    ret = calloc(size+1, sizeof(char));
    if(!ret)
    {
        fclose(fp);
        return NULL;
    }

    if(fread(ret, size, 1, fp) != 1)
    {
        free(ret);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return ret;
}

void printRedfishEvent(redfishPayload* event, enumeratorAuthentication* auth, const char* context)
{
    char* string;
    printf("%s: Called!\n", __FUNCTION__);
    if(auth == NULL)
    {
        printf("No authentication provided\n");
    }
    else
    {
        printf("Authentication provided!\n");
    }
    string = payloadToString(event, true);
    printf("Event:\n%s\n", string);
    free(string);
}

int main(int argc, char** argv)
{
    int              arg;
    int              opt_index  = 0;
    unsigned int     method = 0;
    char*            host = NULL;
    char*            filename = NULL;
    redfishService*  redfish = NULL;
    redfishPayload*  payload;
    redfishPayload*  res;
    char*            query      = NULL;
    char*            leaf = NULL;
    char*            contents;
    redfishPayload*  post;
    bool             deleteRes;
    char*            eventUri = NULL;
    unsigned int     flags = 0;
    char*            username = NULL;
    char*            password = NULL;
    enumeratorAuthentication auth;

    memset(&auth, 0, sizeof(auth));

    while((arg = getopt_long(argc, argv, "?VSH:M:f:W:u:p:", long_options, &opt_index)) != -1)
    {
        switch(arg)
        {
            case 'V':
                print_version();
                return 0;
                break;
            case '?':
                print_usage(argv[0]);
                return 0;
                break;
            case 'M':
                if(strcasecmp(optarg, "GET") == 0)
                {
                    method = 0;
                }
                else if(strcasecmp(optarg, "PATCH") == 0)
                {
                    method = 1;
                }
                else if(strcasecmp(optarg, "POST") == 0)
                {
                    method = 2;
                }
                else if(strcasecmp(optarg, "DELETE") == 0)
                {
                    method = 3;
                }
                else
                {
                    fprintf(stderr, "Error! Unknown Method %s!\n", optarg);
                    return 1;
                }
                break;
            case 'f':
                filename = strdup(optarg);
                break;
            case 'H':
                host = strdup(optarg);
                break;
            case 'e':
                eventUri = strdup(optarg);
                break;
            case 'W':
                if(strcasecmp(optarg, "verdoc") == 0)
                {
                    flags |= REDFISH_FLAG_SERVICE_NO_VERSION_DOC;
                    break;
                }
                break;
            case 'u':
                username = strdup(optarg);
                break;
            case 'p':
                password = strdup(optarg);
                break;
            case 'S':
                auth.authType = REDFISH_AUTH_SESSION;
                break;
        }
    }
    if(host == NULL)
    {
        print_usage(argv[0]);
        return 1;
    }
    if(username && password)
    {
        auth.authCodes.userPass.username = username;
        auth.authCodes.userPass.password = password;
        redfish = createServiceEnumerator(host, NULL, &auth, flags);
    }
    else
    {
        redfish = createServiceEnumerator(host, NULL, NULL, flags);
    }

    if(eventUri != NULL)
    {
        if(registerForEvents(redfish, eventUri, REDFISH_EVENT_TYPE_ALL, printRedfishEvent, NULL) == true)
        {
            signal(SIGINT, inthand);
            printf("Successfully registered. Waiting for events...\n");
            while(!stop)
            {
                pause();
            }
        }
        else
        {
            printf("Failed to register for events! Cleaning up...\n");
        }
        free(eventUri);
        cleanupServiceEnumerator(redfish);
        if(host)
        {
            free(host);
        }
        if(filename)
        {
            free(filename);
        }
        return 0;
    }

    if(optind < argc)
    {
        query = argv[optind++];
    }
    switch(method)
    {
        default:
            break;
        case 1:
            if(query)
            {
                leaf = strrchr(query, '/');
                if(leaf)
                {
                    leaf[0] = '\0';
                    leaf++;
                }
            }
            break;
    }
    if(query)
    {
        payload = getPayloadByPath(redfish, query);
    }
    else
    {
        payload = getPayloadByPath(redfish, "/");;
    }
    switch(method)
    {
        case 0:
        default:
            printPayload(payload);
            break;
        case 1:
            if(leaf && optind < argc)
            {
                res = patchPayloadStringProperty(payload, leaf, argv[optind]);
                printf("PATCH to %s: %s\n", query, (res?"Success":"Failed!"));
                printPayload(res);
                cleanupPayload(res);
            }
            else if(leaf)
            {
                fprintf(stderr, "Missing value for PATCH!\n");
            }
            else
            {
                fprintf(stderr, "Missing property for PATCH!\n");
            }
            break;
        case 2:
            if(!filename)
            {
                fprintf(stderr, "Missing POST payload!\n");
            }
            else
            {
                contents = getFileContents(filename);
                if(contents)
                {
                    post = createRedfishPayloadFromString(contents, redfish);
                    res = postPayload(payload, post);
                    cleanupPayload(post);
                    printf("POST to %s: %s\n", query, (res?"Success":"Failed!"));
                    cleanupPayload(res);
                    free(contents);
                }
                else
                {
                    fprintf(stderr, "Unable to obtain POST payload!\n");
                }
            }
            break;
        case 3:
            deleteRes = deletePayload(payload);
            printf("DELETE to %s: %s\n", query, (deleteRes?"Success":"Failed!"));
            break;
    }
    cleanupPayload(payload);
    cleanupServiceEnumerator(redfish);
    if(host)
    {
        free(host);
    }
    if(filename)
    {
        free(filename);
    }
    return 0;
}

/* vim: set tabstop=4 shiftwidth=4 expandtab: */
