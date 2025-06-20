#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_PATH 1024

//  전역 변수 
typedef struct {
    char** buffer;
    int size;
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} bounded_buffer_t;

typedef struct {
    int index;
} thread_arg_t;

bounded_buffer_t job_queue;

char keyword[256];
int matches_total = 0;
pthread_mutex_t matches_mutex = PTHREAD_MUTEX_INITIALIZER;

int scan_complete = 0;

int files_scanned = 0;
pthread_mutex_t files_mutex = PTHREAD_MUTEX_INITIALIZER;

//  문자열 함수 
int case_insensitive_search(const char* line, const char* word) {
    int count = 0;
    char* lower_line = strdup(line);
    char* lower_word = strdup(word);
    
    for (int i = 0; lower_line[i]; i++) lower_line[i] = tolower(lower_line[i]);
    for (int i = 0; lower_word[i]; i++) lower_word[i] = tolower(lower_word[i]);

    char* pos = lower_line;
    while ((pos = strstr(pos, lower_word)) != NULL) {
        count++;
        pos++;
    }
    free(lower_line);
    free(lower_word);
    return count;
}

//  버퍼 연산 
void buffer_init(bounded_buffer_t* b, int size) {
    b->buffer = malloc(sizeof(char*) * size);
    b->size = size;
    b->front = b->rear = b->count = 0;
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->not_full, NULL);
    pthread_cond_init(&b->not_empty, NULL);
}

void buffer_push(bounded_buffer_t* b, char* item) {
    pthread_mutex_lock(&b->mutex);
    while (b->count == b->size) {
        pthread_cond_wait(&b->not_full, &b->mutex);
    }
    b->buffer[b->rear] = item;
    b->rear = (b->rear + 1) % b->size;
    b->count++;
    pthread_cond_signal(&b->not_empty);
    pthread_mutex_unlock(&b->mutex);
}

char* buffer_pop(bounded_buffer_t* b) {
    pthread_mutex_lock(&b->mutex);
    while (b->count == 0 && !scan_complete) {
        pthread_cond_wait(&b->not_empty, &b->mutex);
    }
    if (b->count == 0 && scan_complete) {
        pthread_mutex_unlock(&b->mutex);
        return NULL;
    }
    char* item = b->buffer[b->front];
    b->front = (b->front + 1) % b->size;
    b->count--;
    pthread_cond_signal(&b->not_full);
    pthread_mutex_unlock(&b->mutex);
    return item;
}

//  파일 탐색 
void file_searcher(char* path, int thread_index, int file_index) {
    FILE* file = fopen(path, "r");
    if (!file) return;

    char line[4096];
    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        count += case_insensitive_search(line, keyword);
    }
    fclose(file);

    printf("[Thread#%d-%d] %s: %d found\n", thread_index, file_index, path, count);
    pthread_mutex_lock(&matches_mutex);
    matches_total += count;
    pthread_mutex_unlock(&matches_mutex);
    free(path);
}

//  디렉토리 순회 함수 
void search_directory(const char* dirname) {
    DIR* dir = opendir(dirname);
    if (!dir) return;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

        struct stat st;
        if (stat(path, &st) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            search_directory(path);
        } else if (S_ISREG(st.st_mode)) {
            char* filepath = strdup(path);
            buffer_push(&job_queue, filepath);
            pthread_mutex_lock(&files_mutex);
            files_scanned++;
            pthread_mutex_unlock(&files_mutex);
        }
    }
    closedir(dir);
}

//  워커 스레드 
void* worker_thread(void* arg) {
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    int thread_index = t_arg->index;
    printf("[Thread#%d] started searching '%s'...\n", thread_index, keyword);
    int file_index = 0;
    while (1) {
        char* path = buffer_pop(&job_queue);
        if (!path) break;
        file_searcher(path, thread_index, file_index);
        file_index++;
    }
    free(t_arg);
    return NULL;
}

//  메인 함수 
int main(int argc, char* argv[]) {
    int opt;
    int b = 0, t = 0;
    char* d = NULL;
    char* w = NULL;

    while ((opt = getopt(argc, argv, "b:t:d:w:")) != -1) {
        switch (opt) {
            case 'b': b = atoi(optarg); break;
            case 't': t = atoi(optarg); break;
            case 'd': d = optarg; break;
            case 'w': w = optarg; break;
            default:
                fprintf(stderr, "Usage: %s -b <buffer size> -t <threads> -d <directory> -w <word>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!b || !t || !d || !w) {
        fprintf(stderr, "Missing arguments.\n");
        exit(EXIT_FAILURE);
    }

    printf("Buffer size=%d, Num threads=%d, Directory=%s, Searchword=%s\n", b, t, d, w);

    strncpy(keyword, w, sizeof(keyword));
    buffer_init(&job_queue, b);

    pthread_t threads[t];
    for (int i = 0; i < t; i++) {
        thread_arg_t* t_arg = malloc(sizeof(thread_arg_t));
        t_arg->index = i;
        pthread_create(&threads[i], NULL, worker_thread, t_arg);
    }

    search_directory(d);

    pthread_mutex_lock(&job_queue.mutex);
    scan_complete = 1;
    pthread_cond_broadcast(&job_queue.not_empty);
    pthread_mutex_unlock(&job_queue.mutex);

    for (int i = 0; i < t; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Total found = %d (Num files=%d)\n", matches_total, files_scanned);
    return 0;
}