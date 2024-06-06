#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory.h>

struct index_hdr_s *data;
struct stat stats;
int fd;

struct block {
    uint8_t occupied;
    struct index_s *start;
    struct index_s *end;
};

struct {
    struct block *first;
    struct block *last;
} blocks;

struct taskParam {
    int id;
};

pthread_mutex_t mutex;
pthread_barrier_t barrier;

uint64_t min(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

void mapFile(char *filename) {
    fd = open(filename, O_RDWR);

    fstat(fd, &stats);

    data = mmap(NULL, stats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
}

void unmap() {
    munmap(data, stats.st_size);
    close(fd);
}

int indexComparator(struct index_s *a, struct index_s *b) {
    return a->time_mark - b->time_mark;
}

void sortTask(int id) {
    printf("THREAD %04d ::: Starting sorting...\n", id);

    struct block *cur = blocks.first;
    while (1) {
        pthread_mutex_lock(&mutex);
        for (; cur != blocks.last; cur++) {
            if (!cur->occupied) {
                cur->occupied = 1;
                break;
            }
        }
        pthread_mutex_unlock(&mutex);
        if (cur == blocks.last)
            break;
        qsort(cur->start, cur->end - cur->start, sizeof(*cur->start), &indexComparator);

        printf("THREAD %04d ::: Sorted %p - %p block\n", id, (void*)cur->start, (void*)cur->end);
    }
}

void mergeTask(int id) {
    struct block *cur = blocks.first;
    while (1) {
        pthread_mutex_lock(&mutex);
        for (; cur + 1 < blocks.last; cur += 2)
            if (!cur[0].occupied && !cur[1].occupied) {
                cur[0].occupied = 1;
                cur[1].occupied = 1;
                break;
            }
        pthread_mutex_unlock(&mutex);
        if (cur + 1 >= blocks.last) break;

        printf("THREAD %04d ::: Merging blocks: %p - %p\n", id, (void*)cur[0].start, (void*)cur[1].end);
        struct index_s *next = cur[0].start;
        for (; cur[1].start != cur[1].end; cur[0].end++, cur[1].start++) {
            struct index_s temp = *cur[1].start;

            struct index_s *j = next;
            for (; j != cur[0].end && indexComparator(j, &temp) < 0; j++);
            memmove(j + 1, j, (cur[0].end - j) * sizeof(*j));
            *j = temp;
            next = j;
        }
        printf("THREAD %04d ::: Merged blocks: %p - %p\n", id, (void*)cur[0].start, (void*)cur[1].end);
    }
}

void task(struct taskParam *params) {
    int id = params->id;
    free(params);

    sortTask(id);
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    while (blocks.first != blocks.last) {
        mergeTask(id);
        pthread_barrier_wait(&barrier);
        pthread_barrier_wait(&barrier);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Use sorter [blocks] [threads] [filename]");
        return -1;
    }

    int blocksAmount, threadsAmount;
    char *filename = argv[3];

    if (sscanf(argv[1], "%d", &blocksAmount) != 1 || blocksAmount == 0) {
        printf("Blocks should be a positive decimal number.\n");
        return 2;
    }

    if (sscanf(argv[2], "%d", &threadsAmount) != 1 || threadsAmount == 0) {
        printf("Threads should be a positive decimal number.\n");
        return 3;
    }

    mapFile(filename);

    blocksAmount = min(blocksAmount, data->records);
    int blockSize = data->records / blocksAmount;

    struct block blk[blocksAmount];
    for (int i = 0; i < blocksAmount; i++) {
        blk[i].occupied = 0;
        blk[i].start = &data->idx[i * blockSize];
        blk[i].end = &data->idx[(i + 1) * blockSize];
    }
    blk[blocksAmount - 1].end = &data->idx[data->records];

    blocks.first = blk;
    blocks.last = blk + blocksAmount;

    pthread_mutex_init(&mutex, NULL);
    pthread_barrier_init(&barrier, NULL, threadsAmount + 1);

    pthread_t threads[threadsAmount];
    for (int i = 0; i < threadsAmount; i++) {
        struct taskParam *params = malloc(sizeof(struct taskParam));
        params->id = i;

        pthread_create(&threads[i], NULL, task, params);
    }

    pthread_barrier_wait(&barrier);

    printf("MAIN THREAD  ::: Unoccupying blocks\n");
    for (struct block *cur = blocks.first; cur != blocks.last; cur++)
        cur->occupied = 0;
    printf("MAIN THREAD ::: Blocks unoccupied\n");
    pthread_barrier_wait(&barrier);

    while (1) {
        pthread_barrier_wait(&barrier);
        if (blocks.last - blocks.first <= 1) {
            blocks.first = blocks.last;
            pthread_barrier_wait(&barrier);
            break;
        }
        for (struct block *to = blocks.first, *from = blocks.first; from < blocks.last; to += 1, from += 2)
            *to = *from;

        blocks.last -= (blocks.last - blocks.first) / 2;
        for (struct block *cur = blocks.first; cur != blocks.last; cur++)
            cur->occupied = 0;
        printf("MAIN THREAD ::: Blocks unoccupied\n");
        pthread_barrier_wait(&barrier);
    }

    for (int i = 0; i < threadsAmount; i++)
        pthread_join(threads[i], NULL);

    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&mutex);

    unmap();

    return 0;
}