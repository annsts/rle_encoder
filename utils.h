#ifndef _UTILS_H_
#define _UTILS_H_

// Struct to hold all information about the task 
typedef struct task {
    char *data;
    size_t size;
    char *encoded;
    size_t *final_size;
    struct task *next;         
    int order;
    int is_processed;
} task;

// Struct for working task queue 
typedef struct {
    task *head;
    task *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} t_queue;

void encode(const char *input, size_t length, char **output, size_t *output_size);
char* process_files(int argc, char *argv[], uint64_t *total_size_out);

#endif