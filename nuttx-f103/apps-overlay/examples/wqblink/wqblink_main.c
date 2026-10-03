/****************************************************************************
 * wqblink_main.c - Week7 work-queue demo (WarShip / stm32f103)
 *
 * Copy to: ~/automotive-nuttx/apps/examples/wqblink/
 *
 * Contrast with ledblink:
 *   ledblink  = dedicated task + usleep() loop (task stays alive)
 *   wqblink   = queue delayed work on LPWORK; main can exit, blink continues
 *
 * Usage:
 *   nsh> wqblink          # start periodic work (same-phase dual LED if USERLED)
 *   nsh> wqblink stop     # cancel pending work
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include <nuttx/clock.h>
#include <nuttx/wqueue.h>

#ifdef CONFIG_USERLED
#  include <nuttx/leds/userled.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#endif

#ifndef CONFIG_EXAMPLES_WQBLINK_INTERVAL_MS
#  define CONFIG_EXAMPLES_WQBLINK_INTERVAL_MS 500
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct work_s g_work;
static volatile bool g_running;
static bool g_on;
static unsigned long g_tick;
#ifdef CONFIG_USERLED
static int g_ledfd = -1;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void wqblink_worker(FAR void *arg)
{
  clock_t delay;

  (void)arg;

  if (!g_running)
    {
      return;
    }

  g_tick++;
  g_on = !g_on;

#ifdef CONFIG_USERLED
  if (g_ledfd >= 0)
    {
      userled_set_t ledset = g_on ? (userled_set_t)0x3 : (userled_set_t)0x0;
      ioctl(g_ledfd, ULEDIOC_SETALL, ledset);
    }
#endif

  printf("wqblink: worker tick=%lu both=%d (on LPWORK)\n",
         g_tick, g_on ? 1 : 0);

  /* Re-arm: delayed work uses the wqueue timer internally (wd_*) */
  delay = MSEC2TICK(CONFIG_EXAMPLES_WQBLINK_INTERVAL_MS);
  work_queue(LPWORK, &g_work, wqblink_worker, NULL, delay);
}

static void wqblink_stop(void)
{
  int ret;

  g_running = false;
  ret = work_cancel(LPWORK, &g_work);
  printf("wqblink: stop (work_cancel=%d)\n", ret);

#ifdef CONFIG_USERLED
  if (g_ledfd >= 0)
    {
      ioctl(g_ledfd, ULEDIOC_SETALL, (userled_set_t)0x0);
      close(g_ledfd);
      g_ledfd = -1;
    }
#endif
}

static int wqblink_start(void)
{
  clock_t delay;
  int ret;

  if (g_running)
    {
      printf("wqblink: already running (use: wqblink stop)\n");
      return 0;
    }

#ifdef CONFIG_USERLED
  if (g_ledfd < 0)
    {
      g_ledfd = open("/dev/userleds", O_WRONLY);
      if (g_ledfd < 0)
        {
          printf("wqblink: open /dev/userleds failed (%d), printf-only\n",
                 errno);
        }
    }
#endif

  g_tick = 0;
  g_on = false;
  g_running = true;

  delay = MSEC2TICK(CONFIG_EXAMPLES_WQBLINK_INTERVAL_MS);
  ret = work_queue(LPWORK, &g_work, wqblink_worker, NULL, delay);
  if (ret < 0)
    {
      g_running = false;
      printf("wqblink: work_queue failed: %d\n", ret);
      return ret;
    }

  printf("wqblink: started period=%d ms on LPWORK\n",
         CONFIG_EXAMPLES_WQBLINK_INTERVAL_MS);
  printf("wqblink: main returns; blink runs on worker thread\n");
  printf("wqblink: stop with: wqblink stop\n");
  return 0;
}

/****************************************************************************
 * main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc > 1 && argv[1] != NULL &&
      (strcmp(argv[1], "stop") == 0 || argv[1][0] == 's'))
    {
      wqblink_stop();
      return 0;
    }

  return wqblink_start();
}
