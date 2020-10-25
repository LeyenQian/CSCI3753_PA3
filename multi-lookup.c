#include "util.h"
#include "multi-loopup.h"

int main(int argc, const char **argv)
{
    // parse & check arguments
    P_MAIN_ARGS p_main_args = (P_MAIN_ARGS)malloc(sizeof(MAIN_ARGS));
    if(parse_arguments(p_main_args, argc, argv) == OP_FAILURE)
    {
        printf(PROGRAM_USAGE);
        free(p_main_args);
        return EXIT_FAILURE;
    }

    // initial requester thread pool

    // initial resolver thread pool

    free(p_main_args);
    return EXIT_SUCCESS;
}

int parse_arguments(P_MAIN_ARGS p_main_args, int argc, const char **argv)
{
    if(argc < 5) return OP_FAILURE;
    memset(p_main_args, 0, sizeof(MAIN_ARGS));
    
    p_main_args->requester_threads_count = atoi(argv[1]) > MAX_REQUESTER_THREADS ? MAX_REQUESTER_THREADS : atoi(argv[1]);
    p_main_args->resolver_threads_count  = atoi(argv[2]) > MAX_RESOLVER_THREADS ? MAX_RESOLVER_THREADS : atoi(argv[2]);
    p_main_args->p_requester_log_path = (char *)argv[3];
    p_main_args->p_resolver_log_path  = (char *)argv[4];
    
    // accept at most <MAX_INPUT_FILES> of input hostname file paths
    for(int idx = 5; idx < argc && p_main_args->hostname_paths_count < MAX_INPUT_FILES; idx ++)
    {
        p_main_args->hostname_paths[p_main_args->hostname_paths_count ++] = (char *)argv[idx];
    }

    // arguments sanity check
    return (p_main_args->requester_threads_count == 0 || 
            p_main_args->resolver_threads_count == 0 || 
            p_main_args->hostname_paths_count == 0 || 
            strlen(p_main_args->hostname_paths[0]) == 0 ||
            strlen(p_main_args->p_requester_log_path) == 0 || 
            strlen(p_main_args->p_resolver_log_path) == 0) ? 
            OP_FAILURE : OP_SUCCESS;
}

void *requester_thread(void *argv)
{

}

void *resolver_thread(void *argv)
{

}