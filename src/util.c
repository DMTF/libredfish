//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include "util.h"
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
    memcpy(ret, string, (end-string));
    ret[(end-string)] = 0;
    return ret;
}

char* getIpv4Address(const char* interface)
{
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
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    tmp = inet_ntop(AF_INET, &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), buf, sizeof(buf));
    return safeStrdup(tmp);
}

char* getIpv6Address(const char* interface)
{
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
        if(current->ifa_addr != NULL && current->ifa_addr->sa_family == AF_INET6 && strcmp(current->ifa_name, interface) == 0)
        {
            break;
        }

        current = current->ifa_next;
    }
    if(current == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("Could not locate interface with name \"%s\"", interface);
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
}

int getRandomSocket(const char* ip, unsigned int* portNum)
{
    int ret;
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
        ip6_socket_struct.sin6_port = 0;
        addr = (struct sockaddr*)&ip6_socket_struct;
        size = sizeof(struct sockaddr_in6);
    }
    else
    {
        inet_pton(AF_INET, ip, (void *)&ip4_socket_struct.sin_addr.s_addr);
        ip4_socket_struct.sin_family = AF_INET;
        ip4_socket_struct.sin_port = 0;
    }
    ret = socket(domain, SOCK_STREAM, 0);
    if(ret < 0)
    {
        return ret;
    }
    if(bind(ret, addr, size) < 0)
    {
        close(ret);
        return -1;
    }
    listen(ret, 5);
    if(portNum)
    {
        if(getsockname(ret, addr, &size) == -1)
        {
            REDFISH_DEBUG_WARNING_PRINT("getsockname returned error! errno = %d", errno);
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

thread getThreadId()
{
#ifndef _MSC_VER
    return pthread_self();
#else
    return GetCurrentThread();
#endif
}
