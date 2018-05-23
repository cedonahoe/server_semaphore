#include "server_impl.h"
#include "request.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

int buffsize;
int* buffer;
int fill = 0;
int take = 0;

sem_t empty;
sem_t full;
sem_t mutex;

void* consume();

typedef struct {
    sem_t lock;
    sem_t writelock;
    int readers;
} rwlock_t;

rwlock_t *rw;

void getreadlock(rwlock_t *rw) {
    sem_wait(&rw->lock);
    rw->readers++;
    if (rw->readers == 1)
      sem_wait(&rw->writelock);
    sem_post(&rw->lock);
}

void releasereadlock(rwlock_t *rw) {
    sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0)
      sem_post(&rw->writelock);
    sem_post(&rw->lock);
}

void getwritelock(rwlock_t *rw) {
    sem_wait(&rw->writelock);
}

void releasewritelock(rwlock_t *rw) {
    sem_post(&rw->writelock);
}

void server_init(int argc, char* argv[]) {
    if (argc < 4 || argc > 4) {
        fprintf(stderr, "Usage: %s <port> <threads> <buffers>\n", argv[0]);
        exit(1);
    }
    int threads  = atoi(argv[2]);
    if (threads < 1) {
      exit(1);
    }
     buffsize = atoi(argv[3]);
    if (buffsize < 1) {
      exit(1);
    }
    buffer = malloc(sizeof(int) * buffsize);
    for (int x = 0 ; x < buffsize; x ++) {
      buffer[x] = 0;
    }
    rw = malloc(sizeof(rwlock_t));
    sem_init(&empty, 0, buffsize);
    sem_init(&full, 0, 0);
    rw->readers = 0;
    sem_init(&rw->lock, 0, 1);
    sem_init(&rw->writelock, 0, 1);
    pthread_t *pthreads;
    pthreads = malloc(sizeof(pthread_t) * threads);
    for (int i = 0; i < threads; i ++) {
      pthread_create(&pthreads[i], NULL, consume, NULL);
      pthread_detach(pthread_self());
    }
}


void insert(void* arg) {
    int inject = *(int*)arg;
    free(arg);
    buffer[fill] = inject;
    fill = (fill + 1) % buffsize;
}

int get() {
    int got = buffer[take];
    take = (take + 1) % buffsize;
    return got;
}

void* consume() {
    int tmp = 0;
    while (1) {
      sem_wait(&full);
      getreadlock(rw);
      tmp = get();
      releasereadlock(rw);
      sem_post(&empty);
      requestHandle(tmp);
    }
}

void server_dispatch(int connfd) {
    // Handle the request in the main thread.
    int* arg = malloc(sizeof(int));
    *arg = connfd;
    sem_wait(&empty);
    getwritelock(rw);
    insert(arg);
    releasewritelock(rw);
    sem_post(&full);
    return;
}
