//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#ifndef _MSC_VER
#include <unistd.h>
#else
#include <windows.h>
//Windows sleep function is capital and in milliseconds
#define sleep(x) Sleep((x)*1000);
#endif
#include <signal.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

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

#define strcasecmp stricmp
#define strtok_r strtok_s
#else
#include <syslog.h>
#endif

int verbose = LOG_CRIT;

static void safeFree(void* ptr);
static void doCLI(redfishService* service);

static struct option long_options[] =
{
    {"help",       no_argument,       0,      '?'},
    {"version",    no_argument,       0,      'V'},
    {"host",       required_argument, 0,      'H'},
    {"workaround", required_argument, 0,      'W'},
    {"username",   required_argument, 0,      'u'},
    {"password",   required_argument, 0,      'p'},
    {"session",    no_argument,       0,      'S'},
    {"verbose",    no_argument,       0,      'v'},
    {"token",      required_argument, 0,      'T'},
    {"valgrind",   no_argument,       0,      'X'},
    {0, 0, 0, 0}
};

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
    printf("  -H, --host                 The host to query\n");
    printf("  -v, --verbose              Log more information\n");
    printf("  -T, --token [bearer token] A bearer token to use instead of standard redfish auth\n");
    printf("  -u, --username [user]      The username to authenticate with\n");
    printf("  -p, --password [pass]      The password to authenticate with\n");
    printf("  -S, --session              Use session based auth, as opposed to basic auth\n");
    printf("Report bugs to Patrick_Boyd@Dell.com\n");
}

void print_version()
{
    printf("Dell libRedfish CLI Tool\n");
    printf("Copyright (C) 2019 DMTF.\n");
    printf("Written by Patrick Boyd.\n");
}

int main(int argc, char** argv)
{
    int              arg;
    int              opt_index  = 0;
    char*            host = NULL;
    redfishService*  redfish = NULL;
    unsigned int     flags = 0;
    char*            username = NULL;
    char*            password = NULL;
    char*            token = NULL;
    enumeratorAuthentication auth;
    bool valgrind = false;

    memset(&auth, 0, sizeof(auth));

    while((arg = getopt_long(argc, argv, "?VSH:W:u:p:vT:X", long_options, &opt_index)) != -1)
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
                username = strdup(optarg);
                break;
            case 'p':
                password = strdup(optarg);
                break;
            case 'T':
                token = strdup(optarg);
                break;
            case 'S':
                auth.authType = REDFISH_AUTH_SESSION;
                break;
            case 'v':
                verbose++;
                break;
            case 'X':
                valgrind = true;
                break;
        }
    }
    if(host == NULL)
    {
        print_usage(argv[0]);
        return 1;
    }

    libredfishSetDebugFunction(syslogPrintf);

    if(username && password)
    {
        auth.authCodes.userPass.username = username;
        auth.authCodes.userPass.password = password;
        redfish = createServiceEnumerator(host, NULL, &auth, flags);
    }
    else if(token)
    {
        auth.authCodes.authToken.token = token;
        auth.authType = REDFISH_AUTH_BEARER_TOKEN;
        redfish = createServiceEnumerator(host, NULL, &auth, flags);
    }
    else
    {
        redfish = createServiceEnumerator(host, NULL, NULL, flags);
    }
    if(redfish == NULL)
    {
        fprintf(stderr, "Unable to create service enumerator\n");
    }
    safeFree(token);

    doCLI(redfish);

    serviceDecRefAndWait(redfish);
    if(valgrind)
    {
        sleep(1);
    }
    safeFree(host);
    safeFree(username);
    safeFree(password);
    return 0;
}

typedef struct {
    char* path;
    redfishPayload* current;
} cliState;

cliState gCLIState = {
    NULL,
    NULL
};

typedef int (*commandFunc)(redfishService* service, int argc, char** argv);

int exitFunc(redfishService* service, int argc, char** argv)
{
    (void)service;
    (void)argc;
    (void)argv;
    return -1;
}

typedef struct {
    const char* key;
    int type;
} jsonProperty;

size_t enumerateJsonPayload(json_t* json, jsonProperty** properties)
{
    const char *key;
    json_t *value;
    size_t ret = json_object_size(json);
    size_t i = 0;
    jsonProperty* props = calloc(ret, sizeof(jsonProperty));

    json_object_foreach(json, key, value) {
        props[i].key = key;
        props[i].type = json_typeof(value);
        i++;
    }
    *properties = props;
    return ret;
}

char* guessLinkFromId(const char* id)
{
    char tmp1[256] = {0};
    char tmpRet[256] = {0};
    char* tmp;
    //Hopefully this implementation follows the OpenAPI stuff... otherwise this probably isn't going to work...
    tmp = strchr(id+1, '/');
    if(tmp == NULL)
    {
        return NULL;
    }
    //I think everything off the root is a collection so index this...
    strncpy(tmp1, id, tmp-id);
    tmp++;
    snprintf(tmpRet, sizeof(tmpRet)-1, "%s[Id=%s]", tmp1, tmp);
    return strdup(tmpRet); 
}

void showLinks(json_t* links, bool showTo, bool listMetadata)
{
    const char* key;
    json_t* value;
    json_t* sub;
    json_t* odataIdJson;
    const char* odataId;
    size_t i, count;
    char* better;

    json_object_foreach(links, key, value) {
        odataId = "unknown";
        if(json_is_array(value) && json_array_size(value) == 0)
        {
            //Skip empty arrays of links...
            continue;
        }
        else if(json_is_array(value))
        {
            count = json_array_size(value);
            for(i = 0; i < count; i++)
            {
                if(showTo)
                {
                    sub = json_array_get(value, i);
                    if(sub == NULL)
                    {
                        fprintf(stderr, "Array changed!?!?\n");
                    }
                    odataIdJson = json_object_get(sub, "@odata.id");
                    if(odataIdJson)
                    {
                        odataId = json_string_value(odataIdJson);
                        if(strncmp(odataId, "/redfish/v1", 11) == 0)
                        {
                            odataId+=11;
                        }
                    }
                    better = guessLinkFromId(odataId);
                    if(better)
                    {
                        printf("\033[1;36m%s[%lu] -> %s\033[0m\n", key, i, better);
                        free(better);
                    }
                    else
                    {
                        printf("\033[1;36m%s[%lu] -> %s\033[0m\n", key, i, odataId);
                    }
                }
                else
                {
                    printf("\033[1;36m%s[%lu]\033[0m\n", key, i);
                }
            }
            continue;
        }
        if(strchr(key, '@'))
        {
            if(listMetadata)
            {
                printf("\033[1;32mLinks.%s\033[0m\n", key+1);
            }
            else
            {
                //Skip metadata...
                continue;
            }
        }
        if(showTo)
        {
            odataIdJson = json_object_get(value, "@odata.id");
            if(odataIdJson)
            {
                odataId = json_string_value(odataIdJson);
                if(strncmp(odataId, "/redfish/v1", 11) == 0)
                {
                    odataId+=11;
                }
            }
            better = guessLinkFromId(odataId);
            if(better)
            {
                printf("\033[1;36m%s -> %s\033[0m\n", key, better);
                free(better);
            }
            else
            {
                printf("\033[1;36m%s -> %s\033[0m\n", key, odataId);
            }
        }
        else
        {
            printf("\033[1;36m%s\033[0m\n", key);
        }
    }
}

void showActions(json_t* actions, const char* prefix, bool listMetadata)
{
    const char* key;
    json_t* value;
    char* tmp;

    json_object_foreach(actions, key, value) {
        if(json_is_object(value) && json_object_size(value) == 0)
        {
            //Skip empty Oem objects...
            continue;
        }
        else if(strcmp(key, "Oem") == 0)
        {
            showActions(value, key, listMetadata);
            continue;
        }
        if(strchr(key, '@'))
        {
            if(listMetadata)
            {
                printf("\033[1;32mActions.%s%s\033[0m\n", prefix, key+1);
            }
            else
            {
                //Skip metadata...
                continue;
            }
        }
        tmp = strchr(key, '#');
        if(tmp == NULL)
        {
            printf("\033[1;35m%s%s\033[0m\n", prefix, key);
        }
        else
        {
            printf("\033[1;35m%s%s\033[0m\n", prefix, tmp+1);
        }
    }
}

int lsFunc(redfishService* service, int argc, char** argv)
{
    redfishPayload* payload;
    size_t count, i, size, j;
    jsonProperty* props = NULL;
    bool listMetadata = false;
    bool showFull = false;

    (void)service;

    for(i = 0; i < (size_t)argc; i++)
    {
        if(strcmp(argv[i], "-a") == 0)
        {
            listMetadata = true;
        }
        else if(strcmp(argv[i], "-l") == 0)
        {
            showFull = true;
        }
        else if(strcmp(argv[i], "-al") == 0)
        {
            listMetadata = true;
            showFull = true;
        }
    }
    
    payload = gCLIState.current;
    if(payload == NULL)
    {
        printf("(null)\n");
        return 1;
    }
    if(isPayloadCollection(payload)) 
    {
        count = enumerateJsonPayload(payload->json, &props);
        for(i = 0; i < count; i++)
        {
            if(listMetadata == false && strchr(props[i].key, '@'))
            {
                continue;
            }
            switch(props[i].type)
            {
                case JSON_OBJECT:
                  if(strcmp(props[i].key, "Links") == 0)
                  {
                      showLinks(json_object_get(payload->json, "Links"), showFull, listMetadata);
                  }
                  else if(strcmp(props[i].key, "Actions") == 0)
                  {
                      showActions(json_object_get(payload->json, "Actions"), "", listMetadata);
                  }
                  else
                  {
                      printf("\033[1;32m%s\033[0m\n", props[i].key);
                  }
                  break;
                case JSON_ARRAY:
                  if(strcmp(props[i].key, "Members") == 0)
                  {
                      size = getCollectionSize(payload);
                      for(j = 0; j < size; j++)
                      {
                          printf("\033[1;32m%u\033[0m\n", (unsigned int)j);
                      }
                  }
                  else
                  {
                      //Shouldn't happen, but handle it just in case...
                      printf("\033[1;32m%s\033[0m\n", props[i].key);
                  }
                  break;
                case JSON_STRING:
                  printf("%s\n", props[i].key);
                  break;
                default:
                  printf("%u => %s\n", props[i].type, props[i].key);
            }
        }
    }
    else
    {
        count = enumerateJsonPayload(payload->json, &props);
        for(i = 0; i < count; i++)
        {
            if(listMetadata == false && strchr(props[i].key, '@'))
            {
                continue;
            }
            switch(props[i].type)
            {
                case JSON_OBJECT:
                  if(strcmp(props[i].key, "Links") == 0)
                  {
                      showLinks(json_object_get(payload->json, "Links"), showFull, listMetadata);
                      break;
                  }
                  else if(strcmp(props[i].key, "Actions") == 0)
                  {
                      showActions(json_object_get(payload->json, "Actions"), "", listMetadata);
                      break;
                  }
#if __GNUC__ >= 7
                  //Yes, we want this to fall through...
                  __attribute__ ((fallthrough));
#endif
                case JSON_ARRAY:
                  printf("\033[1;32m%s\033[0m\n", props[i].key);
                  break;
                case JSON_STRING:
                  printf("%s\n", props[i].key);
                  break;
                default:
                  printf("%u => %s\n", props[i].type, props[i].key);
            }
        }
    }
    if(props)
    {
        free(props);
    }
    return 0;
}

int cdFunc(redfishService* service, int argc, char** argv)
{
    redfishPayload* payload;
    redfishPayload* child;
    unsigned long index;
    char newPath[1024];
    char* end = NULL;
    size_t len;

    if(argc < 2)
    {
        fprintf(stderr, "No child specified\n");
        return 1;
    }

    if(strcmp(argv[1], ".") == 0)
    {
        //Done...
        return 0;
    }
    else if(strcmp(argv[1], "..") == 0)
    {
        len = strlen(gCLIState.path);
        if(gCLIState.path[len-1] == ']')
        {
            //Remove the number node...
            end = strrchr(gCLIState.path, '[');
            if(end)
            {
                //Just null terminate here...
                *end = 0;
            }
        }
        else
        {
            end = strrchr(gCLIState.path, '/');
            if(end == gCLIState.path)
            {
                //Leave the first trailing /
                end[1] = 0;
            }
            else
            {
                *end = 0;
            }
        }
        child = getPayloadByPath(service, gCLIState.path);
        if(child == NULL)
        {
            fprintf(stderr, "Unable to get parent at %s\n", gCLIState.path);
            return 1;
        }
        cleanupPayload(gCLIState.current);
        gCLIState.current = child;
        return 0;
    }

    //first get the current payload...
    payload = gCLIState.current;
    if(payload == NULL)
    {
        fprintf(stderr, "Unable to access current path!\n");
        return 1;
    }
    child = getPayloadForPathString(payload, argv[1]); 
    if(child)
    {
        if(strcmp(gCLIState.path, "/") == 0)
        {
            snprintf(newPath, sizeof(newPath)-1, "/%s", argv[1]);
        }
        else
        {
            snprintf(newPath, sizeof(newPath)-1, "%s/%s", gCLIState.path, argv[1]);
        }
        free(gCLIState.path);
        gCLIState.path = strdup(newPath);
        cleanupPayload(payload);
        gCLIState.current = child;
        return 0;
    }
    else
    {
        //Try links...
        snprintf(newPath, sizeof(newPath)-1, "Links/%s", argv[1]);
        child = getPayloadForPathString(payload, newPath);
        if(child)
        {
            snprintf(newPath, sizeof(newPath)-1, "%s/Links/%s", gCLIState.path, argv[1]);
            free(gCLIState.path);
            gCLIState.path = strdup(newPath);
            cleanupPayload(payload);
            gCLIState.current = child;
            return 0;
        }
    }
    index = strtoul(argv[1], &end, 10);
    if(end && end[0] == 0)
    {
        child = getPayloadByIndex(payload, index);
        if(child)
        {
            snprintf(newPath, sizeof(newPath)-1, "%s[%lu]", gCLIState.path, index);
            free(gCLIState.path);
            gCLIState.path = strdup(newPath);
            cleanupPayload(payload);
            gCLIState.current = child;
            return 0;
        }
    }
    fprintf(stderr, "Unable to change to child %s\n", argv[1]);
    return 1;
}

int catFunc(redfishService* service, int argc, char** argv)
{
    redfishPayload* payload = NULL;
    unsigned long index;
    char* end = NULL;
    bool freePayload = false;

    (void)service;

    if(argc < 2)
    {
        fprintf(stderr, "No child specified\n");
        return 1;
    }

    if(strcmp(argv[1], ".") == 0)
    {
        payload = gCLIState.current;
    }
    else
    {
        payload = getPayloadByNodeName(gCLIState.current, argv[1]);
        if(payload == NULL)
        {
            index = strtoul(argv[1], &end, 10);
            if(end && end[0] == 0)
            {
                payload = getPayloadByIndex(gCLIState.current, index);
            }
        }
        freePayload = true;
    }
    if(payload == NULL)
    {
        fprintf(stderr, "Unable to get content for %s\n", argv[1]);
        return 1;
    }
    end = payloadToString(payload, true);
    if(end == NULL)
    {
        fprintf(stderr, "Unable to get string for %s\n", argv[1]);
        return 1;
    }
    printf("%s\n", end);
    if(freePayload)
    {
        cleanupPayload(payload);
    }
    free(end);
    return 0;
}

int patchFunc(redfishService* service, int argc, char** argv)
{
    redfishPayload* ret;
    char* end = NULL;

    (void)service;

    if(argc < 2)
    {
        fprintf(stderr, "No child specified\n");
        return 1;
    }
    else if(argc < 3)
    {
        fprintf(stderr, "No value specified\n");
        return 1;
    }

    ret = patchPayloadStringProperty(gCLIState.current, argv[1], argv[2]);
    if(ret == NULL)
    {
        fprintf(stderr, "No returned payload. This is probably an error.\n");
        return 1;
    }
    end = payloadToString(ret, true);
    cleanupPayload(ret);
    if(end == NULL)
    {
        fprintf(stderr, "Unable to get string return\n");
        return 1;
    }
    printf("%s\n", end);
    free(end);
    return 0;
}

char* logStringMap[] = {
    "Emergency",
    "Alert",
    "Critical",
    "Error",
    "Warning",
    "Notice",
    "Info",
    "Debug"
};

size_t countInString(const char* str, char x)
{
    size_t ret = 0;
    char* tmp = strchr(str, x);
    while(tmp)
    {
        ret++;
        tmp++; //Move to next char...
        tmp = strchr(tmp, x);
    }
    return ret;
}

int debugFunc(redfishService* service, int argc, char** argv)
{
    size_t pluscount;
    size_t minuscount;

    (void)service;
    if(argc <= 1)
    {
        if(verbose > LOG_DEBUG)
        {
            printf("Current Level = Beyond Debug %d\n", verbose);
        }
        else
        {
            printf("Current Level = %s %d\n", logStringMap[verbose], verbose);
        }
    }
    else if(argc == 2)
    {
        pluscount = countInString(argv[1], '+');
        minuscount = countInString(argv[1], '-');
        if(pluscount || minuscount)
        {
            verbose -= minuscount;
            verbose += pluscount;
            if(verbose < 0)
            {
                verbose = 0;
            }
        }
    }
    return 0;
}

static void addStringToJsonObject(json_t* object, const char* key, const char* value)
{
    json_t* jValue = json_string(value);

    json_object_set(object, key, jValue);

    json_decref(jValue);
}

int actionFunc(redfishService* service, int argc, char** argv)
{
    redfishPayload* actions;
    redfishPayload* action;
    redfishPayload* postpayload;
    redfishPayload* retPayload;
    char realName[100];
    int i;
    json_t* postPayloadJson;
    char* value;
    
    actions = getPayloadByNodeName(gCLIState.current, "Actions");
    if(actions == NULL)
    {
        fprintf(stderr, "No Actions property on current payload\n");
        return 1;
    }
    snprintf(realName, sizeof(realName)-1, "#%s", argv[0]+2);
    realName[sizeof(realName)-1] = 0;
    action = getPayloadByNodeName(actions, realName);
    cleanupPayload(actions);
    if(action == NULL)
    {
        fprintf(stderr, "Unable to location action named %s\n", realName);
        return 1;
    }
    postPayloadJson = json_object();
    for(i = 1; i < argc; i++)
    {
        value = strchr(argv[i], '=');
        if(value)
        {
            value[0] = 0;
            value++;
            addStringToJsonObject(postPayloadJson, argv[i], value);
        }
        else
        {
            fprintf(stderr, "Unable to parse parameter %s\n", argv[i]);
            return 1;
        }
    }
    postpayload = createRedfishPayload(postPayloadJson, service);
    if(postpayload == NULL)
    {
        fprintf(stderr, "Unable to allocate postPayload!\n");
        return 1;
    }
    retPayload = postPayload(action, postpayload);
    cleanupPayload(action);
    cleanupPayload(postpayload);
    if(retPayload)
    {
        value = payloadToString(retPayload, true);
        if(value)
        {
            printf("%s\n", value);
            free(value);
        }
    }
    else
    {
        fprintf(stderr, "No payload returned... probably an error\n");
        return 1;
    }
    return 0;
}

typedef struct {
    commandFunc func;
    char* funcName;
} funcMap;

static funcMap map[] = {
  {exitFunc, "exit"},
  {lsFunc, "ls"},
  {cdFunc, "cd"},
  {catFunc, "cat"},
  {patchFunc, "patch"},
  {debugFunc, "debug"},
  {NULL, NULL}
};

char* trimwhitespace(char* str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

static commandFunc parseCommand(const char* buffer, int* argc, char*** argv)
{
    char* myBuff = strdup(buffer);
    char* cont;
    char* first;
    char** tmp;
    commandFunc ret = NULL;
    size_t i;

    myBuff = trimwhitespace(myBuff);
    first = strtok_r(myBuff, " ", &cont);

    if(first == NULL)
    {
        free(myBuff);
        return exitFunc;
    }
    for(i = 0; map[i].funcName != NULL; i++)
    {
        if(strcasecmp(first, map[i].funcName) == 0)
        {
            ret = map[i].func;
            break;
        }
    }
    if(first[0] == '.')
    {
        ret = actionFunc;
    }
    if(ret == NULL)
    {
        free(myBuff);
        return ret;
    }
    *argc = 1;
    *argv = calloc(sizeof(char*), 1);
    (*argv)[0] = strdup(first);
    while((first = strtok_r(NULL, " ", &cont)) != NULL)
    {
        (*argc)++;
        tmp = realloc(*argv, sizeof(char*)*(*argc));
        if(tmp == NULL)
        {
            free(*argv);
            fprintf(stderr, "Unable to allocate memory for arguments!\n");
            abort();
        }
        tmp[(*argc)-1] = strdup(first);
        *argv = tmp;
    }

    free(myBuff);
    return ret;
}

#ifdef HAVE_READLINE
char* command_tab_generator(const char* text, int state)
{
    static int list_index, len;
    static bool isAction;
    char* name;
    json_t* actionJson;
    const char* key;
    json_t* value;
    int i;
    char tmp[100];

    if(!state)
    {
        list_index = 0;
        len = strlen(text);
        if(text[0] == '.')
        {
            isAction = true;
        }
        else
        {
            isAction = false;
        }
    }

    if(isAction)
    {
        actionJson = json_object_get(gCLIState.current->json, "Actions");
        if(actionJson == NULL)
        {
            return NULL;
        }
        i = 0;
        json_object_foreach(actionJson, key, value) {
            if(i < list_index) {
                i++;
                continue;
            }
            snprintf(tmp, sizeof(tmp)-1, "./%s", key+1);
            tmp[sizeof(tmp)-1] = 0;
            list_index++;
            if(strncmp(tmp, text, len) == 0)
            {
                return strdup(tmp);
            }
            i++;
        }
    }
    else
    {
        while((name = map[list_index++].funcName))
        {
            if(strncmp(name, text, len) == 0)
            {
                return strdup(name);
            }
        }
    }

    return NULL;
}

char* child_completion(const char* text, int state)
{
    static int len;
    static size_t count, list_index;
    static size_t memberCount;
    static jsonProperty* props;
    char tmp[100];

    if(!state)
    {
        list_index = 0;
        len = strlen(text);
        count = enumerateJsonPayload(gCLIState.current->json, &props);
        memberCount = (size_t)-1;
    }

    while(list_index < count)
    {
        if(strncmp(props[list_index].key, text, len) == 0)
        {
            return strdup(props[list_index++].key);
        }
        list_index++;
    }
    if(isPayloadCollection(gCLIState.current)) 
    {
        if(memberCount == (size_t)-1)
        {
            memberCount = getCollectionSize(gCLIState.current);
        }
        while(list_index < count+memberCount)
        {
            snprintf(tmp, sizeof(tmp)-1, "%lu", list_index-count);
            list_index++;
            if(strncmp(tmp, text, len) == 0)
            {
                return strdup(tmp);
            }
        }
    }
    free(props);

    return NULL;
}

char** tab_completion(const char* text, int start, int end)
{
    (void)end;
    rl_attempted_completion_over = 1;
    if(start == 0)
    {
        return rl_completion_matches(text, command_tab_generator);
    }
    else
    {
        return rl_completion_matches(text, child_completion);
    }
}
#endif

static void doCLI(redfishService* service)
{
#ifndef HAVE_READLINE
    char buffer[1024];
#else
    char* buffer;
    char prompt[1024];
#endif
    commandFunc command;
    int res;
    int argc;
    char** argv;
    int i;

#ifdef HAVE_READLINE
    rl_attempted_completion_function = tab_completion;

    using_history();
#endif

    gCLIState.path = strdup("/");
    gCLIState.current = getPayloadByPath(service, gCLIState.path);

    while(1)
    {
#ifndef HAVE_READLINE
        printf("%s> ", gCLIState.path);
        fgets(buffer, sizeof(buffer), stdin);
#else
        snprintf(prompt, sizeof(prompt)-1, "%s> ", gCLIState.path);
        prompt[sizeof(prompt)-1] = 0;
        buffer = readline(prompt);
#endif
        command = parseCommand(buffer, &argc, &argv);
        if(command == NULL)
        {
            printf("Unknown command \"%s\"\n", buffer);
#ifdef HAVE_READLINE
            add_history(buffer);
            free(buffer);
#endif
            continue;
        }
#ifdef HAVE_READLINE
        add_history(buffer);
        free(buffer);
#endif
        res = command(service, argc, argv);
        for(i = 0; i < argc; i++)
        {
            free(argv[i]);
        }
        free(argv);
        if(res < 0)
        {
            break;
        }
    }
    cleanupPayload(gCLIState.current);
    free(gCLIState.path);
}

static void safeFree(void* ptr)
{
    if(ptr)
    {
        free(ptr);
    }
}
/* vim: set tabstop=4 shiftwidth=4 expandtab: */
