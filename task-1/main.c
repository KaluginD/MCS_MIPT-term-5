#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

#include "runner.h"

typedef struct ctx_t {
  int a;
  int b;
  int x;
  double p;
  size_t N;
  int P;
  double bounds;
  int reached_b;
  int steps;
  double time;
} ctx_t;

void get_results(void *context) {
  ctx_t *ctx = context;
  FILE *file = fopen("results.txt", "w");
  fprintf(file, "%.2f %.1f %.4fs %d %d %d %ld %.2f %d\n",
          (double) ctx->reached_b / ctx->N,
          (double) ctx->steps / ctx->N,
          ctx->time, ctx->a, ctx->b, ctx->x,
          ctx->N, ctx->p, ctx->P
  );
}

void ctor(void *context) {
  ctx_t *ctx = context;
  ctx->bounds = (double) RAND_MAX * (1 - ctx->p);
}

void dtor(void *context) {
  ctx_t *ctx = context;
}

int wanderings(int i, void *context, int seed, int *steps) {
  ctx_t *ctx = context;
  int rand_seed = (i + seed) ^omp_get_thread_num();

  int coord = ctx->x;
  while ((coord != ctx->a) && (coord != ctx->b)) {
    *steps += 1;
    double curr_seed = (double) rand_r(&rand_seed);
    coord += 2 * (curr_seed > ctx->bounds) - 1;
  }

  return (coord == ctx->b);

}

void run(void *context) {
  ctx_t *ctx = context;

  int reached_b = 0;
  int steps = 0;

  struct timeval begin, end;

  srand(time(NULL));
  int rand_seed = rand() % 10000;

  gettimeofday(&begin, NULL);

#pragma omp parallel for reduction(+ : steps, reached_b)
  for (uint i = 0; i < ctx->N; ++i) {
    int curr_steps = 0;
    reached_b += wanderings(i, ctx, rand_seed, &curr_steps);
    steps += curr_steps;
  }

  gettimeofday(&end, NULL);

  double worktime = (end.tv_sec - begin.tv_sec) * 1000000u + (end.tv_usec - begin.tv_usec) / 1.e6;

  ctx->reached_b = reached_b;
  ctx->steps = steps;
  ctx->time = worktime;

  get_results(ctx);
}

int main(int argc, char **argv) {
  int a = atoi(argv[1]);
  int b = atoi(argv[2]);
  int x = atoi(argv[3]);
  size_t N = atoi(argv[4]);
  double p = atof(argv[5]);
  int P = atoi(argv[6]);

  omp_set_num_threads(P);

  struct ctx_t ctx = {
      .a = a,
      .b = b,
      .x = x,
      .N = N,
      .p = p,
      .P = P
  };

  runner_pre(ctor, &ctx);

  runner_run(run, &ctx, "wanderings");

  runner_post(dtor, &ctx);

  return 0;
}