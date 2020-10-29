#include "util.h"
#include "multi-lookup.h"

int main(int argc, const char **argv)
{
    struct timeval beg, end;
    char time_log[64] = {0};

    // parse & check arguments
    P_PROC_MNGR p_proc_mngr = (P_PROC_MNGR)malloc(sizeof(PROC_MNGR));
    memset(p_proc_mngr, 0, sizeof(PROC_MNGR));

    if(parse_arguments(p_proc_mngr, argc, argv) == OP_FAILURE)
    {
        free(p_proc_mngr);
        return EXIT_FAILURE;
    }

    sprintf(p_proc_mngr->performance_report, 
            "Number of requester threads is %d\nNumber of resolver threads is %d\n",
            p_proc_mngr->requester_threads_count, p_proc_mngr->resolver_threads_count);
    
    init_memory_pool(p_proc_mngr);
    TIME_LAP(&beg, &end, NULL, init_thread_pool(p_proc_mngr););

    // save or append log to <performance.txt>
    sprintf(time_log, 
            "%s: total time is %f seconds\n\n", 
            argv[0], ((end.tv_sec - beg.tv_sec) * 1000000.0 + end.tv_usec - beg.tv_usec) / 1000000.0);
    strcat(p_proc_mngr->performance_report, time_log);
    save_log("./performance.txt", p_proc_mngr->performance_report);
    
    free_thread_pool(p_proc_mngr);
    free_memory_pool(p_proc_mngr);
    free(p_proc_mngr);
    return EXIT_SUCCESS;
}


int parse_arguments(P_PROC_MNGR p_proc_mngr, int argc, const char **argv)
{
    FILE * fp = NULL;
    if(argc < 5)
    {
        printf(PROGRAM_USAGE);
        return OP_FAILURE;
    }
    
    p_proc_mngr->requester_threads_count = atoi(argv[1]) > MAX_REQUESTER_THREADS ? MAX_REQUESTER_THREADS : atoi(argv[1]);
    p_proc_mngr->resolver_threads_count  = atoi(argv[2]) > MAX_RESOLVER_THREADS ? MAX_RESOLVER_THREADS : atoi(argv[2]);
    p_proc_mngr->p_requester_log_path = (char *)argv[3];
    p_proc_mngr->p_resolver_log_path  = (char *)argv[4];
    
    // accept at most <MAX_INPUT_FILES> of input hostname file paths
    for(int idx = 5; idx < argc && p_proc_mngr->hostname_paths_count < MAX_INPUT_FILES; idx ++)
    {
        p_proc_mngr->hostname_paths[p_proc_mngr->hostname_paths_count ++] = (char *)argv[idx];
    }

    // only check the existence of the output path, not necessary for files [overwrite if file exists]
    fp = fopen(p_proc_mngr->p_requester_log_path, "w");
    if(fp == NULL)
    {
        fprintf(stderr, "Bogus output File Path: [ %s ]\n", p_proc_mngr->p_requester_log_path);
        return OP_FAILURE;
    }
    fclose(fp);

    fp = fopen(p_proc_mngr->p_resolver_log_path, "w");
    if(fp == NULL)
    {
        fprintf(stderr, "Bogus output File Path: [ %s ]\n", p_proc_mngr->p_requester_log_path);
        return OP_FAILURE;
    }
    fclose(fp);

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
    pthread_cond_init(&p_proc_mngr->task_list.ready, NULL);

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
    pthread_cond_destroy(&p_proc_mngr->task_list.ready);

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

    for(P_NODE p_node = get_node(p_memory_pool, NODE_FREE_NO_MALLOC); p_node != NULL; p_node = get_node(p_memory_pool, NODE_FREE_NO_MALLOC))
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
        if(p_node == NULL && flag == NODE_FREE_NO_MALLOC) return NULL;
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
            P_NODE p_curr_node = p_memory_pool->free;
            while(p_curr_node->next != NULL) p_curr_node = p_curr_node->next;
            p_curr_node->next = p_node;
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
            P_NODE p_curr_node = p_memory_pool->used;
            while(p_curr_node->next != NULL) p_curr_node = p_curr_node->next;
            p_curr_node->next = p_node;
        }
    }
}


void fill_tasks_helper(P_PROC_MNGR p_proc_mngr)
{
    for(int i = 0; i < ARRAY_SIZE; i ++)
    {
        P_NODE p_node = NULL;
        if(p_proc_mngr->task_list.tasks[i].flag != TASK_DONE) continue;

        p_node = get_node(&p_proc_mngr->memory_pool, NODE_USED);
        if(p_node == NULL) break;

        strcpy(p_proc_mngr->task_list.tasks[i].domain, p_node->domain);
        p_proc_mngr->task_list.tasks[i].flag = TASK_FREE;
        p_proc_mngr->task_list.active_count ++;
        put_node(&p_proc_mngr->memory_pool, p_node, NODE_FREE);
    }
}


int fill_tasks(P_PROC_MNGR p_proc_mngr, int* p_total_serviced_file)
{
    // move content from queue to task list
    //      read file, queue is empty
    //      read queue, queue is not empty
    if(p_proc_mngr->memory_pool.used != NULL)
    {
        fill_tasks_helper(p_proc_mngr);
    }
    else
    {
        FILE *fp = NULL;
        
        // skip bogus input file
        for(;;)
        {
            char* path = NULL;
            if(p_proc_mngr->hostname_paths_count < 1) return OP_FAILURE;
            path = p_proc_mngr->hostname_paths[--p_proc_mngr->hostname_paths_count];

            fp = fopen(path, "r");
            if(fp == NULL)
            {
                fprintf(stderr, "Bogus input File Path: [ %s ]", path);
                continue;
            }
            break;
        }

        while(!feof(fp))
        {
            char* find_enter = NULL;
            P_NODE p_node = get_node(&p_proc_mngr->memory_pool, NODE_FREE);
            memset(p_node->domain, 0, MAX_NAME_LENGTH); // adding this just for valgrind
            
            fgets(p_node->domain, MAX_NAME_LENGTH, fp);

            // this code cause [Invalid read of size 1]
            //p_node->domain[strlen(p_node->domain) - 1] = '\0';    

            find_enter = strchr(p_node->domain, '\n');
            if(find_enter) *find_enter = '\0';
            
            if(strlen(p_node->domain) == 0) put_node(&p_proc_mngr->memory_pool, p_node, NODE_FREE);
            else put_node(&p_proc_mngr->memory_pool, p_node, NODE_USED);
        }

        fclose(fp);
        (*p_total_serviced_file) ++;
        fill_tasks_helper(p_proc_mngr);
    }
    
    return OP_SUCCESS;
}


int save_log(char* path, char* content)
{
    FILE* fp = fopen(path, "a");
    if(fp == NULL) return OP_FAILURE;

    fwrite(content, strlen(content), 1, fp);
    fclose(fp);
    return OP_SUCCESS;
}


void *requester_thread(void *argv)
{
    int total_serviced_file = 0;
    char log_content[64] = {0};

    // increase active count
    P_PROC_MNGR p_proc_mngr = (P_PROC_MNGR)argv;
    MUTEX_OPR(&p_proc_mngr->requester_pool.mutex, p_proc_mngr->requester_pool.active_count++;)

    // try to get a input file path [with lock]
    //      succeed --> fill task list
    //      failed  --> continue the loop, if input file path is incorrect
    //      failed  --> terminate thread,  if hostname_paths_count <= 1
    MUTEX_OPR(&p_proc_mngr->task_list.mutex,
        for(;;)
        {
            pthread_cond_wait(&p_proc_mngr->task_list.empty, &p_proc_mngr->task_list.mutex);
            if(p_proc_mngr->hostname_paths_count < 1 && p_proc_mngr->memory_pool.used == NULL) break;
            if(p_proc_mngr->task_list.active_count > 0) continue;
            if(fill_tasks(p_proc_mngr, &total_serviced_file) == OP_FAILURE) break;
            pthread_cond_broadcast(&p_proc_mngr->task_list.ready);
        }
    )

    // decrease active count & write log to <serviced.txt> & performance report
    sprintf(log_content, "Thread %lx serviced %d files.\n", pthread_self(), total_serviced_file);
    MUTEX_OPR(&p_proc_mngr->requester_pool.mutex, 
        p_proc_mngr->requester_pool.active_count--;
        save_log(p_proc_mngr->p_requester_log_path, log_content);
        strcat(p_proc_mngr->performance_report, log_content);
    )

    // in case when current thread is the last one, while there still resolvers wait for ready condition
    // only notify resolvers after reduce the active count
    pthread_cond_broadcast(&p_proc_mngr->task_list.ready);
    return NULL;
}


P_TASK get_task(P_PROC_MNGR p_proc_mngr)
{
    P_TASK p_task = NULL;

    for(int i = 0; i < ARRAY_SIZE; i ++)
    {
        if(p_proc_mngr->task_list.tasks[i].flag == TASK_FREE)
        {
            p_proc_mngr->task_list.active_count --;
            p_task = &p_proc_mngr->task_list.tasks[i];
            p_task->flag = TASK_BUSY; break;
        }
    }

    return p_task;
}


void *resolver_thread(void *argv)
{
    char log_content[MAX_NAME_LENGTH + MAX_IP_LENGTH] = {0};

    // increase pool active count
    P_PROC_MNGR p_proc_mngr = (P_PROC_MNGR)argv;
    MUTEX_OPR(&p_proc_mngr->resolver_pool.mutex, p_proc_mngr->resolver_pool.active_count++;)

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
            p_task = get_task(p_proc_mngr);

            if(p_task == NULL)
            {
                if(p_proc_mngr->requester_pool.active_count <= 0) break;
                pthread_cond_broadcast(&p_proc_mngr->task_list.empty);
                pthread_cond_wait(&p_proc_mngr->task_list.ready, &p_proc_mngr->task_list.mutex);
                pthread_mutex_unlock(&p_proc_mngr->task_list.mutex); continue;
            }
        )

        memset(p_task->address, 0, MAX_IP_LENGTH);
        dnslookup(p_task->domain, p_task->address, MAX_IP_LENGTH);
        sprintf(log_content, "%s,%s\n", p_task->domain, p_task->address);
        MUTEX_OPR(&p_proc_mngr->resolver_pool.mutex, save_log(p_proc_mngr->p_resolver_log_path, log_content);)
        p_task->flag = TASK_DONE;
    }

    // decrease pool active count
    MUTEX_OPR(&p_proc_mngr->resolver_pool.mutex, p_proc_mngr->resolver_pool.active_count--;);
    return NULL;
}