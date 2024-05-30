#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h> 
#include <pthread.h>
#include <ctype.h>
#include "utils.h" 

// initialize a global variable to track the task order 
int task_order = 0; 

// declare output array and its size to hold processed tasks in order 
task **output_arr; 
int output_arr_size; 

// array mutex for proper thread synchronization 
pthread_mutex_t output_arr_mutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_cond_t output_arr_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t output_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

// flag to signal output thread to exit 
int should_exit = 0; 

/* This is a function to enqueue task in the queue with initial data and size. */
void enqueue(t_queue *queue, char *data, size_t size) {
    // allocate memory for a new task 
    task *new_task = malloc(sizeof(task));
    if (new_task == NULL) {
        perror("Failed to allocate new task");
        return;
    }
    new_task->data = data;
    // lock mutex 
    pthread_mutex_lock(&queue->mutex);
    // if task is not null, initialize size, final size and encoded data 
    if(new_task->data != NULL){
        new_task->size = size;
        new_task->encoded = NULL; 
        new_task->final_size = malloc(sizeof(size_t)); 
        *new_task->final_size = 0; 
        // increase task order to track its order
        new_task->order = task_order++;
    }
    new_task->next = NULL; 
    // add task to the queue 
    if (queue->tail != NULL) {
        queue->tail->next = new_task;
    } else {
        queue->head = new_task;
    }
    queue->tail = new_task;
    // signal new task in the queue
    pthread_cond_signal(&queue->cond);
    // unlock mutex 
    pthread_mutex_unlock(&queue->mutex);
}

/* This is a function to dequeue a task in the queue. */
task *dequeue(t_queue *queue) {
    // lock mutex 
    pthread_mutex_lock(&queue->mutex);
    // if the queue is empty, wait for signal 
    while (queue->head == NULL) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    // update head of the queue 
    task *task = queue->head;
    queue->head = task->next;
    // set tail to null if queue is empty 
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    // unlock mutex
    pthread_mutex_unlock(&queue->mutex);
    return task;
}
// Dequeue and enqueue reference: https://stackoverflow.com/questions/41914822/c-pthreads-issues-with-thread-safe-queue-implementation 

/* This is a function that dequeues and encodes each non-NULL task in the queue. */
void *worker(void *arg) {
    // get the queue from args 
    t_queue *queue = (t_queue *)arg;
    while (1) {
        // dequeue one task 
        task *task = dequeue(queue);
        // if signal to stop - break 
        if (task->data == NULL) { 
            free(task);
            break;
        }
        // encode the task's chunk 
        encode(task->data, task->size, &task->encoded, task->final_size); 
        task->is_processed = 1;
        // lock array mutex
        pthread_mutex_lock(&output_arr_mutex);
        // add the task to the output array at its correct position
        if (task->order >= 0 && task->order < output_arr_size) {
            output_arr[task->order] = task;
        } else { 
            fprintf(stderr, "out of bounds task order %d \n ", task->order); 
        } 
        // unlock array mutex
        pthread_mutex_unlock(&output_arr_mutex);

        //what 
        //pthread_cond_signal(&output_arr_cond);
    }
    return NULL;
}

// pthread, mutex and conditional variables references: 
// https://www.omscs-notes.com/operating-systems/threads-case-study-pthreads/
// https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html

// variables to hold the last written character and last written count 
char last_char = 0;
int last_count = 0;
// track order 
int next_order = 0;

/* This is a function to write the last character and count. */
void write_stitched() {
    if (last_count > 0) {
        unsigned char output[2] = {last_char, (unsigned char)last_count};
        fwrite(output, sizeof(char), 2, stdout);
        last_char = 0;
        last_count = 0;
    }
}

/* This is a function to check if the first and last characters
  are the same and write updated count to the output*/
void stitch_and_write(task *current_task) {
    // get the size of the current task 
    size_t size = *current_task->final_size;
    // if the size is greater than or equal to 2
    if (size >= 2) {
        // get the first character and the first count 
        char first_char = current_task->encoded[0];
        int first_count = (unsigned char)current_task->encoded[1];
        // if the last and first characters are the same 
        if (last_char == first_char) {
            // add counts 
            first_count += last_count;
            // assuming no character occurs for more than 255 times in a row 
            if (first_count > 255) first_count = 255;
            // modify the first count of the current task 
            current_task->encoded[1] = (unsigned char)first_count;
        } else {
            // otherwise if the size of the current task is less or equal to 2, write it 
            write_stitched();
        }
        // update last character and count 
        last_char = current_task->encoded[size - 2];
        last_count = (unsigned char)current_task->encoded[size - 1];
    }
    // write the current task to stdout 
    if (size > 2) {
        fwrite(current_task->encoded, sizeof(char), size - 2, stdout);
    }
}

/* This is a function to free memory allocated to the task. */
void clean(task *current_task) {
    free(current_task->final_size);
    free(current_task->encoded);
    free(current_task);
}

/* This is a function to process and write the encoded result to stdout. */
void *output() {
    while (1) {
        // lock mutex
        pthread_mutex_lock(&output_arr_mutex);
        // pthread_cond_wait(&output_arr_cond, &output_arr_mutex);
        // if there is available task in the output array 
        while (next_order < output_arr_size && output_arr[next_order] != NULL && output_arr[next_order]->is_processed) {
            // get the task 
            task *current_task = output_arr[next_order];
            // stitch the current task with the last character of the previous task if there are any 
            stitch_and_write(current_task);
            // free current task memory 
            clean(current_task);
            output_arr[next_order] = NULL;
            next_order++;
        }
        // unlock mutex
        pthread_mutex_unlock(&output_arr_mutex);
        // if there is signal to exit or if reached the end of the tasks 
        // pthread_mutex_lock(&output_thread_mutex);
        pthread_mutex_lock(&output_thread_mutex);
        int check = should_exit; 
        pthread_mutex_unlock(&output_thread_mutex);

        if (check && next_order >= task_order) {
            // pthread_mutex_unlock(&output_thread_mutex);
            // write the last remaning characters and break 
            write_stitched();
            break;
        }
    }
    return NULL;
}

/* This is main function that handles sequential and parallel encoding. */
int main(int argc, char *argv[]) {
    // by default number of threads is 1
    int num_threads = 1; 

    // get input 
    int option;
    while ((option = getopt(argc, argv, "j:")) != -1) {
        switch (option) {
            // if -j jobs is provided, update the number of threads 
            case 'j':
                num_threads = atoi(optarg);
                break;
            default: 
                exit(EXIT_FAILURE);
        }
    }

    // if no file provided, print error 
    if (argc == 1) {
        fprintf(stderr, "No files provided\n");
        exit(EXIT_FAILURE);
    }

    // variables to hold the total size of the input and the input 
    uint64_t total_size = 0;
    char *input = NULL; 
    // parallel RLE 
    if (num_threads > 1) {
        input = process_files(argc, argv, &total_size);
        if (input == NULL) {
             exit(EXIT_FAILURE);
        }
    
    // lock output array mutex
    pthread_mutex_lock(&output_arr_mutex);

    // calculate output array size based on 4KB chunks 
    if(total_size > 4096){
        output_arr_size = (total_size/4096) + 1;
    }
    // if total_size is less than 4KB set to 100 by default 
    else{
        output_arr_size = 100; 
    }
    // fprintf(stderr, "total tasks: %d \n", output_array_size); 

    // allocate memory for the output array and populate all to NULL 
    output_arr = malloc(output_arr_size * sizeof(task*)); 
    if (output_arr == NULL) {
        perror("Failed to allocate output array");
        exit(EXIT_FAILURE);
    }
    else{
        for (int i = 0; i < output_arr_size; i++){
            output_arr[i] = NULL; 
    }
    }
    // unlock output array mutex
    pthread_mutex_unlock(&output_arr_mutex);

    // initialize task queue 
    t_queue queue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

    // create output thread 
    pthread_t output_thread;
    if (pthread_create(&output_thread, NULL, output, NULL) != 0) {
        perror("Failed to create output thread");
        return 1;
    }

    // create threads 
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker, &queue) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // divide the input into 4KB chunks and submit tasks to the task queue 
    size_t remaining_size = total_size;
        while (remaining_size > 0) {
            size_t chunk_size = (remaining_size < 4096) ? remaining_size : 4096;
            enqueue(&queue, input + (total_size - remaining_size), chunk_size);
            remaining_size -= chunk_size;
        }

    // enqueue NULL tasks to indicate end of tasks 
    for (int i = 0; i < num_threads; i++) {
        enqueue(&queue, NULL, 0); 
    }

    // wait for all threads to finish
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // signal output thread to finish 
    pthread_mutex_lock(&output_thread_mutex);
    should_exit = 1;
    pthread_mutex_unlock(&output_thread_mutex);

    pthread_join(output_thread, NULL);

    // free output array 
    free(output_arr); 

    } 
    // sequential RLE 
    else {
        // process files and get concatenated input 
        input = process_files(argc, argv, &total_size);
        if (input == NULL) {
             exit(EXIT_FAILURE);
        }
        char *encoded = NULL;
        size_t final_size = 0;
        // encode and write the input 
        encode(input, total_size, &encoded, &final_size);
        fwrite(encoded, sizeof(char), final_size, stdout);
        free(encoded);
    }

    free(input);

    return 0;
}