#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread_checksum_lab.h"

static int parse_positive_int(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || v <= 0 || v > 1000000000L) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int parse_positive_u32(const char *s, uint32_t *out) {
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (*s == '\0' || *end != '\0' || v > 0xFFFFFFFFUL) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t value_for_index(size_t index, int rounds, uint32_t seed) {
    uint32_t x = seed ^ (uint32_t)(index * 2654435761u);
    for (int r = 0; r < rounds; ++r) {
        x = mix32(x + (uint32_t)r * 2246822519u);
    }
    return x;
}

void *worker_run(void *arg) {
    WorkerArgs *w = (WorkerArgs *)arg;
    uint64_t sum = 0;
    uint32_t xor = 0;
    uint32_t max = 0;

    /* TODO: compute partial_sum, partial_xor, partial_max for range [start, end). */
    for (size_t i = w->start; i < w->end; ++i){
        uint32_t value = value_for_index(i, w->rounds, w->seed);
        sum += value;
        xor ^= value;
        if (value > max) {
            max = value;
        }
        w->partial_sum = sum;
        w->partial_xor = xor;
        w->partial_max = max;
    }

    (void)w;

    return NULL;
}

int run_single(WorkerArgs *args, uint64_t *sum, uint32_t *x, uint32_t *maxv) {
    if (args == NULL || sum == NULL || x == NULL || maxv == NULL) {
        return -1;
    }

    if (worker_run(args) != NULL) {
        return -1;
    }

    *sum = args->partial_sum;
    *x = args->partial_xor;
    *maxv = args->partial_max;
    return 0;
}

int run_multi(WorkerArgs *args, int thread_count, uint64_t *sum, uint32_t *x, uint32_t *maxv) {
    if (args == NULL || thread_count <= 0 || sum == NULL || x == NULL || maxv == NULL) {
        return -1;
    }

    pthread_t *threads = (pthread_t *)calloc((size_t)thread_count, sizeof(pthread_t));
    if (threads == NULL) {
        perror("calloc");
        return -1;
    }

    /* TODO: create one thread per chunk and join all threads. */
    free(threads);
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "usage: %s <mode:single|multi> <threads> <items> <rounds> <seed>\\n", argv[0]);
        return 2;
    }

    const char *mode = argv[1];
    int thread_count = 0;
    int items = 0;
    int rounds = 0;
    uint32_t seed = 0;

    if (parse_positive_int(argv[2], &thread_count) != 0 ||
        parse_positive_int(argv[3], &items) != 0 ||
        parse_positive_int(argv[4], &rounds) != 0 ||
        parse_positive_u32(argv[5], &seed) != 0) {
        fprintf(stderr, "invalid numeric argument\\n");
        return 2;
    }

    if (strcmp(mode, "single") != 0 && strcmp(mode, "multi") != 0) {
        fprintf(stderr, "mode must be single or multi\\n");
        return 2;
    }

    if (thread_count > items) {
        thread_count = items;
    }

    WorkerArgs *jobs = (WorkerArgs *)calloc((size_t)thread_count, sizeof(WorkerArgs));
    if (jobs == NULL) {
        perror("calloc");
        return 1;
    }

    size_t base = (size_t)items / (size_t)thread_count;
    size_t rem = (size_t)items % (size_t)thread_count;
    size_t cursor = 0;

    for (int i = 0; i < thread_count; ++i) {
        size_t width = base + ((size_t)i < rem ? 1u : 0u);
        jobs[i].start = cursor;
        jobs[i].end = cursor + width;
        jobs[i].rounds = rounds;
        jobs[i].seed = seed;
        jobs[i].partial_sum = 0;
        jobs[i].partial_xor = 0;
        jobs[i].partial_max = 0;
        cursor += width;
    }

    uint64_t sum = 0;
    uint32_t x = 0;
    uint32_t maxv = 0;

    int rc;
    if (strcmp(mode, "single") == 0) {
        jobs[0].start = 0;
        jobs[0].end = (size_t)items;
        rc = run_single(&jobs[0], &sum, &x, &maxv);
        thread_count = 1;
    } else {
        rc = run_multi(jobs, thread_count, &sum, &x, &maxv);
    }

    free(jobs);

    if (rc != 0) {
        fprintf(stderr, "TODO: implement thread checksum logic\\n");
        return 1;
    }

    printf("result: sum=%" PRIu64 " xor=%" PRIu32 " max=%" PRIu32 "\\n", sum, x, maxv);
    printf("meta: mode=%s threads=%d items=%d rounds=%d seed=%" PRIu32 "\\n",
           mode, thread_count, items, rounds, seed);

    return 0;
}
