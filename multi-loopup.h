#ifndef MULTI_LOOPUP_H
#define MULTI_LOOPUP_H

#include <pthread.h>
#include <unistd.h>

#define ARRAY_SIZE            20
#define MAX_INPUT_FILES       10
#define MAX_RESOLVER_THREADS  10
#define MAX_REQUESTER_THREADS 5
#define MAX_NAME_LENGTH       1025              // not used
#define MAX_IP_LENGTH         INET6_ADDRSTRLEN  // not used

#define OP_SUCCESS  0
#define OP_FAILURE -1

#define PROGRAM_USAGE "\
NAME\n\n\
multi-lookup - resolve a set of hostnames to IP addresses\n\n\
SYNOPSIS\n\n\
multi-lookup <# requester> <# resolver> <requester log> <resolver log> [ <data file> ... ]\n\n\
DESCRIPTION\n\n\
The file names specified by <data file> are passed to the pool of requester threads which place information into a shared data area.\
Resolver threads read the shared data area and find the corresponding IP address.\n\n\
<# requesters> number of requestor threads to place into the thread pool.\n\n\
<# resolvers> number of resolver threads to place into the thread pool.\n\n\
<requester log> name of the file into which all the requester status information is written.\n\n\
<resolver log> name of the file into which all the resolver status information is written.\n\n\
<data file> filename to be processed. Each file contains a list of host names, one per line, that are to be resolved.\n\n"

typedef struct __MAIN_ARGS
{
    int requester_threads_count;
    int resolver_threads_count;
    int hostname_paths_count;
    char* p_requester_log_path;
    char* p_resolver_log_path;
    char* hostname_paths[MAX_INPUT_FILES];

}MAIN_ARGS, * P_MAIN_ARGS;

void *requester_thread(void *);
void *resolver_thread(void *);
int parse_arguments(P_MAIN_ARGS, int, const char **);

#endif