#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>
#include <pthread.h>
#include <sys/param.h>
#include <memory.h>

#define MAX_NUM 10000000

typedef struct ThreadInfo {
  int n, m;
  int begin, chunks_to_sort;
  int *array;
} ThreadInfo;

typedef struct MergeInfo {
  int first_left, first_right;
  int second_left, second_right;
  int *array;
} MergeInfo;

int Comparator(const void *a, const void *b) {
  const int *inta = (const int *) a;
  const int *intb = (const int *) b;
  return (*inta - *intb);
}

void *SortSomeChunks(void *income_info) {
  ThreadInfo *info = (ThreadInfo *) income_info;
  for (int i = 0, array_index = info->begin;
       i < info->chunks_to_sort;
       ++i, array_index += info->m) {
    qsort(&(info->array[array_index]), info->m, sizeof(int), Comparator);
  }
  return NULL;
}

void ChunkSort(const int n, const int m, const int P, int *array) {
  int chunks_per_array = n / m;
  int chunks_per_thread = chunks_per_array / P;
  int elements_per_thread = m * chunks_per_thread;

  ThreadInfo *info = (ThreadInfo *) malloc(sizeof(ThreadInfo) * P);
  for (int i = 0; i < P; ++i) {
    info[i].n = n;
    info[i].m = m;
    info[i].array = array;
    info[i].chunks_to_sort = chunks_per_thread;
  }

  pthread_t *threads = (pthread_t *) malloc(sizeof(pthread_t) * P);
  int array_index = 0;
  for (size_t thread_number = 0; thread_number < P;
       ++thread_number, array_index += elements_per_thread) {
    info[thread_number].begin = array_index;
    pthread_create(&(threads[thread_number]), NULL, SortSomeChunks, &info[thread_number]);
  }

  for (int i = 0; i < P; ++i) {
    pthread_join(threads[i], NULL);
  }
  free(threads);
  free(info);

  qsort(&array[array_index], n - array_index, sizeof(int), Comparator);
}

void *MergeChunks(void *income_info) {
  MergeInfo *info = (MergeInfo *) income_info;

  int first_iter = 0, second_iter = 0;
  int first_size = info->first_right - info->first_left + 1;
  int second_size = info->second_right - info->second_left + 1;

  int *first_array = malloc(sizeof(int) * first_size);
  memcpy(first_array, &(info->array[info->first_left]), sizeof(int) * first_size);

  int *second_array = malloc(sizeof(int) * second_size);
  memcpy(second_array, &(info->array[info->second_left]), sizeof(int) * second_size);

  while (first_iter < first_size && second_iter < second_size) {
    if (first_array[first_iter] < second_array[second_iter]) {
      info->array[info->first_left + first_iter + second_iter] = first_array[first_iter++];
    } else {
      info->array[info->first_left + first_iter + second_iter] = second_array[second_iter++];
    }
  }
  while (first_iter < first_size) {
    info->array[info->first_left + first_iter + second_iter] = first_array[first_iter++];
  }
  while (second_iter < second_size) {
    info->array[info->first_left + first_iter + second_iter] = second_array[second_iter++];
  }
  free(first_array);
  free(second_array);
  return NULL;
}

void ParallelSort(const int n, const int m, const int P, int *array) {
  ChunkSort(n, m, P, array);

  pthread_t *threads = (pthread_t *) malloc(sizeof(pthread_t) * P);
  MergeInfo *info = (MergeInfo *) malloc(sizeof(MergeInfo) * P);

  int size = m;
  while (size < n) {
    int array_index = 0;
    while (array_index + size < n - 1) {
      int working_threads = P;
      for (int i = 0; i < P; ++i) {
        if (n < array_index + size) {
          working_threads = i;
          break;
        }
        info[i].first_left = array_index;
        info[i].first_right = array_index + size - 1;
        info[i].second_left = array_index + size;
        info[i].second_right = MIN(array_index + 2 * size, n) - 1;
        info[i].array = array;
        array_index = info[i].second_right + 1;
        pthread_create(&(threads[i]), NULL, MergeChunks, &info[i]);
      }
      for (int i = 0; i < working_threads; ++i) {
        pthread_join(threads[i], NULL);
      }
    }
    size *= 2;
  }

  free(threads);
  free(info);
}

int main(int argc, char **argv) {
  if (argc != 4) {
    return 1;
  }
  int n = atoi(argv[1]);
  int m = atoi(argv[2]);
  int P = atoi(argv[3]);

  FILE *results = fopen("data.txt", "w");
  int *array = (int *) malloc(n * sizeof(int));
  srand(time(NULL));
  for (size_t i = 0; i < n; ++i) {
    array[i] = rand() % MAX_NUM;
    fprintf(results, "%d ", array[i]);
  }
  fprintf(results, "\n");

  int *array_for_parallel_sort = (int *) malloc(n * sizeof(int));
  int *array_for_builtin_sort = (int *) malloc(n * sizeof(int));
  memcpy(array_for_parallel_sort, array, n * sizeof(int));
  memcpy(array_for_builtin_sort, array, n * sizeof(int));

  double parallel_begin_time = omp_get_wtime();
  ParallelSort(n, m, P, array_for_parallel_sort);
  double parallel_sort_time = omp_get_wtime() - parallel_begin_time;

  double builtin_begin_time = omp_get_wtime();
  qsort(array_for_builtin_sort, n, sizeof(int), Comparator);
  double builtin_sort_time = omp_get_wtime() - builtin_begin_time;

  size_t flag = 1;
  for (size_t i = 0; i < n; ++i) {
    fprintf(results, "%d ", array_for_parallel_sort[i]);
    if (array_for_builtin_sort[i] != array_for_parallel_sort[i]) {
      flag = 0;
    }
  }
  fprintf(results, "\n");
  fclose(results);
  free(array);
  free(array_for_builtin_sort);
  free(array_for_parallel_sort);

  if (flag == 1) {
    printf("sorting algorithm works correctly\n");
  } else {
    printf("sorting algorithm failed :(\n");
    return 1;
  };

  FILE *stats = fopen("stats.txt", "w");
  fprintf(stats, "%.5fs %d %d %d\n", parallel_sort_time, n, m, P);
  fclose(stats);

  printf("parallel sort time - %.5fs\nbuiltin sort time  - %.5fs\n",
         parallel_sort_time, builtin_sort_time);

  return 0;
}