#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/time.h>

pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t can_read = PTHREAD_COND_INITIALIZER;
pthread_cond_t can_write = PTHREAD_COND_INITIALIZER;

int reader_count = 0;
int writer_active = 0;
int write_fd;
struct timeval program_start_time;

double get_elapsed_time() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    double elapsed = (current_time.tv_sec - program_start_time.tv_sec) + 
                     (current_time.tv_usec - program_start_time.tv_usec) / 1000000.0;
    return elapsed;
}

typedef struct {
    char type;
    int processing_time;
    int thread_id;
} thread_arg_t;

void* reader_thread(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;
    
    printf("[%.3f] Reader#%d: Created!\n", get_elapsed_time(), targ->thread_id);
    
    pthread_mutex_lock(&reader_count_mutex);
    while (writer_active) {
        pthread_cond_wait(&can_read, &reader_count_mutex);
    }
    reader_count++;
    if (reader_count == 1) {
        pthread_mutex_lock(&resource_mutex);
    }
    pthread_mutex_unlock(&reader_count_mutex);

    printf("[%.3f] Reader#%d: Read started! (reading %d ms)\n", 
           get_elapsed_time(), targ->thread_id, targ->processing_time);
    usleep(targ->processing_time * 1000);

    pthread_mutex_lock(&reader_count_mutex);
    reader_count--;
    if (reader_count == 0) {
        pthread_mutex_unlock(&resource_mutex);
        pthread_cond_signal(&can_write);
    }
    pthread_mutex_unlock(&reader_count_mutex);
    
    printf("[%.3f] Reader#%d: Terminated\n", get_elapsed_time(), targ->thread_id);
    free(arg);
    return NULL;
}

void* writer_thread(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;
    
    printf("[%.3f] Writer#%d: Created!\n", get_elapsed_time(), targ->thread_id);

    pthread_mutex_lock(&reader_count_mutex);
    while (writer_active || reader_count > 0) {
        pthread_cond_wait(&can_write, &reader_count_mutex);
    }
    writer_active = 1;
    pthread_mutex_unlock(&reader_count_mutex);

    pthread_mutex_lock(&resource_mutex);

    printf("[%.3f] Writer#%d: Write started! (writing %d ms)\n", 
           get_elapsed_time(), targ->thread_id, targ->processing_time);
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "Writer %d processed for %d ms\n", 
             targ->thread_id, targ->processing_time);
    write(write_fd, buffer, strlen(buffer));
    usleep(targ->processing_time * 1000);

    pthread_mutex_unlock(&resource_mutex);

    pthread_mutex_lock(&reader_count_mutex);
    writer_active = 0;
    pthread_cond_broadcast(&can_read);
    pthread_cond_signal(&can_write);
    pthread_mutex_unlock(&reader_count_mutex);

    printf("[%.3f] Writer#%d: Terminated\n", get_elapsed_time(), targ->thread_id);
    free(arg);
    return NULL;
}

int main(int argc, char *argv[]) {
    FILE *sequence_file;
    char line[256];
    char type;
    int processing_time;
    int thread_counter = 0;
    pthread_t threads[100];
    int thread_count = 0;

    gettimeofday(&program_start_time, NULL);

    if (argc != 2) {
        puts("데이터 입력 파일이 존재하지 않습니다.");
        printf("Usage: %s <sequence file>\n", argv[0]);
        exit(1);
    }

    if ((sequence_file = fopen(argv[1], "r")) == NULL) {
        puts("sequence file open() Error!");
        exit(2);
    }

    if ((write_fd = open("output.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
        puts("output file open() Error!");
        fclose(sequence_file);
        exit(3);
    }

    while (fgets(line, sizeof(line), sequence_file) != NULL) {
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        if (sscanf(line, "%c %d", &type, &processing_time) != 2) {
            printf("Invalid line format: %s", line);
            continue;
        }

        thread_arg_t* arg = malloc(sizeof(thread_arg_t));
        if (arg == NULL) {
            puts("Memory allocation failed!");
            break;
        }

        arg->type = type;
        arg->processing_time = processing_time;
        arg->thread_id = ++thread_counter;

        if (type == 'R' || type == 'r') {
            if (pthread_create(&threads[thread_count], NULL, reader_thread, arg) != 0) {
                perror("Failed to create reader thread");
                free(arg);
                continue;
            }
        } else if (type == 'W' || type == 'w') {
            if (pthread_create(&threads[thread_count], NULL, writer_thread, arg) != 0) {
                perror("Failed to create writer thread");
                free(arg);
                continue;
            }
        } else {
            printf("Unknown thread type: %c\n", type);
            free(arg);
            continue;
        }

        thread_count++;

        usleep(100000);
    }

    printf("End of sequence.\n");

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    fclose(sequence_file);
    close(write_fd);
    
    pthread_mutex_destroy(&resource_mutex);
    pthread_mutex_destroy(&reader_count_mutex);
    pthread_cond_destroy(&can_read);
    pthread_cond_destroy(&can_write);

    return 0;
}