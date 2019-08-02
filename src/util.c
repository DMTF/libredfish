//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifndef _MSC_VER
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <afunix.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "IPHLPAPI.lib")
#endif
#include "util.h"
#include "queue.h"
#include "debug.h"

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
    if(ret == NULL)
    {
        return NULL;
    }
    memcpy(ret, string, (end-string));
    ret[(end-string)] = 0;
    return ret;
}

#ifdef _MSC_VER
//Windows IP stuff
static void initWinsock()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret;

	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	wVersionRequested = MAKEWORD(2, 2);
	ret = WSAStartup(wVersionRequested, &wsaData);
	if(ret != 0)
	{
		REDFISH_DEBUG_ERR_PRINT("%s: WSAStartup returned %d!", __func__, ret);
	}
}

static wchar_t* windowsStringFromCString(const char* cstr)
{
	size_t inputLength = strlen(cstr);
	size_t ignore;
	wchar_t* ret = (wchar_t*)calloc(inputLength + 1, sizeof(wchar_t));

	mbstowcs_s(&ignore, ret, inputLength+1, cstr, inputLength+1);

	return ret;
}

static PIP_ADAPTER_ADDRESSES getAdapterByName(const char* _interface, DWORD interfaceType, PIP_ADAPTER_ADDRESSES* freeMe)
{
	ULONG size = 4096;
	ULONG ret;
	PIP_ADAPTER_ADDRESSES addresses = (PIP_ADAPTER_ADDRESSES)malloc(size);
	PIP_ADAPTER_ADDRESSES current;
	PIP_ADAPTER_ADDRESSES adapter = NULL;
	wchar_t* wideName;

	wideName = windowsStringFromCString(_interface);

	ret = GetAdaptersAddresses(interfaceType, 0, NULL, addresses, &size);
	if (ret != 0)
	{
		return NULL;
	}
	current = addresses;
	while (current)
	{
		if (strcmp(_interface, current->AdapterName) == 0 || wcscmp(wideName, current->FriendlyName) == 0)
		{
			adapter = current;
			break;
		}
	}
	free(wideName);
	*freeMe = addresses;
	return adapter;
}

static char* getWindowsIP(const char* _interface, DWORD addressType)
{
	PIP_ADAPTER_ADDRESSES bigBuff = NULL;
	PIP_ADAPTER_ADDRESSES adapter;
	DWORD rc;
	char* ret = NULL;
	DWORD strSize = 1024;
	wchar_t addressStr[1024] = { 0 };
	size_t maxLength;

	initWinsock();

	adapter = getAdapterByName(_interface, addressType, &bigBuff);
	if (adapter && adapter->FirstUnicastAddress)
	{
		rc = WSAAddressToStringW(adapter->FirstUnicastAddress->Address.lpSockaddr, adapter->FirstUnicastAddress->Address.iSockaddrLength, NULL, addressStr, &strSize);
		if (rc != 0)
		{
			printf("WSAAddressToString returned %d %d\n", rc, WSAGetLastError());
		}
		else
		{
			maxLength = wcslen(addressStr) + 1;
			ret = (char*)calloc(maxLength, 1);
			wcstombs_s(&maxLength, ret, maxLength, addressStr, strSize);
		}
	}
	if (bigBuff)
	{
		free(bigBuff);
	}

	WSACleanup();
	return ret;
}
#endif

char* getIpv4Address(const char* networkInterface)
{
#ifdef _MSC_VER
    return getWindowsIP(networkInterface, AF_INET);
#else
    int fd;
    struct ifreq ifr;
    const char* tmp;
    char buf[256];

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0)
    {
        REDFISH_DEBUG_WARNING_PRINT("Could not open socket! errno = %d", errno);
        return NULL;
    }

    memset(&ifr, 0, sizeof(ifr));

    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to "eth0" */
    strncpy(ifr.ifr_name, networkInterface, IFNAMSIZ-1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    tmp = inet_ntop(AF_INET, &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), buf, sizeof(buf));
    return safeStrdup(tmp);
#endif
}

char* getIpv6Address(const char* networkInterface)
{
#ifdef _MSC_VER
    return getWindowsIP(networkInterface, AF_INET6);
#else
    struct ifaddrs* ifap;
    struct ifaddrs* current;
    char host[NI_MAXHOST] = {0};

    if(getifaddrs(&ifap) < 0)
    {
        REDFISH_DEBUG_WARNING_PRINT("getifaddrs returned error! errno = %d", errno);
        return NULL;
    }
    current = ifap;
    while(current)
    {
        /*Look only for an interface that has an address, the address is IPv6, and the name matches*/
        if(current->ifa_addr != NULL && current->ifa_addr->sa_family == AF_INET6 && strcmp(current->ifa_name, networkInterface) == 0)
        {
            break;
        }

        current = current->ifa_next;
    }
    if(current == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("Could not locate interface with name \"%s\"", networkInterface);
        freeifaddrs(ifap);
        return NULL;
    }
    if(getnameinfo(current->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) != 0)
    {
        REDFISH_DEBUG_WARNING_PRINT("getnameinfo returned error! errno = %d", errno);
        freeifaddrs(ifap);
        return NULL;
    }

    freeifaddrs(ifap);
    return safeStrdup(host);
#endif
}

SOCKET getSocket(const char* ip, unsigned int* portNum)
{
	SOCKET ret;
    int domain = AF_INET;
    struct sockaddr_in6 ip6_socket_struct;
    struct sockaddr_in ip4_socket_struct;
    struct sockaddr* addr = (struct sockaddr*)&ip4_socket_struct;
    socklen_t size = sizeof(struct sockaddr_in);
    if(strstr(ip, ":"))
    {
        domain = AF_INET6;
        inet_pton(AF_INET6, ip, (void *)&ip6_socket_struct.sin6_addr.s6_addr);
        ip6_socket_struct.sin6_family = AF_INET6;
        ip6_socket_struct.sin6_port = htons(*portNum);
        addr = (struct sockaddr*)&ip6_socket_struct;
        size = sizeof(struct sockaddr_in6);
    }
    else
    {
        inet_pton(AF_INET, ip, (void *)&ip4_socket_struct.sin_addr.s_addr);
        ip4_socket_struct.sin_family = AF_INET;
        ip4_socket_struct.sin_port = htons(*portNum);
    }
    ret = socket(domain, SOCK_STREAM, 0);
    if(ret < 0)
    {
        return ret;
    }
    if(bind(ret, addr, size) < 0)
    {
        socketClose(ret);
        return -1;
    }
    listen(ret, 5);
    //If portNum is passed in as 0 then need to return the actual socket number used...
    if(!(*portNum))
    {
        if(getsockname(ret, addr, &size) == -1)
        {
            REDFISH_DEBUG_WARNING_PRINT("%s: getsockname returned error! errno = %d", __func__, errno);
        }
        else
        {
            if(domain == AF_INET6)
            {
                *portNum = ntohs(ip6_socket_struct.sin6_port);
            }
            else
            {
                *portNum = ntohs(ip4_socket_struct.sin_port);
            }
        }
    }
    return ret;
}

SOCKET getDomainSocket(const char* name)
{
    SOCKET ret;
    struct sockaddr_un addr;

    ret = socket(AF_UNIX, SOCK_STREAM, 0);
    if(ret < 0)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Unable to open socket\n", __func__);
        return ret;
    }
    unlink(name);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, name, sizeof(addr.sun_path)-1);
    if(bind(ret, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Unable to bind socket %s\n", __func__, name);
		socketClose(ret);
        return -1;
    }
    listen(ret, 5);
    return ret;
}

thread getThreadId()
{
#ifndef _MSC_VER
    return pthread_self();
#else
    return GetCurrentThread();
#endif
}

void addStringToJsonArray(json_t* array, const char* value)
{
    json_t* jValue = json_string(value);

    json_array_append(array, jValue);

    json_decref(jValue);
}

void socketClose(SOCKET socket)
{
#ifdef _MSC_VER
	closesocket(socket);
#else
    close(socket);
#endif
}
