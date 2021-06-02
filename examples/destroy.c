#include <stdio.h>
#include <getopt.h>
#include <redfish.h>
#include <string.h>

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

#define strcasecmp _stricmp
#else
#include <syslog.h>
#endif

int verbose = LOG_CRIT;

static struct option long_options[] =
{
    {"help",       no_argument,       0,      '?'},
    {"version",    no_argument,       0,      'V'},
    {"host",       required_argument, 0,      'H'},
    {"workaround", required_argument, 0,      'W'},
    {"username",   required_argument, 0,      'u'},
    {"password",   required_argument, 0,      'p'},
    {"verbose",    no_argument,       0,      'v'},
    {0, 0, 0, 0}
};

#ifndef _MSC_VER
__attribute__((format(printf, 2, 3)))
#endif
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
    printf("Test libRedfish's session destroy logic.\n\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -?, --help                 Display this usage message\n");
    printf("  -V, --version              Display the software version\n");
    printf("  -H, --host                 The host to query\n");
    printf("  -v, --verbose              Log more information\n");
    printf("  -u, --username [user]      The username to authenticate with\n");
    printf("  -p, --password [pass]      The password to authenticate with\n");
    printf("Report bugs to Patrick_Boyd@Dell.com\n");
}

void print_version()
{
    printf("libRedfish Destroy Test Tool\n");
    printf("Written by Patrick Boyd.\n");
}


int main(int argc, char** argv)
{
    int              arg;
    int              opt_index  = 0;
    char*            host = NULL;
    redfishService*  redfish = NULL;
    char*            username = NULL;
    char*            password = NULL;
    unsigned int     flags = 0;
    bool             ret;
    char*            token = NULL;
    char*            sessionUri = NULL;
    enumeratorAuthentication auth;

    while((arg = getopt_long(argc, argv, "?VH:W:u:p:v", long_options, &opt_index)) != -1)
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
            case 'v':
                verbose++;
                break;
        }
    }

    if(host == NULL || username == NULL || password == NULL)
    {
        print_usage(argv[0]);
        return 1;
    }

    libredfishSetDebugFunction(syslogPrintf);

    auth.authType = REDFISH_AUTH_SESSION;
    auth.authCodes.userPass.username = username;
    auth.authCodes.userPass.password = password;
    redfish = createServiceEnumerator(host, NULL, &auth, flags);
    if(redfish == NULL)
    {
        fprintf(stderr, "Unable to create service enumerator\n");
        return -1;
    }

    ret = destroyServiceForSession(redfish, &token, &sessionUri);
    if(!ret)
    {
        fprintf(stderr, "Unable to destroy service ptr!\n");
        return -2;
    }
    printf("Session Token is %s\n", token);
    printf("Session URI is %s\n", sessionUri);
    return 0;
}