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
    
    init_memory_pool(p_proc_mngr);
    init_thread_pool(p_proc_mngr);

    free_thread_pool(p_proc_mngr);
    free_memory_pool(p_proc_mngr);
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
    // initial task list
    pthread_mutex_init(&p_proc_mngr->task_list.mutex, NULL);
    pthread_cond_init(&p_proc_mngr->task_list.empty, NULL);

    // initial resolver & requester threads pool
    pthread_mutex_init(&p_proc_mngr->requester_pool.mutex, NULL);
    for(int idx = 0; idx < p_proc_mngr->requester_threads_count; idx ++)
        pthread_create(&p_proc_mngr->requester_pool.requester_ids[idx], NULL, requester_thread, p_proc_mngr);

    pthread_mutex_init(&p_proc_mngr->resolver_pool.mutex, NULL);
    for(int idx = 0; idx < p_proc_mngr->resolver_threads_count; idx ++)
        pthread_create(&p_proc_mngr->resolver_pool.resolver_ids[idx], NULL, resolver_thread, p_proc_mngr);

    // join threads pool
    for(int idx = 0; idx < p_proc_mngr->requester_threads_count; idx ++)
        pthread_join(p_proc_mngr->requester_pool.requester_ids[idx], NULL);

    for(int idx = 0; idx < p_proc_mngr->resolver_threads_count; idx ++)
        pthread_join(p_proc_mngr->resolver_pool.resolver_ids[idx], NULL);
}


void free_thread_pool(P_PROC_MNGR p_proc_mngr)
{
    // clean up task list
    pthread_mutex_destroy(&p_proc_mngr->task_list.mutex);
    pthread_cond_destroy(&p_proc_mngr->task_list.empty);

    // clean up threads pool
    pthread_mutex_destroy(&p_proc_mngr->requester_pool.mutex);
    pthread_mutex_destroy(&p_proc_mngr->resolver_pool.mutex);
}


void init_memory_pool(P_PROC_MNGR p_proc_mngr)
{
    P_MEMORY_POOL p_memory_pool = &p_proc_mngr->memory_pool;

    for(int i = 0; i < MEMORY_POOL_SIZE; i ++)
    {
        P_NODE p_node = (P_NODE)malloc(sizeof(NODE));
        put_node(p_memory_pool, p_node, NODE_FREE);
    }
}


void free_memory_pool(P_PROC_MNGR p_proc_mngr)
{
    P_MEMORY_POOL p_memory_pool = &p_proc_mngr->memory_pool;

    for(P_NODE p_node = get_node(p_memory_pool, NODE_FREE_NO_MALLOC); p_node != NULL; p_node = get_node(p_memory_pool, NODE_FREE))
        if(p_node != NULL) free(p_node);

    for(P_NODE p_node = get_node(p_memory_pool, NODE_USED); p_node != NULL; p_node = get_node(p_memory_pool, NODE_USED))
        if(p_node != NULL) free(p_node);
}


P_NODE get_node(P_MEMORY_POOL p_memory_pool, int flag)
{
    P_NODE p_node = NULL;

    if(flag == NODE_FREE || flag == NODE_FREE_NO_MALLOC)
    {
        p_node = p_memory_pool->free;
        if(p_node == NULL && flag == NODE_FREE) return (P_NODE)malloc(sizeof(NODE));
        else return NULL;
        p_memory_pool->free = p_node->next;
    }
    else
    {
        p_node = p_memory_pool->used;
        if(p_node == NULL) return NULL;
        p_memory_pool->used = p_node->next;
    }

    return p_node;
}


void put_node(P_MEMORY_POOL p_memory_pool, P_NODE p_node, int flag)
{
    p_node->next = NULL;

    if(flag == NODE_FREE)
    {
        if(p_memory_pool->free == NULL)
        {
            p_memory_pool->free = p_node;
        }
        else
        {
            p_node->next = p_memory_pool->free;
            p_memory_pool->free = p_node;
        }
    }
    else
    {
        if(p_memory_pool->used == NULL)
        {
            p_memory_pool->used = p_node;
        }
        else
        {
            p_node->next = p_memory_pool->used;
            p_memory_pool->used = p_node;
        }
    }
}


void fill_tasks_helper(P_PROC_MNGR p_proc_mngr)
{
    for(int i = 0; i < ARRAY_SIZE; i ++)
    {
        P_NODE p_node = NULL;
        if(p_proc_mngr->task_list.tasks[i].flag != TASK_DONE) continue;

        get_node(&p_proc_mngr->memory_pool, NODE_USED);
        if(p_node == NULL) break;

        strcpy(p_proc_mngr->task_list.tasks[i].domain, p_node->domain);
        p_proc_mngr->task_list.tasks[i].flag = TASK_FREE;
        put_node(&p_proc_mngr->memory_pool, p_node, NODE_FREE);
    }
}


int fill_tasks(P_PROC_MNGR p_proc_mngr)
{
    // move content from queue to task list
    //      read file, queue is empty
    //      read queue, queue is not empty
    if(p_proc_mngr->memory_pool.free != NULL)
    {
        fill_tasks_helper(p_proc_mngr);
    }
    else
    {
        FILE *fp = NULL;
        char* path = p_proc_mngr->hostname_paths[--p_proc_mngr->hostname_paths_count];

        fp = fopen(path, "r");
        if(fp == NULL) return OP_FAILURE;

        while(!feof(fp))
        {
            P_NODE p_node = get_node(&p_proc_mngr->memory_pool, NODE_FREE);
            fgets(p_node->domain, MAX_NAME_LENGTH, fp);
            put_node(&p_proc_mngr->memory_pool, p_node, NODE_USED);
        }

        fclose(fp);
        fill_tasks_helper(p_proc_mngr);
    }
    
    return OP_SUCCESS;
}


void save_log(char* path, char* content)
{

}


void *requester_thread(void *argv)
{
    // increase active count
    P_PROC_MNGR p_proc_mngr = (P_PROC_MNGR)argv;
    MUTEX_OPR(&p_proc_mngr->requester_pool.mutex, p_proc_mngr->requester_pool.active_count++;);

    // try to get a input file path [with lock]
    //      succeed --> fill task list
    //      failed  --> continue the loop, if input file path is incorrect
    //      failed  --> terminate thread,  if hostname_paths_count <= 1
    pthread_mutex_lock(&p_proc_mngr->task_list.mutex);
    for(;;)
    {
        pthread_cond_wait(&p_proc_mngr->task_list.empty, &p_proc_mngr->task_list.mutex);
        printf("%lx  --->  receive empty signal\n",pthread_self());

        if(p_proc_mngr->hostname_paths_count < 1) break;
        if(fill_tasks(p_proc_mngr) == OP_FAILURE) continue;
    }
    pthread_mutex_unlock(&p_proc_mngr->task_list.mutex);

    // decrease active count
    MUTEX_OPR(&p_proc_mngr->requester_pool.mutex, p_proc_mngr->requester_pool.active_count--;);
    return NULL;
}


void *resolver_thread(void *argv)
{
    // increase pool active count
    P_PROC_MNGR p_proc_mngr = (P_PROC_MNGR)argv;
    MUTEX_OPR(&p_proc_mngr->resolver_pool.mutex, p_proc_mngr->resolver_pool.active_count++;);

    for(P_TASK p_task = NULL;;p_task = NULL)
    {
        // check termination condition [without lock]
        if(p_proc_mngr->requester_pool.active_count <= 0 &&
           p_proc_mngr->task_list.active_count <= 0) break;

        // try to get a task [with lock]
        //      succeed --> mark the flag as TASK_BUSY
        //      failed  --> terminate thread, if active count of requesters <= 0
        //      failed  --> notify requester, if active count of requesters > 0
        MUTEX_OPR(&p_proc_mngr->task_list.mutex, 
            for(int i = 0; i < ARRAY_SIZE; i ++)
            {
                if(p_proc_mngr->task_list.tasks[i].flag == TASK_FREE)
                {
                    p_proc_mngr->task_list.active_count --;
                    p_task = &p_proc_mngr->task_list.tasks[i];
                    p_task->flag = TASK_BUSY; break;
                }
            }
        );

        if(p_task == NULL)
        {
            if(p_proc_mngr->requester_pool.active_count <= 0) break;
            pthread_cond_signal(&p_proc_mngr->task_list.empty); continue;
        }

        // solve the task [without lock]
        //      write result to <results.txt> [with lock]
        //      mark the flag as TASK_DONE
        dnslookup(p_task->domain, p_task->address, MAX_IP_LENGTH);
        printf("%lx  --->  %s: %s\n",pthread_self(), p_task->domain, p_task->address);
        p_task->flag = TASK_DONE;
    }

    // decrease pool active count
    MUTEX_OPR(&p_proc_mngr->resolver_pool.mutex, p_proc_mngr->resolver_pool.active_count--;);
    return NULL;
}