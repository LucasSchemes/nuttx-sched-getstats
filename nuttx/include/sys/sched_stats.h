#ifndef __INCLUDE_SYS_SCHED_STATS_H
#define __INCLUDE_SYS_SCHED_STATS_H

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

#define SCHED_STATS_ALL        0x00u
#define SCHED_STATS_NO_IDLE    0x01u
#define SCHED_STATS_VALID_MASK (SCHED_STATS_NO_IDLE)

struct sched_stats_s
{
  uint32_t total_tasks;
  uint32_t running_tasks;
  uint32_t ready_tasks;
  uint32_t blocked_sem;
  uint32_t blocked_sig;
  uint32_t blocked_other;
  uint32_t kernel_threads;
  uint32_t user_tasks;
  uint32_t pthreads;
  uint32_t pidhash_size;
  uint32_t sched_ticks;
  uint8_t  min_priority;
  uint8_t  max_priority;
};

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

int sched_getstats(FAR struct sched_stats_s *stats, unsigned int flags);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif
