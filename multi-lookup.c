#include "util.h"
#include "multi-loopup.h"

int main(int argc, const char **argv)
{
    // parse & check arguments
    P_PROC_MNGR p_proc_mngr = (P_PROC_MNGR)malloc(sizeof(PROC_MNGR));
    memset(p_proc_mngr, 0, sizeof(PROC_MNGR));

    if(parse_arguments(p_proc_mngr, argc, argv) == OP_FAILURE)
    {
        printf(PROGRAM_USAGE);
        free(p_proc_mngr);
        return EXIT_FAILURE;
    }
    
    init_thread_pool(p_proc_mngr);

    printf("%d, %d\n", p_proc_mngr->requester_pool.active_count, p_proc_mngr->resolver_pool.active_count);

    free(p_proc_mngr);
    return EXIT_SUCCESS;
}


int parse_arguments(P_PROC_MNGR p_proc_mngr, int argc, const char **argv)
{
    if(argc < 5) return OP_FAILURE;
    
    p_proc_mngr->requester_threads_count = atoi(argv[1]) > MAX_REQUESTER_THREADS ? MAX_REQUESTER_THREADS : atoi(argv[1]);
    p_proc_mngr->resolver_threads_count  = atoi(argv[2]) > MAX_RESOLVER_THREADS ? MAX_RESOLVER_THREADS : atoi(argv[2]);
    p_proc_mngr->p_requester_log_path = (char *)argv[3];
    p_proc_mngr->p_resolver_log_path  = (char *)argv[4];
    
    // accept at most <MAX_INPUT_FILES> of input hostname file paths
    for(int idx = 5; idx < argc && p_proc_mngr->hostname_paths_count < MAX_INPUT_FILES; idx ++)
    {
        p_proc_mngr->hostname_paths[p_proc_mngr->hostname_paths_count ++] = (char *)argv[idx];
    }

    // arguments sanity check
    return (p_proc_mngr->requester_threads_count == 0 || 
            p_proc_mngr->resolver_threads_count == 0 || 
            p_proc_mngr->hostname_paths_count == 0 || 
            strlen(p_proc_mngr->hostname_paths[0]) == 0 ||
            strlen(p_proc_mngr->p_requester_log_path) == 0 || 
            strlen(p_proc_mngr->p_resolver_log_path) == 0) ? 
            OP_FAILURE : OP_SUCCESS;
}


void init_thread_pool(P_PROC_MNGR p_proc_mngr)
{
    // initial resolver & requester thread pool
    init_resolvers(p_proc_mngr);
    init_requesters(p_proc_mngr);

    // join threads
    for(int idx = 0; idx < p_proc_mngr->requester_threads_count; idx ++)
        pthread_join(p_proc_mngr->requester_pool.requester_ids[idx], NULL);

    for(int idx = 0; idx < p_proc_mngr->resolver_threads_count; idx ++)
        pthread_join(p_proc_mngr->resolver_pool.resolver_ids[idx], NULL);

    // clean up
    pthread_mutex_destroy(&p_proc_mngr->requester_pool.mutex);
    pthread_mutex_destroy(&p_proc_mngr->resolver_pool.mutex);
}


void init_requesters(P_PROC_MNGR p_proc_mngr)
{
    pthread_mutex_init(&p_proc_mngr->requester_pool.mutex, NULL);

    for(int idx = 0; idx < p_proc_mngr->requester_threads_count; idx ++)
        pthread_create(&p_proc_mngr->requester_pool.requester_ids[idx], NULL, requester_thread, p_proc_mngr);
}


void init_resolvers(P_PROC_MNGR p_proc_mngr)
{
    pthread_mutex_init(&p_proc_mngr->resolver_pool.mutex, NULL);

    for(int idx = 0; idx < p_proc_mngr->resolver_threads_count; idx ++)
        pthread_create(&p_proc_mngr->resolver_pool.resolver_ids[idx], NULL, resolver_thread, p_proc_mngr);
}


void *requester_thread(void *argv)
{
    // increase active count
    P_PROC_MNGR p_proc_mngr = (P_PROC_MNGR)argv;
    MUTEX_OPR(&p_proc_mngr->requester_pool.mutex, p_proc_mngr->requester_pool.active_count++;);


    // decrease active count
    MUTEX_OPR(&p_proc_mngr->requester_pool.mutex, p_proc_mngr->requester_pool.active_count--;);
    return NULL;
}


void *resolver_thread(void *argv)
{
    // increase active count
    P_PROC_MNGR p_proc_mngr = (P_PROC_MNGR)argv;
    MUTEX_OPR(&p_proc_mngr->resolver_pool.mutex, p_proc_mngr->resolver_pool.active_count++;);


    // decrease active count
    MUTEX_OPR(&p_proc_mngr->resolver_pool.mutex, p_proc_mngr->resolver_pool.active_count--;);
    return NULL;
}