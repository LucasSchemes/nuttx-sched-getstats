#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/sched_stats.h>

#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/sched.h>

#include "sched/sched.h"

int nxsched_get_stats(FAR struct sched_stats_s *stats, unsigned int flags)
{
  struct sched_stats_s local;
  irqstate_t irqflags;
  uint8_t min_prio = UINT8_MAX;
  uint8_t max_prio = 0;
  int i;

  if (stats == NULL)
    {
      return -EINVAL;
    }

  if ((flags & ~SCHED_STATS_VALID_MASK) != 0u)
    {
      return -EINVAL;
    }

  memset(&local, 0, sizeof(local));

  irqflags = enter_critical_section();

  local.sched_ticks  = (uint32_t)clock_systime_ticks();
  local.pidhash_size = (uint32_t)g_npidhash;

  if (g_pidhash != NULL)
    {
      for (i = 0; i < g_npidhash; i++)
        {
          FAR struct tcb_s *tcb = g_pidhash[i];

          if (tcb == NULL || tcb->task_state == TSTATE_TASK_INVALID)
            {
              continue;
            }

          if ((flags & SCHED_STATS_NO_IDLE) != 0u &&
              tcb->sched_priority == 0)
            {
              continue;
            }

          local.total_tasks++;

          switch (tcb->task_state)
            {
              case TSTATE_TASK_RUNNING:
                local.running_tasks++;
                break;

              case TSTATE_TASK_READYTORUN:
                local.ready_tasks++;
                break;

              case TSTATE_TASK_PENDING:
                local.ready_tasks++;
                break;

              case TSTATE_WAIT_SEM:
                local.blocked_sem++;
                break;

              case TSTATE_WAIT_SIG:
                local.blocked_sig++;
                break;

              default:
                if (tcb->task_state >= FIRST_BLOCKED_STATE)
                  {
                    local.blocked_other++;
                  }
                break;
            }

          switch (tcb->flags & TCB_FLAG_TTYPE_MASK)
            {
              case TCB_FLAG_TTYPE_KERNEL:
                local.kernel_threads++;
                break;

              case TCB_FLAG_TTYPE_PTHREAD:
                local.pthreads++;
                break;

              case TCB_FLAG_TTYPE_TASK:
              default:
                local.user_tasks++;
                break;
            }

          if (tcb->sched_priority < min_prio)
            {
              min_prio = tcb->sched_priority;
            }

          if (tcb->sched_priority > max_prio)
            {
              max_prio = tcb->sched_priority;
            }
        }
    }

  leave_critical_section(irqflags);

  if (local.total_tasks > 0u)
    {
      local.min_priority = min_prio;
      local.max_priority = max_prio;
    }

  *stats = local;
  return OK;
}

int sched_getstats(FAR struct sched_stats_s *stats, unsigned int flags)
{
  int ret;

  ret = nxsched_get_stats(stats, flags);
  if (ret < 0)
    {
      set_errno(-ret);
      return ERROR;
    }

  return OK;
}
