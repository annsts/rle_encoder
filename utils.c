#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h> 
#include <ctype.h>

// Struct to hold information about a task 
typedef struct task {
    char *data;
    size_t size;
    char *encoded;
    size_t *final_size;
    struct task *next;         
    int order;
    int is_processed;
} task;

/* This is a function to encode char[] using RLE. */
void encode(const char *input, size_t length, char **output, size_t *output_size) {
    // set maximum output_size to 4 times the length 
    size_t max_output_size = 4 * length; 
    // allocate memeory for the output 
    *output = malloc(max_output_size);
    // handle allocation failed 
    if (*output == NULL) {
        perror("allocation failed");
        exit(EXIT_FAILURE);
    }
    // initialize variable to track actual size of the output 
    size_t j = 0;
    for (size_t i = 0; i < length; i++) {
        // variable to track current char 
        unsigned char current = input[i];
        int count = 1;
        // if the characters are the same, increase count 
        while (i < length - 1 && input[i] == input[i + 1]) {
            count++;
            i++;
        }
        // assuming character cannot appear more than 255 in a row
        if (count > 255) {
            count = 255;
        }
        // set the output
        (*output)[j++] = current; // character
        (*output)[j++] = (unsigned char)count; // 1-byte unsigned 
    }
    // adjust the size of the output to actual size 
    *output_size = j;
    // reallocate 
    *output = realloc(*output, *output_size); 
    // handle reallocation error 
    if (*output == NULL) {
        perror("reallocation failed");
        exit(EXIT_FAILURE);
    }
}

/* This is function to process files and concatenate them if needed. */
char* process_files(int argc, char *argv[], uint64_t *total_size_out) {
    // set the variables for total size of the file 
    uint64_t total_size = 0;
    // iterate over args and get the total size of the files 
    struct stat st;
    for (int i = optind; i < argc; i++) {
        if (stat(argv[i], &st) == -1) {
            perror("Couldn't get file size");
            continue;
        }
        total_size += st.st_size;
    }
    // https://stackoverflow.com/questions/46943690/how-to-use-stat-to-check-if-commandline-argument-is-a-directory

    // allocate memory for all files
    char *all_input = malloc(total_size + 1); // +1 for null-terminator
    if (all_input == NULL) {
        perror("allocation for combined files failed");
        return NULL;
    }
    // track size of the files to concatenate them 
    off_t current_offset = 0;
    // open files 
    for (int i = optind; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            perror("error opening file");
            continue;
        }
        if (fstat(fd, &st) == -1) {
            perror("couldn't get file size");
            close(fd);
            continue;
        }
        // write at concatenated buffer exactly st.st_size bytes 
        if (read(fd, all_input + current_offset, st.st_size) != st.st_size) {
            perror("error reading file");
            close(fd);
            free(all_input);
            return NULL;
        }
        // increase offset by a file size that was just written 
        current_offset += st.st_size;
        close(fd);
    }
    // add null terminator at the end
    all_input[current_offset] = '\0'; 
    // update total size 
    *total_size_out = total_size; 
    return all_input;
}