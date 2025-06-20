#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define MAX_THREADS 100

typedef struct {
    sem_t mutex;          
    sem_t queue;          
    sem_t writeLock;   
    sem_t readTry;   
    int readCount;      
    int waitingWriters;   
} rwlock_t;

void rwlock_init(rwlock_t *rw) {
    sem_init(&rw->mutex, 0, 1);
    sem_init(&rw->queue, 0, 1);
    sem_init(&rw->writeLock, 0, 1);
    sem_init(&rw->readTry, 0, 1);
    rw->readCount = 0;
    rw->waitingWriters = 0;
}

void rwlock_acquire_readlock(rwlock_t *rw) {
    sem_wait(&rw->queue);      
    sem_wait(&rw->readTry);
    sem_wait(&rw->mutex);
    rw->readCount++;
    if (rw->readCount == 1)
        sem_wait(&rw->writeLock);
    sem_post(&rw->mutex);
    sem_post(&rw->readTry);

}

void rwlock_release_readlock(rwlock_t *rw) {
    sem_wait(&rw->mutex);
    rw->readCount--;
    if (rw->readCount == 0) {
        sem_post(&rw->writeLock);
        sem_post(&rw->queue); 
    }
    sem_post(&rw->mutex);
}

void rwlock_acquire_writelock(rwlock_t *rw) {
    sem_wait(&rw->mutex);
    rw->waitingWriters++;
    sem_post(&rw->mutex);

    sem_wait(&rw->queue);
    sem_wait(&rw->readTry);
    sem_wait(&rw->writeLock);

    sem_wait(&rw->mutex);
    rw->waitingWriters--;
    sem_post(&rw->mutex);
}

void rwlock_release_writelock(rwlock_t *rw) {
    sem_post(&rw->writeLock);
    sem_post(&rw->readTry);
    sem_post(&rw->queue);
}


long long get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}

typedef struct {
    int id;
    int time_ms;
} thread_arg_t;

rwlock_t rw;

void *reader(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    printf("[%.4f] Reader#%d: Created!\n\n", (get_timestamp_ms() % 100000) / 1000.0, targ->id);

    rwlock_acquire_readlock(&rw);
    printf("[%.4f] Reader#%d: Read started! (reading %d ms)\n\n", (get_timestamp_ms() % 100000) / 1000.0, targ->id, targ->time_ms);
    usleep(targ->time_ms * 1000);
    printf("[%.4f] Reader#%d: Terminated\n\n", (get_timestamp_ms() % 100000) / 1000.0, targ->id);

    rwlock_release_readlock(&rw);

    free(targ);
    return NULL;
}

void *writer(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    printf("[%.4f] Writer#%d: Created!\n\n", (get_timestamp_ms() % 100000) / 1000.0, targ->id);

    rwlock_acquire_writelock(&rw);
    printf("[%.4f] Writer#%d: Write started! (writing %d ms)\n\n", (get_timestamp_ms() % 100000) / 1000.0, targ->id, targ->time_ms);
    usleep(targ->time_ms * 1000);
    printf("[%.4f] Writer#%d: Terminated\n\n", (get_timestamp_ms() % 100000) / 1000.0, targ->id);

    rwlock_release_writelock(&rw);

    free(targ);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <sequence file>\n", argv[0]);
        exit(1);
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("fopen");
        exit(1);
    }

    rwlock_init(&rw);

    char line[128];
    pthread_t threads[MAX_THREADS];
    int tcnt = 0, rid = 1, wid = 1;

    while (fgets(line, sizeof(line), fp)) {
        if (tcnt >= MAX_THREADS) break;
        char type;
        int t;
        if (sscanf(line, "%c %d", &type, &t) != 2) continue;

        thread_arg_t *arg = malloc(sizeof(thread_arg_t));
        arg->time_ms = t;
        if (type == 'R') {
            arg->id = rid++;
            pthread_create(&threads[tcnt++], NULL, reader, arg);
        } else if (type == 'W') {
            arg->id = wid++;
            pthread_create(&threads[tcnt++], NULL, writer, arg);
        }
        usleep(100 * 1000); 
    }

    fclose(fp);

    for (int i = 0; i < tcnt; i++)
        pthread_join(threads[i], NULL);

    printf("End of sequence.\n");
    return 0;
}