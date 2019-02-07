//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#ifndef _MSC_VER
#include <unistd.h>
#define mutex_t           pthread_mutex_t
#define mutex_lock        pthread_mutex_lock
#define mutex_unlock      pthread_mutex_unlock
#else
#include <windows.h>
#define mutex_t           SRWLOCK
#define mutex_lock        AcquireSRWLockExclusive
#define mutex_unlock      ReleaseSRWLockExclusive
#endif
#include <signal.h>

#include <redfish.h>

#ifdef _MSC_VER
//The defines are the same as linux's syslog.h
#define	LOG_EMERG	0
#define	LOG_ALERT	1
#define	LOG_CRIT	2
#define	LOG_ERR		3
#define	LOG_WARNING	4
#define	LOG_NOTICE	5
#define	LOG_INFO	6
#define	LOG_DEBUG	7
#else
#include <syslog.h>
#endif

volatile sig_atomic_t stop = 0;

int verbose = LOG_CRIT;

static void safeFree(void* ptr);

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
    {"verbose",    no_argument,       0,      'v'},
    {"token",      required_argument, 0,      'T'},
    {"command",    required_argument, 0,      'c'},
    {0, 0, 0, 0}
};

typedef void (*commandFunc)(redfishPayload* payload);

typedef struct {
    const char* commandName;
    commandFunc function;
} commandMapping;

static void getHealth(redfishPayload* payload);
static void getRollup(redfishPayload* payload);
static void getState(redfishPayload* payload);
static void getName(redfishPayload* payload);

static commandMapping commands[] = {
    {"getHealth", getHealth},
    {"getRollup", getRollup},
    {"getState", getState},
    {"getName", getName},
    {NULL, NULL}
};

void inthand(int signum)
{
    (void)signum;
    stop = 1;
}

void syslogPrintf(int priority, const char* message, ...)
{
    va_list args;
    va_start(args, message);

    if(priority <= verbose)
    {
        vfprintf(stderr, message, args);
    }
    va_end(args);
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
    printf("  -v, --verbose              Log more information\n");
    printf("  -T, --token [bearer token] A bearer token to use instead of standard redfish auth\n");
    printf("  -u, --username [user]      The username to authenticate with\n");
    printf("  -p, --password [pass]      The password to authenticate with\n");
    printf("  -S, --session              Use session based auth, as opposed to basic auth\n");
    printf("  -c, --command [command]    Run the specified command on the resource\n");
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

    (void)context;

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

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

typedef struct
{
    unsigned int method;
    char*        leaf;
    char*        query;
    char*        filename;
    redfishService*  redfish;
    int          argc;
    char**       argv;
    commandMapping* command;
} gotPayloadContext;

void patchDone(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    gotPayloadContext* myContext = context;

    printf("PATCH to %s: %s (%u)\n", myContext->query, (success?"Success":"Failed!"), httpCode);
    printPayload(payload);
    cleanupPayload(payload);
}

void postDone(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    gotPayloadContext* myContext = context;

    printf("POST to %s: %s (%u)\n", myContext->query, (success?"Success":"Failed!"), httpCode);
    printPayload(payload);
    cleanupPayload(payload);
}

void gotPayload(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    gotPayloadContext* myContext = context;
    bool             res;
    redfishPayload*  post;
    char*            contents;
    char             tmp[1024];

    if(success == false)
    {
        printf("Got a failure, httpCode = %u\n", httpCode);
    }
    if(payload)
    {
        if(myContext->command)
        {
            myContext->command->function(payload);
            cleanupPayload(payload);
            free(context);
            return;
        }
        switch(myContext->method)
        {
            case 0:
            default:
                printPayload(payload);
                free(context);
                break;
            case 1:
                if(myContext->leaf && optind < myContext->argc)
                {
                    snprintf(tmp, sizeof(tmp), "{\"%s\": \"%s\"}", myContext->leaf, myContext->argv[optind]);
                    post = createRedfishPayloadFromString(tmp, myContext->redfish);
                    res = patchPayloadAsync(payload, post, NULL, patchDone, context);
                    cleanupPayload(post);
                    if(res == false)
                    {
                        fprintf(stderr, "Unable to invoke async PATCH!\n");
                    }
                }
                else if(myContext->leaf)
                {
                    fprintf(stderr, "Missing value for PATCH!\n");
                    free(context);
                }
                else
                {
                    fprintf(stderr, "Missing property for PATCH!\n");
                    free(context);
                }
                break;
            case 2:
                if(!myContext->filename)
                {
                    fprintf(stderr, "Missing POST payload!\n");
                }
                else
                {
                    contents = getFileContents(myContext->filename);
                    if(contents)
                    {
                        post = createRedfishPayloadFromString(contents, myContext->redfish);
                        res = postPayloadAsync(payload, post, NULL, postDone, context);
                        cleanupPayload(post);
                        if(res == false)
                        {
                            fprintf(stderr, "Unable to invoke async POST!\n");
                        }
                        free(contents);
                    }
                    else
                    {
                        fprintf(stderr, "Unable to obtain POST payload!\n");
                    }
                }
                break;
            case 3:
                res = deletePayload(payload);
                printf("DELETE to %s: %s\n", myContext->query, (res?"Success":"Failed!"));
                free(context);
                break;
        }
        cleanupPayload(payload);
    }
}

static commandMapping* getCommandByString(const char* name)
{
    size_t i;
    for(i = 0; commands[i].commandName; i++)
    {
        if(strcasecmp(name, commands[i].commandName) == 0)
        {
            return &(commands[i]);
        }
    }
    return NULL;
}

typedef struct {
    char*           query;
    char*           filename;
    unsigned int    method;
    commandMapping* command;
    int             argc;
    char**          argv;
} redfishParams;

redfishParams gRedfishParams = {0};

static void gotRedfishService(redfishService* service, void* context)
{
    mutex_t*           mutex = (mutex_t*)context;
    char*              leaf = NULL; 
    gotPayloadContext* myContext;
 
    switch(gRedfishParams.method)
    {
        default:
            break;
        case 1:
            if(gRedfishParams.query)
            {
                leaf = strrchr(gRedfishParams.query, '/');
                if(leaf)
                {
                    leaf[0] = '\0';
                    leaf++;
                }
            }
            break;
    }
    myContext = malloc(sizeof(gotPayloadContext));
    myContext->method = gRedfishParams.method;
    myContext->leaf = leaf;
    myContext->query = gRedfishParams.query;
    myContext->filename = gRedfishParams.filename;
    myContext->redfish = service;
    myContext->argc = gRedfishParams.argc;
    myContext->argv = gRedfishParams.argv;
    myContext->command = gRedfishParams.command;
    if(gRedfishParams.query)
    {
        getPayloadByPathAsync(service, gRedfishParams.query, NULL, gotPayload, myContext);
    }
    else
    {
        getPayloadByPathAsync(service, "/", NULL, gotPayload, myContext);
    }
    serviceDecRefAndWait(service);
    mutex_unlock(mutex);
}

int main(int argc, char** argv)
{
    int              arg;
    int              opt_index  = 0;
    char*            host = NULL;
    unsigned int     flags = 0;
    enumeratorAuthentication* auth = NULL;
    bool ret;
#ifndef _MSC_VER
    mutex_t          mutex = PTHREAD_MUTEX_INITIALIZER;
#else
    mutex_t          mutex;

    InitializeSRWLock(&mutex);
#endif

    memset(&auth, 0, sizeof(auth));

    while((arg = getopt_long(argc, argv, "?VSH:M:f:W:u:p:vT:c:", long_options, &opt_index)) != -1)
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
                    gRedfishParams.method = 0;
                }
                else if(strcasecmp(optarg, "PATCH") == 0)
                {
                    gRedfishParams.method = 1;
                }
                else if(strcasecmp(optarg, "POST") == 0)
                {
                    gRedfishParams.method = 2;
                }
                else if(strcasecmp(optarg, "DELETE") == 0)
                {
                    gRedfishParams.method = 3;
                }
                else
                {
                    fprintf(stderr, "Error! Unknown Method %s!\n", optarg);
                    return 1;
                }
                break;
            case 'f':
                gRedfishParams.filename = strdup(optarg);
                break;
            case 'H':
                host = strdup(optarg);
                break;
            case 'W':
                if(strcasecmp(optarg, "verdoc") == 0)
                {
                    flags |= REDFISH_FLAG_SERVICE_NO_VERSION_DOC;
                    break;
                }
                break;
            case 'u':
                if(auth == NULL)
                {
                    auth = malloc(sizeof(enumeratorAuthentication));
                }
                auth->authCodes.userPass.username = strdup(optarg);
                break;
            case 'p':
                if(auth == NULL)
                {
                    auth = malloc(sizeof(enumeratorAuthentication));
                }
                auth->authCodes.userPass.password = strdup(optarg);
                break;
            case 'T':
                if(auth == NULL)
                {
                    auth = malloc(sizeof(enumeratorAuthentication));
                }
                auth->authCodes.authToken.token = strdup(optarg);
                auth->authType = REDFISH_AUTH_BEARER_TOKEN;
                break;
            case 'S':
                if(auth == NULL)
                {
                    auth = malloc(sizeof(enumeratorAuthentication));
                }
                auth->authType = REDFISH_AUTH_SESSION;
                break;
            case 'v':
                verbose++;
                break;
            case 'c':
                gRedfishParams.command = getCommandByString(optarg);
                break;
        }
    }
    if(host == NULL)
    {
        print_usage(argv[0]);
        return 1;
    }

    if(optind < argc)
    {
        gRedfishParams.query = argv[optind++];
    }

    gRedfishParams.argc = argc;
    gRedfishParams.argv = argv;

    libredfishSetDebugFunction(syslogPrintf);

    mutex_lock(&mutex);
    ret = createServiceEnumeratorAsync(host, NULL, auth, flags, gotRedfishService, &mutex);
    if(ret != false)
    {
        //Wait till the callback is done...
        mutex_lock(&mutex);
    }
    else
    {
        fprintf(stderr, "createServiceEnumeratorAsync returned false!\n");
    }

    safeFree(host);
    safeFree(gRedfishParams.filename);
    return 0;
}

static void safeFree(void* ptr)
{
    if(ptr)
    {
        free(ptr);
    }
}

static void printHealth(redfishHealth health, const char* healthType)
{
    const char* healthStr;
    switch(health)
    {
        case RedfishHealthError:
            healthStr = "Error";
            break;
        case RedfishHealthUnknown:
            healthStr = "Unknown";
            break;
        case RedfishHealthOK:
            healthStr = "OK";
            break;
        case RedfishHealthWarning:
            healthStr = "Warning";
            break;
        case RedfishHealthCritical:
            healthStr = "Critical";
            break;
        default:
            healthStr = "Non-enum value";
            break;
    }
    printf("Resource %s is %s (%d)\n", healthType, healthStr, health);
}

static void getHealth(redfishPayload* payload)
{
    redfishHealth health;
    if(payload == NULL)
    {
        fprintf(stderr, "Payload is NULL!\n");
        return;
    }
    health = getResourceHealth(payload);
    printHealth(health, "health");
}

static void getRollup(redfishPayload* payload)
{
    redfishHealth health;
    if(payload == NULL)
    {
        fprintf(stderr, "Payload is NULL!\n");
        return;
    }
    health = getResourceRollupHealth(payload);
    printHealth(health, "rollup health");
}

static void getState(redfishPayload* payload)
{
    redfishState state;
    const char* stateStr;
    if(payload == NULL)
    {
        fprintf(stderr, "Payload is NULL!\n");
        return;
    }
    state = getResourceState(payload);
    switch(state)
    {
        case RedfishStateError:
            stateStr = "Error";
            break;
        case RedfishStateUnknown:
            stateStr = "Unknown";
            break;
        case RedfishStateEnabled:
            stateStr = "Enabled";
            break;
        case RedfishStateDisabled:
            stateStr = "Disabled";
            break;
        case RedfishStateStandbyOffline:
            stateStr = "StandbyOffline";
            break;
        case RedfishStateStandbySpare:
            stateStr = "StandbySpare";
            break;
        case RedfishStateInTest:
            stateStr = "InTest";
            break;
        case RedfishStateStarting:
            stateStr = "Starting";
            break;
        case RedfishStateAbsent:
            stateStr = "Absent";
            break;
        case RedfishStateUnavailableOffline:
            stateStr = "UnavailableOffline";
            break;
        case RedfishStateDeferring:
            stateStr = "Deferring";
            break;
        case RedfishStateQuiesced:
            stateStr = "Quiesced";
            break;
        case RedfishStateUpdating:
            stateStr = "Updating";
            break;
        default:
            stateStr = "Non-enum value";
            break;
    }
    printf("Resource state is %s (%d)\n", stateStr, state);
}

static void getName(redfishPayload* payload)
{
    char* name;
    if(payload == NULL)
    {
        fprintf(stderr, "Payload is NULL!\n");
        return;
    }
    name = getResourceName(payload);
    if(name == NULL)
    {
        fprintf(stderr, "Name is NULL!\n");
        return;
    }
    printf("Name is \"%s\"\n", name);
    free(name);
}

/* vim: set tabstop=4 shiftwidth=4 expandtab: */
