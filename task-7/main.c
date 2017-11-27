#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <mpi.h>

int main(int argc, char **argv) {
  if (argc != 5) {
    printf("wrong input!\n");
    return EXIT_FAILURE;
  }

  int l, a, b, N;
  l = atoi(argv[1]);
  a = atoi(argv[2]);
  b = atoi(argv[3]);
  N = atoi(argv[4]);
  int len = a * b * l;
  int node, seed;
  int* field;
  struct timespec start, end;
  MPI_File results;
  MPI_Datatype form, neighbour;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &node);
  int field_size = a * b * l * l;
  field = calloc(field_size, sizeof(int));

  if (node != 0) {
    MPI_Scatter(NULL, 1, MPI_INT, &seed, 1, MPI_INT, 0, MPI_COMM_WORLD);
  } else {
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (size != a * b)  {
      printf("Field has wrong size %d.\n", size);
      return EXIT_FAILURE;
    }
    int *seeds = malloc(size * sizeof(int));
    seed = time(NULL);
    for(size_t i = 0; i < size; ++i) {
      seeds[i] = seed + i;
    }
    MPI_Scatter(seeds, 1, MPI_INT, &seed, 1, MPI_INT, 0, MPI_COMM_WORLD);
    clock_gettime(CLOCK_MONOTONIC, &start);
    free(seeds);
  }

  for(size_t i = 0; i < N; ++i) {
    int x = rand() % l;
    int y = rand() % l;
    int r = rand() % (a * b);
    ++field[(a * b) * (y * l + x) + r];
  }

  MPI_File_open(MPI_COMM_WORLD, "data.bin", MPI_MODE_WRONLY | MPI_MODE_CREATE,
                MPI_INFO_NULL, &results);
  MPI_File_set_size(results, 0);
  MPI_Type_contiguous(len, MPI_INT, &neighbour);
  MPI_Type_create_resized(neighbour, 0, a * len * sizeof(int), &form);
  MPI_Type_commit(&form);
  int position = len * (a * l * (node / a) + node % a);
  MPI_File_set_view(results, position * sizeof(int), MPI_INT, form, "naive", MPI_INFO_NULL);
  MPI_File_write_all(results, field, field_size, MPI_INT, MPI_STATUS_IGNORE);
  MPI_File_close(&results);
  free(field);

  if (!node) {
    clock_gettime(CLOCK_MONOTONIC, &end);
    double working_time = (end.tv_nsec - start.tv_nsec) * 0.000000001 + (end.tv_sec - start.tv_sec);

    FILE *stats = fopen("stats.txt", "w");
    fprintf(stats, "%d %d %d %d %.4fs\n", l, a, b, N, working_time);
    fclose(stats);
  }
  MPI_Finalize();
  return EXIT_SUCCESS;
}
