#include <nuttx/config.h>

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sched_stats.h>
#include <unistd.h>

static sem_t g_worker_sem;

static void print_stats(const char *label, const struct sched_stats_s *s)
{
  printf("\n=== %s ===\n", label);
  printf("  total_tasks    : %lu\n", (unsigned long)s->total_tasks);
  printf("  running_tasks  : %lu\n", (unsigned long)s->running_tasks);
  printf("  ready_tasks    : %lu\n", (unsigned long)s->ready_tasks);
  printf("  blocked_sem    : %lu\n", (unsigned long)s->blocked_sem);
  printf("  blocked_sig    : %lu\n", (unsigned long)s->blocked_sig);
  printf("  blocked_other  : %lu\n", (unsigned long)s->blocked_other);
  printf("  kernel_threads : %lu\n", (unsigned long)s->kernel_threads);
  printf("  user_tasks     : %lu\n", (unsigned long)s->user_tasks);
  printf("  pthreads       : %lu\n", (unsigned long)s->pthreads);
  printf("  pidhash_size   : %lu\n", (unsigned long)s->pidhash_size);
  printf("  sched_ticks    : %lu\n", (unsigned long)s->sched_ticks);
  printf("  priority range : [%u .. %u]\n",
         (unsigned int)s->min_priority,
         (unsigned int)s->max_priority);
}

static void *sem_worker(void *arg)
{
  (void)arg;
  sem_wait(&g_worker_sem);
  return NULL;
}

static void *sleep_worker(void *arg)
{
  (void)arg;
  usleep(1500000);
  return NULL;
}

int main(int argc, char *argv[])
{
  struct sched_stats_s st_all;
  struct sched_stats_s st_noidle;
  struct sched_stats_s st_workers;
  struct sched_stats_s st_after;
  pthread_t t_sem;
  pthread_t t_sleep;
  int ret;
  int failures = 0;

  (void)argc;
  (void)argv;

  printf("Iniciando testes da syscall sched_getstats()...\n");

  ret = sched_getstats(&st_all, SCHED_STATS_ALL);
  if (ret != 0)
    {
      printf("[FALHA] Teste 1 (ALL): ret=%d errno=%d\n", ret, errno);
      failures++;
    }
  else
    {
      print_stats("Teste 1: SCHED_STATS_ALL (baseline)", &st_all);
      if (st_all.total_tasks > 0 && st_all.running_tasks >= 1 &&
          st_all.min_priority == 0)
        {
          printf("[OK] Teste 1 passou.\n");
        }
      else
        {
          printf("[FALHA] Teste 1: valores inesperados.\n");
          failures++;
        }
    }

  ret = sched_getstats(&st_noidle, SCHED_STATS_NO_IDLE);
  if (ret != 0)
    {
      printf("[FALHA] Teste 2 (NO_IDLE): ret=%d errno=%d\n", ret, errno);
      failures++;
    }
  else
    {
      print_stats("Teste 2: SCHED_STATS_NO_IDLE", &st_noidle);
      if (st_noidle.total_tasks < st_all.total_tasks &&
          st_noidle.min_priority > 0)
        {
          printf("[OK] Teste 2 passou (IDLE filtrada).\n");
        }
      else
        {
          printf("[FALHA] Teste 2: IDLE nao foi filtrada.\n");
          failures++;
        }
    }

  sem_init(&g_worker_sem, 0, 0);

  ret = pthread_create(&t_sem, NULL, sem_worker, NULL);
  if (ret != 0)
    {
      printf("[FALHA] Erro ao criar sem_worker: %d\n", ret);
      failures++;
    }

  ret = pthread_create(&t_sleep, NULL, sleep_worker, NULL);
  if (ret != 0)
    {
      printf("[FALHA] Erro ao criar sleep_worker: %d\n", ret);
      failures++;
    }

  usleep(200000);

  ret = sched_getstats(&st_workers, SCHED_STATS_ALL);
  if (ret != 0)
    {
      printf("[FALHA] Teste 3 (com workers): ret=%d errno=%d\n", ret, errno);
      failures++;
    }
  else
    {
      print_stats("Teste 3: Com 2 pthreads bloqueadas", &st_workers);
      if (st_workers.total_tasks >= st_all.total_tasks + 2 &&
          st_workers.pthreads >= st_all.pthreads + 2 &&
          st_workers.blocked_sem >= st_all.blocked_sem + 1)
        {
          printf("[OK] Teste 3 passou (incremento de pthreads e bloqueios confirmado).\n");
        }
      else
        {
          printf("[FALHA] Teste 3: contadores nao refletiram as threads criadas.\n");
          failures++;
        }
    }

  sem_post(&g_worker_sem);
  pthread_join(t_sem, NULL);
  pthread_join(t_sleep, NULL);
  sem_destroy(&g_worker_sem);

  sched_getstats(&st_after, SCHED_STATS_ALL);
  print_stats("Pos-Teste 3: Apos encerrar pthreads", &st_after);

  printf("\n=== Teste 4: Validacao de erros ===\n");

  errno = 0;
  ret = sched_getstats(NULL, SCHED_STATS_ALL);
  if (ret == -1 && errno == EINVAL)
    {
      printf("[OK] Teste 4a (stats == NULL): ret=-1, errno=EINVAL (%d)\n",
             errno);
    }
  else
    {
      printf("[FALHA] Teste 4a (stats == NULL): ret=%d, errno=%d\n",
             ret, errno);
      failures++;
    }

  errno = 0;
  ret = sched_getstats(&st_all, 0x02u);
  if (ret == -1 && errno == EINVAL)
    {
      printf("[OK] Teste 4b (flag invalida 0x02): ret=-1, errno=EINVAL (%d)\n",
             errno);
    }
  else
    {
      printf("[FALHA] Teste 4b (flag invalida): ret=%d, errno=%d\n",
             ret, errno);
      failures++;
    }

  errno = 0;
  ret = sched_getstats(NULL, 0xFFu);
  if (ret == -1 && errno == EINVAL)
    {
      printf("[OK] Teste 4c (NULL + 0xFF): ret=-1, errno=EINVAL (%d)\n",
             errno);
    }
  else
    {
      printf("[FALHA] Teste 4c (NULL + 0xFF): ret=%d, errno=%d\n",
             ret, errno);
      failures++;
    }

  printf("\n========================================\n");
  printf("Resumo: %s (%d falha(s))\n",
         failures == 0 ? "TODOS OS TESTES PASSARAM" : "HOUVE FALHAS",
         failures);
  printf("========================================\n");

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
