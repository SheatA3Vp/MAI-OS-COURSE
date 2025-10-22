#define _POSIX_C_SOURCE 199309L

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include "coordinates.h"

float max_area = 0.0;
int max_i = -1, max_j = -1, max_k = -1;

typedef struct {
    int start;
    int points_count;
    int threads_count;
} ThreadArgs;

typedef struct {
    float area;
    int i, j, k;
} Result;

void* FindTriangle(void* arg) {
    ThreadArgs* args = (ThreadArgs*) arg;
    int start = args->start;
    int n = args->points_count;
    int ths = args->threads_count;
    float local_max = 0.0;
    int local_i = -1, local_j = -1, local_k = -1;
    for (int i = start; i < n - 2; i += ths) {
        for (int j = i + 1; j < n - 1; ++j) {
            for (int k = j + 1; k < n; ++k) {
                // Считаем площадь
                float ax = coordinates[j][0] - coordinates[i][0];
                float ay = coordinates[j][1] - coordinates[i][1];
                float az = coordinates[j][2] - coordinates[i][2];
                float bx = coordinates[k][0] - coordinates[i][0];
                float by = coordinates[k][1] - coordinates[i][1];
                float bz = coordinates[k][2] - coordinates[i][2];
                float cx = ay * bz - az * by;
                float cy = az * bx - ax * bz;
                float cz = ax * by - ay * bx;
                float area = 0.5 * sqrt(cx*cx + cy*cy + cz*cz);
                if (area > local_max) {
                    local_max = area;
                    local_i = i;
                    local_j = j;
                    local_k = k;
                }
            }
        }
    }
    Result* res = malloc(sizeof(Result));
    res->area = local_max;
    res->i = local_i;
    res->j = local_j;
    res->k = local_k;
    return res;
}

void sequential_find(int n) {
    for (int i = 0; i < n - 2; ++i) {
        for (int j = i + 1; j < n - 1; ++j) {
            for (int k = j + 1; k < n; ++k) {
                float ax = coordinates[j][0] - coordinates[i][0];
                float ay = coordinates[j][1] - coordinates[i][1];
                float az = coordinates[j][2] - coordinates[i][2];
                float bx = coordinates[k][0] - coordinates[i][0];
                float by = coordinates[k][1] - coordinates[i][1];
                float bz = coordinates[k][2] - coordinates[i][2];
                float cx = ay * bz - az * by;
                float cy = az * bx - ax * bz;
                float cz = ax * by - ay * bx;
                float area = 0.5 * sqrt(cx*cx + cy*cy + cz*cz);
                if (area > max_area) {
                    max_area = area;
                    max_i = i;
                    max_j = j;
                    max_k = k;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        printf("Usage: %s <num_threads> [num_points]\n", argv[0]);
        return 1;
    }
    if (num_points < 3) {
        printf("Need at least 3 points to form a triangle\n");
        return 1;
    }
    char* endptr;
    long threads_amount_long = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || threads_amount_long < 1 || threads_amount_long > INT_MAX) {
        printf("Number of threads must be a positive integer\n");
        return 1;
    }
    int threads_amount = (int)threads_amount_long;

    int n;
    if (argc == 3) {
        long n_long = strtol(argv[2], &endptr, 10);
        if (*endptr != '\0' || n_long < 3 || n_long > num_points) {
            printf("Number of points must be between 3 and %d\n", num_points);
            return 1;
        }
        n = (int)n_long;
    } else {
        n = num_points;
    }

    struct timespec start, end;
    long long time_ms;

    if (threads_amount == 1) {
        // Последовательная версия
        clock_gettime(CLOCK_MONOTONIC, &start);
        sequential_find(n);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_ms = (end.tv_sec - start.tv_sec) * 1000LL + (end.tv_nsec - start.tv_nsec) / 1000000LL;
        printf("Sequential time: %lld ms\n", time_ms);
    } else {
        // Параллельная версия
        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_t* threads = malloc(threads_amount * sizeof(pthread_t));
        ThreadArgs* args = malloc(threads_amount * sizeof(ThreadArgs));
        for (int i = 0; i < threads_amount; ++i) {
            args[i].start = i;
            args[i].points_count = n;
            args[i].threads_count = threads_amount;
            pthread_create(&threads[i], NULL, FindTriangle, &args[i]);
        }
        // Собираем итоговые результаты
        for (int i = 0; i < threads_amount; ++i) {
            Result* res;
            pthread_join(threads[i], (void**)&res);
            if (res->area > max_area) {
                max_area = res->area;
                max_i = res->i;
                max_j = res->j;
                max_k = res->k;
            }
            free(res);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_ms = (end.tv_sec - start.tv_sec) * 1000LL + (end.tv_nsec - start.tv_nsec) / 1000000LL;
        printf("Parallel time with %d threads: %lld ms\n", threads_amount, time_ms);
        free(threads);
        free(args);
    }

    printf("Max area: %.2f at points %d, %d, %d\n", max_area, max_i, max_j, max_k);
    return 0;
}
