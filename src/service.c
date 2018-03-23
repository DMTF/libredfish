//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>

#include <redfishService.h>
#include <redfishPayload.h>
#include <redpath.h>
#include <redfishEvent.h>

struct MemoryStruct
{
  char *memory;
  size_t size;
  char* origin;
  size_t originalSize;
};

struct EventCallbackRegister
{
    bool unregister;
    redfishEventCallback callback;
    unsigned int eventTypes;
    char* context;
    redfishService* service;
};

#if CZMQ_VERSION_MAJOR >= 3
struct EventActorState
{
    bool terminate;
    zlist_t* registrations;
};

static zactor_t* eventActor = NULL;
#endif

static int initCurl(redfishService* service);
static size_t curlWriteMemory(void *buffer, size_t size, size_t nmemb, void *userp);
static size_t curlReadMemory(void *buffer, size_t size, size_t nmemb, void *userp);
static int curlSeekMemory(void *userp, curl_off_t offset, int origin);
static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userp);
static redfishService* createServiceEnumeratorNoAuth(const char* host, const char* rootUri, bool enumerate, unsigned int flags);
static redfishService* createServiceEnumeratorBasicAuth(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags);
static redfishService* createServiceEnumeratorSessionAuth(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags);
static redfishService* createServiceEnumeratorToken(const char* host, const char* rootUri, const char* token, unsigned int flags);
static char* makeUrlForService(redfishService* service, const char* uri);
static json_t* getVersions(redfishService* service, const char* rootUri);
#if CZMQ_VERSION_MAJOR >= 3
static char* getEventSubscriptionUri(redfishService* service);
static bool registerCallback(redfishService* service, redfishEventCallback callback, unsigned int eventTypes, const char* context);
static bool unregisterCallback(redfishEventCallback callback, unsigned int eventTypes, const char* context);
static void eventActorTask(zsock_t* pipe, void* args);
static void cleanupEventActor();
static void addStringToJsonArray(json_t* array, const char* value);
#endif
static void addStringToJsonObject(json_t* object, const char* key, const char* value);

redfishService* createServiceEnumerator(const char* host, const char* rootUri, enumeratorAuthentication* auth, unsigned int flags)
{
    if(auth == NULL)
    {
        return createServiceEnumeratorNoAuth(host, rootUri, true, flags);
    }
    if(auth->authType == REDFISH_AUTH_BASIC)
    {
        return createServiceEnumeratorBasicAuth(host, rootUri, auth->authCodes.userPass.username, auth->authCodes.userPass.password, flags);
    }
    else if(auth->authType == REDFISH_AUTH_BEARER_TOKEN)
    {
        return createServiceEnumeratorToken(host, rootUri, auth->authCodes.authToken.token, flags);
    }
    else if(auth->authType == REDFISH_AUTH_SESSION)
    {
        return createServiceEnumeratorSessionAuth(host, rootUri, auth->authCodes.userPass.username, auth->authCodes.userPass.password, flags);
    }
    else
    {
        return NULL;
    }
}

json_t* getUriFromService(redfishService* service, const char* uri)
{
    char* url;
    char* string;
    CURLcode res;
    struct MemoryStruct chunk;
    json_t* ret;
    struct curl_slist* headers = NULL;
    char tokenHeader[1024];

    if(service == NULL || uri == NULL)
    {
        return NULL;
    }

    url = makeUrlForService(service, uri);
    if(!url)
    {
        return NULL;
    }

    //Allocate initial memory chunk
    chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */
    chunk.origin = chunk.memory;
    chunk.originalSize = chunk.size;

    if(service->sessionToken)
    {
        snprintf(tokenHeader, sizeof(tokenHeader), "X-Auth-Token: %s", service->sessionToken);
        headers = curl_slist_append(headers, tokenHeader);
    }

    headers = curl_slist_append(headers, "OData-Version: 4.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "User-Agent: libredfish");
#ifdef _MSC_VER
	WaitForSingleObject(service->mutex, INFINITE);
#else
    pthread_mutex_lock(&service->mutex);
#endif
    curl_easy_setopt(service->curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(service->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(service->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(service->curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(service->curl, CURLOPT_URL, url);
    res = curl_easy_perform(service->curl);
    curl_easy_setopt(service->curl, CURLOPT_HTTPHEADER, NULL);
    curl_easy_setopt(service->curl, CURLOPT_WRITEDATA, NULL);
#ifdef _MSC_VER
	ReleaseMutex(service->mutex);
#else
    pthread_mutex_unlock(&service->mutex);
#endif
    curl_slist_free_all(headers);
    free(url);
    if(res != CURLE_OK)
    {
        free(chunk.memory);
        return NULL;
    }

    string = chunk.memory;
    //There is a bug in certain version of CURL where this will contain the header data too (on redirects)
    if(string[0] == 'H' && string[1] == 'T' && string[2] == 'T' && string[3] == 'P')
    {
        string = strstr(string, "\r\n\r\n");
	string+=4;
    }
    ret = json_loads(string, 0, NULL);
    free(chunk.memory);
    return ret;
}

json_t* patchUriFromService(redfishService* service, const char* uri, const char* content)
{
    char*               url;
    char*               resStr;
    json_t*             ret;
    CURLcode            res;
    struct MemoryStruct readChunk;
    struct MemoryStruct writeChunk;
    struct curl_slist* headers = NULL;
    char tokenHeader[1024];

    if(service == NULL || uri == NULL || !content)
    {
        return NULL;
    }

    url = makeUrlForService(service, uri);
    if(!url)
    {
        return NULL;
    }

    writeChunk.memory = (char*)content;
    writeChunk.size = strlen(content);
    writeChunk.origin = writeChunk.memory;
    writeChunk.originalSize = writeChunk.size;
    readChunk.memory = (char*)malloc(1);
    readChunk.size = 0;
    readChunk.origin = readChunk.memory;
    readChunk.originalSize = readChunk.size;

    if(service->sessionToken)
    {
        snprintf(tokenHeader, sizeof(tokenHeader), "X-Auth-Token: %s", service->sessionToken);
        headers = curl_slist_append(headers, tokenHeader);
    }
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Transfer-Encoding:");
    headers = curl_slist_append(headers, "Expect:");

#ifdef _MSC_VER
	WaitForSingleObject(service->mutex, INFINITE);
#else
	pthread_mutex_lock(&service->mutex);
#endif
    curl_easy_setopt(service->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(service->curl, CURLOPT_INFILESIZE, writeChunk.size);
    curl_easy_setopt(service->curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(service->curl, CURLOPT_WRITEDATA, &readChunk);
    curl_easy_setopt(service->curl, CURLOPT_URL, url);
    curl_easy_setopt(service->curl, CURLOPT_READDATA, &writeChunk);
    curl_easy_setopt(service->curl, CURLOPT_UPLOAD, 1L);
    res = curl_easy_perform(service->curl);
    curl_easy_setopt(service->curl, CURLOPT_UPLOAD, 0L);
    curl_easy_setopt(service->curl, CURLOPT_INFILESIZE, -1);
    curl_easy_setopt(service->curl, CURLOPT_HTTPHEADER, NULL);
    curl_easy_setopt(service->curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(service->curl, CURLOPT_READDATA, NULL);
#ifdef _MSC_VER
	ReleaseMutex(service->mutex);
#else
	pthread_mutex_unlock(&service->mutex);
#endif
    free(url);
    curl_slist_free_all(headers);
    if(res != CURLE_OK)
    {
        free(readChunk.memory);
        return NULL;
    }
    if(readChunk.size == 0)
    {
        free(readChunk.memory);
        return NULL;
    }
    resStr = readChunk.memory;
    ret = json_loads(resStr, 0, NULL);
    free(resStr);
    return ret;
}

typedef struct {
    char* Location;
    char* XAuthToken;
} knownHeaders;

static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{
    char* tmp;
    knownHeaders* headers = (knownHeaders*)userdata;
    if(headers == NULL)
    {
        return nitems * size;
    }
#ifdef _MSC_VER
	if (_strnicmp(buffer, "Location:", 9) == 0)
#else
	if (strncasecmp(buffer, "Location:", 9) == 0)
#endif
    {
#ifdef _MSC_VER
		headers->Location = _strdup(buffer + 10);
#else
        headers->Location = strdup(buffer+10);
#endif
        tmp = strchr(headers->Location, '\r');
        if(tmp)
        {
            tmp[0] = 0;
        }
    }
#ifdef _MSC_VER
	else if(_strnicmp(buffer, "X-Auth-Token:", 13) == 0)
#else
    else if(strncasecmp(buffer, "X-Auth-Token:", 13) == 0)
#endif
    {
#ifdef _MSC_VER
		headers->XAuthToken = _strdup(buffer + 14);
#else
        headers->XAuthToken = strdup(buffer+14);
#endif
        tmp = strchr(headers->XAuthToken, '\r');
        if(tmp)
        {
            tmp[0] = 0;
        }
    }

    return nitems * size;
}


json_t* postUriFromService(redfishService* service, const char* uri, const char* content, size_t contentLength, const char* contentType)
{
    char*               url;
    char*               resStr;
    json_t*             ret;
    CURLcode            res;
    struct MemoryStruct readChunk;
    struct MemoryStruct writeChunk;
    struct curl_slist*  headers = NULL;
    long                http_code;
    knownHeaders        headerValues;
    char tokenHeader[1024];

    if(service == NULL || uri == NULL || !content)
    {
        return NULL;
    }

    url = makeUrlForService(service, uri);
    if(!url)
    {
        return NULL;
    }
    if(contentLength == 0)
    {
        contentLength = strlen(content);
    }

    memset(&headerValues, 0, sizeof(headerValues));
    writeChunk.memory = (char*)content;
    writeChunk.size = contentLength;
    writeChunk.origin = writeChunk.memory;
    writeChunk.originalSize = writeChunk.size;
    readChunk.memory = (char*)malloc(1);
    readChunk.size = 0;
    readChunk.origin = readChunk.memory;
    readChunk.originalSize = readChunk.size;

    if(service->sessionToken)
    {
        snprintf(tokenHeader, sizeof(tokenHeader), "X-Auth-Token: %s", service->sessionToken);
        headers = curl_slist_append(headers, tokenHeader);
    }

    if(contentType == NULL)
    {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    else
    {
        snprintf(tokenHeader, sizeof(tokenHeader), "Content-Type: %s", contentType);
        headers = curl_slist_append(headers, tokenHeader);
    }
    headers = curl_slist_append(headers, "Transfer-Encoding:");

#ifdef _MSC_VER
	WaitForSingleObject(service->mutex, INFINITE);
#else
	pthread_mutex_lock(&service->mutex);
#endif
    curl_easy_setopt(service->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(service->curl, CURLOPT_INFILESIZE, writeChunk.size);
    curl_easy_setopt(service->curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(service->curl, CURLOPT_WRITEDATA, &readChunk);
    curl_easy_setopt(service->curl, CURLOPT_URL, url);
    curl_easy_setopt(service->curl, CURLOPT_READDATA, &writeChunk);
    curl_easy_setopt(service->curl, CURLOPT_HEADERDATA, &headerValues);
    curl_easy_setopt(service->curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(service->curl, CURLOPT_UPLOAD, 1L);
    res = curl_easy_perform(service->curl);
    curl_easy_setopt(service->curl, CURLOPT_UPLOAD, 0L);
    curl_easy_setopt(service->curl, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(service->curl, CURLOPT_HEADERFUNCTION, NULL);
    curl_easy_setopt(service->curl, CURLOPT_HTTPHEADER, NULL);
    curl_easy_setopt(service->curl, CURLOPT_INFILESIZE, -1);
#ifdef _MSC_VER
	ReleaseMutex(service->mutex);
#else
	pthread_mutex_unlock(&service->mutex);
#endif
    curl_slist_free_all(headers);
    free(url);
    if(res != CURLE_OK)
    {
        free(readChunk.memory);
        return NULL;
    }
    if(readChunk.size == 0)
    {
        free(readChunk.memory);
        return NULL;
    }
    curl_easy_getinfo(service->curl, CURLINFO_RESPONSE_CODE, &http_code);
    if(http_code >= 400)
    {
        //Error!
        free(readChunk.memory);
        return NULL;
    }
    if(headerValues.XAuthToken)
    {
        if(service->sessionToken)
        {
            free(service->sessionToken);
        }
        service->sessionToken = headerValues.XAuthToken;
    }
    if(http_code == 201 && headerValues.Location)
    {
        free(readChunk.memory);
        ret = getUriFromService(service, headerValues.Location);
        free(headerValues.Location);
        return ret;
    }
    if(headerValues.Location)
    {
        free(headerValues.Location);
    }
    resStr = readChunk.memory;
    ret = json_loads(resStr, 0, NULL);
    free(resStr);
    return ret;
}

bool deleteUriFromService(redfishService* service, const char* uri)
{
    char*               url;
    CURLcode            res;
    struct MemoryStruct readChunk;

    if(service == NULL || uri == NULL)
    {
        return false;
    }

    url = makeUrlForService(service, uri);
    if(!url)
    {
        return false;
    }

    readChunk.memory = (char*)malloc(1);
    readChunk.size = 0;
    readChunk.origin = readChunk.memory;
    readChunk.originalSize = readChunk.size;

#ifdef _MSC_VER
	WaitForSingleObject(service->mutex, INFINITE);
#else
	pthread_mutex_lock(&service->mutex);
#endif
    curl_easy_setopt(service->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(service->curl, CURLOPT_WRITEDATA, &readChunk);
    curl_easy_setopt(service->curl, CURLOPT_URL, url);
    res = curl_easy_perform(service->curl);
#ifdef _MSC_VER
	ReleaseMutex(service->mutex);
#else
	pthread_mutex_unlock(&service->mutex);
#endif
    free(url);
    free(readChunk.memory);
    if(res != CURLE_OK)
    {
        return false;
    }
    return true;
}

bool registerForEvents(redfishService* service, const char* postbackUri, unsigned int eventTypes, redfishEventCallback callback, const char* context)
{
#if CZMQ_VERSION_MAJOR >= 3
    json_t* eventSubscriptionPayload;
    json_t* postRes;
    json_t* typeArray;
    char* eventSubscriptionUri;
    char* eventSubscriptionPayloadStr;

    if(service == NULL || postbackUri == NULL || callback == NULL)
    {
        return false;
    }

    eventSubscriptionUri = getEventSubscriptionUri(service);
    if(eventSubscriptionUri == NULL)
    {
        //This service does not support Eventing
        return false;
    }

    eventSubscriptionPayload = json_object();
    if(eventSubscriptionPayload == NULL)
    {
        //Out of memory
        free(eventSubscriptionUri);
        return false;
    }
    addStringToJsonObject(eventSubscriptionPayload, "Destination", postbackUri);
    addStringToJsonObject(eventSubscriptionPayload, "Context", context);
    addStringToJsonObject(eventSubscriptionPayload, "Protocol", "Redfish");
    if(eventTypes != 0)
    {
        typeArray = json_array();
        if(typeArray == NULL)
        {
            json_decref(eventSubscriptionPayload);
            free(eventSubscriptionUri);
            return false;
        }
        if(eventTypes & REDFISH_EVENT_TYPE_STATUSCHANGE)
        {
            addStringToJsonArray(typeArray, "StatusChange");
        }
        if(eventTypes & REDFISH_EVENT_TYPE_RESOURCEUPDATED)
        {
            addStringToJsonArray(typeArray, "ResourceUpdated");
        }
        if(eventTypes & REDFISH_EVENT_TYPE_RESOURCEADDED)
        {
            addStringToJsonArray(typeArray, "ResourceAdded");
        }
        if(eventTypes & REDFISH_EVENT_TYPE_RESOURCEREMOVED)
        {
            addStringToJsonArray(typeArray, "ResourceRemoved");
        }
        if(eventTypes & REDFISH_EVENT_TYPE_ALERT)
        {
            addStringToJsonArray(typeArray, "Alert");
        }
        json_object_set(eventSubscriptionPayload, "EventTypes", typeArray);
        json_decref(typeArray);
    }
    eventSubscriptionPayloadStr = json_dumps(eventSubscriptionPayload, 0);
    json_decref(eventSubscriptionPayload);
    if(eventSubscriptionPayloadStr == NULL)
    {
        free(eventSubscriptionUri);
        return false;
    }

    //start eventActor before POST
    if(!eventActor || !zactor_is(eventActor))
    {
        eventActor = zactor_new(eventActorTask, NULL);
        if(!eventActor)
        {
            free(eventSubscriptionUri);
            free(eventSubscriptionPayloadStr);
            return false;
        }
    }
    atexit(cleanupEventActor);
    registerCallback(service, callback, eventTypes, context);

    postRes = postUriFromService(service, eventSubscriptionUri, eventSubscriptionPayloadStr, 0, NULL);
    free(eventSubscriptionUri);
    free(eventSubscriptionPayloadStr);
    if(postRes == NULL)
    {
        unregisterCallback(callback, eventTypes, context);
        return false;
    }
    json_decref(postRes);
    return true;
#else
    (void)service;
    (void)postbackUri;
    (void)eventTypes;
    (void)callback;
    (void)context;
    return false;
#endif
}

redfishPayload* getRedfishServiceRoot(redfishService* service, const char* version)
{
    json_t* value;
    json_t* versionNode;
    const char* verUrl;

    if(version == NULL)
    {
        versionNode = json_object_get(service->versions, "v1");
    }
    else
    {
        versionNode = json_object_get(service->versions, version);
    }
    if(versionNode == NULL)
    {
        return NULL;
    }
    verUrl = json_string_value(versionNode);
    if(verUrl == NULL)
    {
        return NULL;
    }
    value = getUriFromService(service, verUrl);
    if(value == NULL)
    {
        if((service->flags & REDFISH_FLAG_SERVICE_NO_VERSION_DOC) == 0)
        {
            json_decref(versionNode);
        }
        return NULL;
    }
    return createRedfishPayload(value, service);
}

redfishPayload* getPayloadByPath(redfishService* service, const char* path)
{
    redPathNode* redpath;
    redfishPayload* root;
    redfishPayload* ret;

    if(!service || !path)
    {
        return NULL;
    }

    redpath = parseRedPath(path);
    if(!redpath)
    {
        return NULL;
    }
    if(!redpath->isRoot)
    {
        cleanupRedPath(redpath);
        return NULL;
    }
    root = getRedfishServiceRoot(service, redpath->version);
    if(redpath->next == NULL)
    {
        cleanupRedPath(redpath);
        return root;
    }
    ret = getPayloadForPath(root, redpath->next);
    cleanupPayload(root);
    cleanupRedPath(redpath);
    return ret;
}

void cleanupServiceEnumerator(redfishService* service)
{
    if(!service)
    {
        return;
    }
    free(service->host);
    curl_easy_cleanup(service->curl);
    json_decref(service->versions);
    if(service->sessionToken != NULL)
    {
        free(service->sessionToken);
    }
    free(service);
    curl_global_cleanup();
}

#if 0
static
void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size)
{
  size_t i;
  size_t c;
  unsigned int width=0x10;

  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
          text, (long)size, (long)size);

  for(i=0; i<size; i+= width) {
    fprintf(stream, "%4.4lx: ", (long)i);

    /* show hex to the left */
    for(c = 0; c < width; c++) {
      if(i+c < size)
        fprintf(stream, "%02x ", ptr[i+c]);
      else
        fputs("   ", stream);
    }

    /* show data on the right */
    for(c = 0; (c < width) && (i+c < size); c++) {
      char x = (ptr[i+c] >= 0x20 && ptr[i+c] < 0x80) ? ptr[i+c] : '.';
      fputc(x, stream);
    }

    fputc('\n', stream); /* newline */
  }
}

static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  const char *text;
  (void)handle; /* prevent compiler warning */
  FILE* stream = (FILE*)userp;

  switch (type) {
  case CURLINFO_TEXT:
    fprintf(stream, "== Info: %s", data);
  default: /* in case a new one is introduced to shock us */
    return 0;

  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  }

  dump(text, stream, (unsigned char *)data, size);
  return 0;
}
#endif

static int initCurl(redfishService* service)
{
    //char* filename = tempnam("/mnt/persistent_data/", "curl.");
    //FILE* fp = fopen(filename, "wb");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    service->curl = curl_easy_init();
    if(!service->curl)
    {
        return -1;
    }
    //curl_easy_setopt(service->curl, CURLOPT_DEBUGFUNCTION, my_trace);
    //curl_easy_setopt(service->curl, CURLOPT_VERBOSE, true);
    //curl_easy_setopt(service->curl, CURLOPT_DEBUGDATA, fp);
    curl_easy_setopt(service->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(service->curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(service->curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(service->curl, CURLOPT_WRITEFUNCTION, curlWriteMemory);
    curl_easy_setopt(service->curl, CURLOPT_READFUNCTION, curlReadMemory);
    curl_easy_setopt(service->curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(service->curl, CURLOPT_SEEKFUNCTION, curlSeekMemory);
    curl_easy_setopt(service->curl, CURLOPT_TIMEOUT, 20L);
#ifdef _MSC_VER
	service->mutex = CreateMutex(NULL, FALSE, NULL);
#else
    pthread_mutex_init(&(service->mutex), NULL);
#endif
    return 0;
}

static size_t curlWriteMemory(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
  void* tmp;

  tmp = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(tmp == NULL)
  {
      free(mem->memory);
      mem->memory = NULL;
      mem->size = 0;
      return 0;
  }
  mem->memory = tmp;

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static size_t curlReadMemory(void *ptr, size_t size, size_t nmemb, void *userp)
{
    size_t fullsize = size*nmemb;
    struct MemoryStruct* pooh = (struct MemoryStruct *)userp;

    if(fullsize < 1)
    {
        return 0;
    }

    if(pooh->size > fullsize)
    {
        memcpy(ptr, pooh->memory, fullsize);
        pooh->memory += fullsize;
        pooh->size -= fullsize;
        return fullsize;
    }
    else if(pooh->size)
    {
        memcpy(ptr, pooh->memory, pooh->size);
        pooh->memory += pooh->size;
        fullsize = pooh->size;
        pooh->size = 0;
        return fullsize;
    }

    return 0;                          /* no more data left to deliver */
}

static int curlSeekMemory(void *userp, curl_off_t offset, int origin)
{
    struct MemoryStruct* pooh = (struct MemoryStruct *)userp;
    (void)origin;

    if(pooh == NULL)
    {
        return CURL_SEEKFUNC_CANTSEEK;
    }

    pooh->memory = pooh->origin+offset;
    pooh->size = pooh->originalSize-offset;

    return CURL_SEEKFUNC_OK;
}

static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userp)
{
    char* header = NULL;
    char* tmp;

    if(userp == NULL)
    {
        return nitems * size;
    }
    if(strncmp(buffer, "Location: ", 10) == 0)
    {
#ifdef _MSC_VER
		*((char**)userp) = _strdup(buffer + 10);
#else
        *((char**)userp) = strdup(buffer+10);
#endif
        header = *((char**)userp);
    }
    if(header)
    {
        tmp = strchr(header, '\r');
        if(tmp)
        {
            *tmp = 0;
        }
        tmp = strchr(header, '\n');
        if(tmp)
        {
            *tmp = 0;
        }
    }
    return nitems * size;
}

static redfishService* createServiceEnumeratorNoAuth(const char* host, const char* rootUri, bool enumerate, unsigned int flags)
{
    redfishService* ret;

    ret = (redfishService*)calloc(1, sizeof(redfishService));
    if(initCurl(ret) != 0)
    {
        free(ret);
        return NULL;
    }
#ifdef _MSC_VER
	ret->host = _strdup(host);
#else
    ret->host = strdup(host);
#endif
    ret->flags = flags;
    if(enumerate)
    {
        ret->versions = getVersions(ret, rootUri);
    }

    return ret;
}

static redfishService* createServiceEnumeratorBasicAuth(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags)
{
    redfishService* ret;

    ret = createServiceEnumeratorNoAuth(host, rootUri, false, flags);
    curl_easy_setopt(ret->curl, CURLOPT_USERNAME, username);
    curl_easy_setopt(ret->curl, CURLOPT_PASSWORD, password);
    ret->versions = getVersions(ret, rootUri);
    return ret;
}

static redfishService* createServiceEnumeratorSessionAuth(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags)
{
    redfishService* ret;
    redfishPayload* payload;
    redfishPayload* links;
    json_t* sessionPayload;
    json_t* session;
    json_t* odataId;
    const char* uri;
    json_t* post;
    char* content;

    ret = createServiceEnumeratorNoAuth(host, rootUri, true, flags);
    if(ret == NULL)
    {
        return NULL;
    }
    payload = getRedfishServiceRoot(ret, NULL);
    if(payload == NULL)
    {
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    links = getPayloadByNodeName(payload, "Links");
    cleanupPayload(payload);
    if(links == NULL)
    {
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    session = json_object_get(links->json, "Sessions");
    if(session == NULL)
    {
        cleanupPayload(links);
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    odataId = json_object_get(session, "@odata.id");
    if(odataId == NULL)
    {
        cleanupPayload(links);
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    uri = json_string_value(odataId);
    post = json_object();
    addStringToJsonObject(post, "UserName", username);
    addStringToJsonObject(post, "Password", password);
    content = json_dumps(post, 0);
    json_decref(post);
    sessionPayload = postUriFromService(ret, uri, content, 0, NULL);
    if(sessionPayload == NULL)
    {
        //Failed to create session!
        free(content);
        cleanupPayload(links);
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    json_decref(sessionPayload);
    cleanupPayload(links);
    free(content);
    return ret;
}

static redfishService* createServiceEnumeratorToken(const char* host, const char* rootUri, const char* token, unsigned int flags)
{
    redfishService* ret;
    char header[256];
    struct curl_slist *headers = NULL;

    ret = createServiceEnumeratorNoAuth(host, rootUri, false, flags);

    snprintf(header, sizeof(header), "Authorization:  Bearer %s", token);
    headers = curl_slist_append(headers, header);
    curl_easy_setopt(ret->curl, CURLOPT_HTTPHEADER, headers);
    ret->versions = getVersions(ret, rootUri);
    return ret;
}

static char* makeUrlForService(redfishService* service, const char* uri)
{
    char* url;
    if(service->host == NULL)
    {
        return NULL;
    }
    url = (char*)malloc(strlen(service->host)+strlen(uri)+1);
    strcpy(url, service->host);
    strcat(url, uri);
    return url;
}

static json_t* getVersions(redfishService* service, const char* rootUri)
{
    if(service->flags & REDFISH_FLAG_SERVICE_NO_VERSION_DOC)
    {
        service->versions = json_object();
        if(service->versions == NULL)
        {
            return NULL;
        }
        addStringToJsonObject(service->versions, "v1", "/redfish/v1");
        return service->versions;
    }
    if(rootUri != NULL)
    {
        return getUriFromService(service, rootUri);
    }
    else
    {
        return getUriFromService(service, "/redfish");
    }
}

#if CZMQ_VERSION_MAJOR >= 3
static char* getEventSubscriptionUri(redfishService* service)
{
    redPathNode* redpath;
    redfishPayload* root;
    redfishPayload* eventSub;
    json_t*         odataId;
    char*           ret;

    redpath = parseRedPath("EventService/Subscriptions");
    if(!redpath)
    {
        return NULL;
    }
    root = getRedfishServiceRoot(service, NULL);
    if(root == NULL)
    {
        cleanupRedPath(redpath);
        return NULL;
    }
    eventSub = getPayloadForPath(root, redpath);
    cleanupPayload(root);
    cleanupRedPath(redpath);
    if(eventSub == NULL)
    {
        return NULL;
    }
    odataId = json_object_get(eventSub->json, "@odata.id");
    if(odataId == NULL)
    {
        cleanupPayload(eventSub);
        return NULL;
    }
    ret = strdup(json_string_value(odataId));
    cleanupPayload(eventSub);
    return ret;
}

static bool registerCallback(redfishService* service, redfishEventCallback callback, unsigned int eventTypes, const char* context)
{
    struct EventCallbackRegister* registration;
    zmsg_t* msg;

    msg = zmsg_new();
    if(!msg)
    {
        return false;
    }

    //Passing to another thread need to allocate
    registration = malloc(sizeof(struct EventCallbackRegister));
    if(!registration)
    {
        zmsg_destroy(&msg);
        return false;
    }
    registration->unregister = false;
    registration->callback = callback;
    registration->eventTypes = eventTypes;
    if(context)
    {
        //Passing to another thread, this may go out of scope unless I duplicate it
        registration->context = strdup(context);
    }
    else
    {
        registration->context = NULL;
    }
    registration->service = service;

    if(zmsg_addmem(msg, registration, sizeof(struct EventCallbackRegister)) != 0)
    {
        if(registration->context)
        {
            free(registration->context);
        }
        free(registration);
        zmsg_destroy(&msg);
        return false;
    }

    if(zmsg_send(&msg, eventActor) == 0)
    {
        //registration is copied into the zmsg...
        free(registration);
        return true;
    }
    //zmsg_send always destroys the msg regardless of return code
    if(registration->context)
    {
        free(registration->context);
    }
    free(registration);
    return false;
}

static bool unregisterCallback(redfishEventCallback callback, unsigned int eventTypes, const char* context)
{
    struct EventCallbackRegister* registration;
    zmsg_t* msg;

    msg = zmsg_new();

    //Passing to another thread need to allocate
    registration = malloc(sizeof(struct EventCallbackRegister));
    if(!registration)
    {
        zmsg_destroy(&msg);
        return false;
    }
    registration->unregister = true;
    registration->callback = callback;
    registration->eventTypes = eventTypes;
    if(context)
    {
        //Passing to another thread, this may go out of scope unless I duplicate it
        registration->context = strdup(context);
    }
    else
    {
        registration->context = NULL;
    }

    if(zmsg_addmem(msg, registration, sizeof(struct EventCallbackRegister)) != 0)
    {
        if(registration->context)
        {
            free(registration->context);
        }
        free(registration);
        zmsg_destroy(&msg);
        return false;
    }

    if(zmsg_send(&msg, eventActor) == 0)
    {
        return true;
    }
    //zmsg_send always destroys the msg regardless of return code
    if(registration->context)
    {
        free(registration->context);
    }
    free(registration);
    return false;
}

static void freeRegistration(void* reg)
{
    struct EventCallbackRegister* registration = (struct EventCallbackRegister*)reg;

    if(registration)
    {
        if(registration->context)
        {
            free(registration->context);
        }
        free(registration);
    }
}

static int eventRegisterCallback(zloop_t* loop, zsock_t* reader, void* arg)
{
    struct EventActorState* state = (struct EventActorState*)arg;
    struct EventCallbackRegister* registration;
    struct EventCallbackRegister* current;
    zmsg_t* msg;
    zframe_t* frame;
    byte* frameData;

    (void)loop;

    msg = zmsg_recv(reader);
    if(!msg)
    {
        return 0;
    }
    frame = zmsg_pop(msg);
    zmsg_destroy(&msg);
    if(!frame)
    {
        return 0;
    }
    frameData = zframe_data(frame);
    if(!frameData)
    {
        zframe_destroy(&frame);
        return 0;
    }
    if(frameData[0] == '$')
    {
        if(strstr((const char*)frameData, "$TERM"))
        {
            state->terminate = true;
            zframe_destroy(&frame);
            return -1;
        }
    }
    registration = malloc(sizeof(struct EventCallbackRegister));
    if(!registration)
    {
        zframe_destroy(&frame);
        return -1;
    }
    memcpy(registration, frameData, sizeof(struct EventCallbackRegister));
    zframe_destroy(&frame);
    if(registration->unregister == false)
    {
        zlist_append(state->registrations, registration);
        zlist_freefn(state->registrations, registration, freeRegistration, true);
    }
    else
    {
        //Find the registration on the zlist...
        current = zlist_first(state->registrations);
        if(!current)
        {
            //No more registrations end the thread...
            state->terminate = true;
            free(registration);
            return -1;
        }
        while(current)
        {
            if(current->context == NULL && registration->context != NULL)
            {
                current = zlist_next(state->registrations);
                continue;
            }
            if(current->callback == registration->callback ||
               (registration->context == NULL || strcmp(current->context, registration->context) == 0))
            {
                zlist_remove(state->registrations, current);
                if(registration->context)
                {
                    free(registration->context);
                }
                if(current->context)
                {
                    free(current->context);
                }
                if(zlist_size(state->registrations) == 0)
                {
                    //No more registrations end the thread...
                    state->terminate = true;
                    free(registration);
                    return -1;
                }
            }

            current = zlist_next(state->registrations);
        }
        free(registration);
    }
    //Coverity complains about registration going out of scope, but in the case of adding to the zlist it doesn't so
    //this is a false positive.
    return 0;
}

static int eventReceivedCallback(zloop_t* loop, zsock_t* reader, void* arg)
{
    struct EventActorState* state = (struct EventActorState*)arg;
    char* msg;
    char* body;
    enumeratorAuthentication* auth;
    json_t* jBody;
    json_t* jContext;
    const char* context = NULL;
    struct EventCallbackRegister* current;
    redfishPayload* payload;

    (void)loop;

    if(!state || state->terminate == true)
    {
        return -1;
    }

    msg = zstr_recv(reader);
    if(!msg)
    {
        return 0;
    }

    body = strstr(msg, "\n\n");
    if(!body)
    {
        free(msg);
        return 0;
    }
    jBody = json_loads(body, 0, NULL);
    if(!jBody)
    {
        //Not in JSON format...
        free(msg);
        return 0;
    }

    if(strncmp("Authorization None", msg, 18) == 0)
    {
        auth = NULL;
    }
    else
    {
        printf("TODO Auth!\n");
        auth = malloc(sizeof(enumeratorAuthentication));
        if(auth)
        {
        }
    }

    jContext = json_object_get(jBody, "Context");
    if(jContext)
    {
        context = json_string_value(jContext);
    }

    if(context)
    {
        //Call each callback where context matches or the registration is NULL
        current = zlist_first(state->registrations);
        while(current)
        {
            if(current->context == NULL || strcmp(current->context, context) == 0)
            {
                payload = createRedfishPayload(jBody, current->service);

                current->callback(payload, auth, context);

                //Don't call cleanupPayload()... we want to leave the json value intact
                free(payload);
            }

            current = zlist_next(state->registrations);
        }
    }
    else
    {
        //Call each callback
        current = zlist_first(state->registrations);
        while(current)
        {
            payload = createRedfishPayload(jBody, current->service);

            current->callback(payload, auth, context);

            //Don't call cleanupPayload()... we want to leave the json value intact
            free(payload);

            current = zlist_next(state->registrations);
        }
    }
    json_decref(jBody);
    if(auth)
    {
        free(auth);
    }
    free(msg);
    return 0;
}

static void eventActorTask(zsock_t* pipe, void* args)
{
    struct EventActorState state;
    zloop_t* loop;
    zsock_t* remote;

    //args should always be null. Shut up compiler warning about it
    (void)args;

    state.terminate = false;
    state.registrations = zlist_new();
    if(!state.registrations)
    {
        zsock_signal(pipe, 0);
        return;
    }

    loop = zloop_new();
    if(!loop)
    {
        zlist_destroy(&state.registrations);
        zsock_signal(pipe, 0);
        return;
    }
    //zloop_set_verbose(loop, true);
    zloop_reader(loop, pipe, eventRegisterCallback, &state);

    //Open 0mq socket for redfish events
    remote = REDFISH_EVENT_0MQ_LIBRARY_NEW_SOCK;
    if(!remote)
    {
        zloop_destroy(&loop);
        zlist_destroy(&state.registrations);
        zsock_signal(pipe, 0);
        return;
    }

    zloop_reader(loop, remote, eventReceivedCallback, &state);

    //Inform the parent we are active
    zsock_signal(pipe, 0);

    zloop_start(loop);

    zloop_destroy(&loop);
    zsock_destroy(&remote);
    zlist_destroy(&state.registrations);
}

static void cleanupEventActor()
{
    if(eventActor && zactor_is(eventActor))
    {
        zactor_destroy(&eventActor);
    }
}

static void addStringToJsonArray(json_t* array, const char* value)
{
    json_t* jValue = json_string(value);

    json_array_append(array, jValue);

    json_decref(jValue);
}
#endif

static void addStringToJsonObject(json_t* object, const char* key, const char* value)
{
    json_t* jValue = json_string(value);

    json_object_set(object, key, jValue);

    json_decref(jValue);
}
