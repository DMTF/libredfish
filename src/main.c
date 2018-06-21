//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <stdlib.h>

#include <redfish.h>

libRedfishDebugFunc gDebugFunc = NULL;

void libredfishSetDebugFunction(libRedfishDebugFunc debugFunc)
{
#ifdef _DEBUG
    gDebugFunc = debugFunc;
#else
    (void)debugFunc;
#endif
}

#ifdef STANDALONE
#include <iostream>
#include <getopt.h>

static struct option long_options[] =
{
    {"help",     no_argument,       0,      '?'},
    {"version",  no_argument,       0,      'V'},
    {"host",     required_argument, 0,      'H'},
    {"method",   required_argument, 0,      'M'},
    {0, 0, 0, 0}
};

void print_usage(const char* name)
{
    printf("Usage: %s [OPTIONS] [Query]\n\n", name);
    printf("Test libRedfish.\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -?, --help                 Display this usage message\n");
    printf("  -V, --version              Display the software version\n");
    printf("  -M, --method               The HTTP method to use (Default is GET if not specified)\n");
    printf("  -H, --host                 The host to query\n");
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

int main(int argc, char** argv)
{
    int                   arg;
    int                   opt_index  = 0;
    unsigned int          method = 0;
    char*                 host = NULL;
    redfishService*       redfish = NULL;
    redfishPayload*  payload;
    bool             have_query = false;
    bool             have_leaf  = false;
    std::string      query;
    std::string      leaf;

    while((arg = getopt_long(argc, argv, "?VH:M:", long_options, &opt_index)) != -1)
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
                    std::cerr << "Error! Unknown Method " << optarg << "!\n";
                    return 1;
                }
                break;
            case 'H':
                host = strdup(optarg);
                break;
        }
    }
    if(host == NULL)
    {
        print_usage(argv[0]);
        return 1;
    }
        redfish = createServiceEnumerator(host, NULL, NULL);

        if(optind < argc)
        {
            query = argv[optind++];
            have_query = true;
        }
        switch(method)
        {
            default:
                break;
            case 1:
                if(have_query)
                {
                    std::size_t pos = query.rfind("/");
                    if(pos != (std::size_t)-1)
                    {
                        leaf = query.substr(pos+1);
                        query = query.substr(0, pos);
                        have_leaf = true;
                    }
                }
        }
        if(have_query)
        {
            payload = getPayloadByPath(redfish, query.c_str());
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
                if(have_leaf && optind < argc)
                {
                    bool res = patchPayloadStringProperty(payload, leaf.c_str(), argv[optind]);
                    std::cout << "PATCH to " << query << ": " << (res?"Success":"Failed!") << "\n";
                }
                else if(have_leaf)
                {
                    std::cerr << "Missing value for PATCH!\n";
                }
                else
                {
                    std::cerr << "Missing property for PATCH!\n";
                }
        }
    cleanupPayload(payload);
    cleanupServiceEnumerator(redfish);
    free(host);
    return 0;
}

#endif
/* vim: set tabstop=4 shiftwidth=4 expandtab: */
