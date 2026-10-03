/****************************************************************************
 * canspeed_main.c - Week8 NuttX CAN TX demo (WarShip)
 *
 * Copy to: ~/automotive-nuttx/apps/examples/canspeed/
 *
 * Default: TX on LPWORK, main returns → NSH stays free.
 *
 * IMPORTANT: open()/close() of /dev/can0 must run on the LPWORK thread.
 * If main opens the fd and then returns, NuttX closes that task's fds and
 * the worker sees errno=9 (EBADF).
 *
 *   nsh> canspeed           # start background TX
 *   nsh> ledblink
 *   nsh> canspeed stop
 *   nsh> canspeed fg        # foreground loop (blocks NSH)
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#include <nuttx/can/can.h>
#include <nuttx/clock.h>
#include <nuttx/wqueue.h>

#ifndef CONFIG_EXAMPLES_CANSPEED_DEVPATH
#  define CONFIG_EXAMPLES_CANSPEED_DEVPATH "/dev/can0"
#endif

#ifndef CONFIG_EXAMPLES_CANSPEED_ID
#  define CONFIG_EXAMPLES_CANSPEED_ID 0x201
#endif

#ifndef CONFIG_EXAMPLES_CANSPEED_INTERVAL_MS
#  define CONFIG_EXAMPLES_CANSPEED_INTERVAL_MS 100
#endif

#ifndef CONFIG_EXAMPLES_CANSPEED_COUNT
#  define CONFIG_EXAMPLES_CANSPEED_COUNT 50
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct work_s g_work;
static volatile bool g_running;
static int g_fd = -1;          /* valid only in LPWORK thread context */
static bool g_printed_bt;
static uint16_t g_speed;
static int g_sent;
static int g_limit;
static size_t g_msgsize;
static struct can_msg_s g_msg;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void canspeed_init_msg(void)
{
  memset(&g_msg, 0, sizeof(g_msg));
  g_msg.cm_hdr.ch_id  = CONFIG_EXAMPLES_CANSPEED_ID;
  g_msg.cm_hdr.ch_rtr = 0;
  g_msg.cm_hdr.ch_dlc = 2;
#ifdef CONFIG_CAN_EXTID
  g_msg.cm_hdr.ch_extid = 0;
#endif
  g_msgsize = CAN_MSGLEN(2);
}

/* Must be called from the task that will write (FG main or LPWORK). */

static int canspeed_open_dev(void)
{
  struct canioc_bittiming_s bt;
  int ret;

  if (g_fd >= 0)
    {
      return 0;
    }

  g_fd = open(CONFIG_EXAMPLES_CANSPEED_DEVPATH, O_RDWR);
  if (g_fd < 0)
    {
      printf("canspeed: open %s failed: %d (%s)\n",
             CONFIG_EXAMPLES_CANSPEED_DEVPATH, errno, strerror(errno));
      return -1;
    }

  if (!g_printed_bt)
    {
      ret = ioctl(g_fd, CANIOC_GET_BITTIMING,
                  (unsigned long)((uintptr_t)&bt));
      if (ret >= 0)
        {
          printf("canspeed: baud=%lu tseg1=%u tseg2=%u sjw=%u\n",
                 (unsigned long)bt.bt_baud, bt.bt_tseg1, bt.bt_tseg2,
                 bt.bt_sjw);
        }

      g_printed_bt = true;
    }

  return 0;
}

static void canspeed_close_dev(void)
{
  if (g_fd >= 0)
    {
      close(g_fd);
      g_fd = -1;
    }
}

static int canspeed_send_one(void)
{
  ssize_t nbytes;

  g_msg.cm_data[0] = (uint8_t)(g_speed >> 8);
  g_msg.cm_data[1] = (uint8_t)(g_speed & 0xff);

  nbytes = write(g_fd, &g_msg, g_msgsize);
  if (nbytes != (ssize_t)g_msgsize)
    {
      printf("canspeed: write failed #%d nbytes=%ld errno=%d\n",
             g_sent + 1, (long)nbytes, errno);
      return -1;
    }

  g_sent++;
  printf("canspeed: #%d id=0x%x speed=%u\n",
         g_sent, (unsigned)g_msg.cm_hdr.ch_id, (unsigned)g_speed);

  g_speed++;
  if (g_speed > 200)
    {
      g_speed = 0;
    }

  return 0;
}

#if defined(CONFIG_SCHED_LPWORK) || defined(CONFIG_SCHED_HPWORK)

static void canspeed_worker(FAR void *arg)
{
  clock_t delay;

  (void)arg;

  if (!g_running)
    {
      canspeed_close_dev();
      return;
    }

  /* Open on worker thread so fd stays alive after NSH task exits. */

  if (canspeed_open_dev() < 0)
    {
      g_running = false;
      return;
    }

  if (canspeed_send_one() < 0)
    {
      g_running = false;
      canspeed_close_dev();
      return;
    }

  if (g_limit != 0 && g_sent >= g_limit)
    {
      g_running = false;
      canspeed_close_dev();
      printf("canspeed: done (background)\n");
      return;
    }

  delay = MSEC2TICK(CONFIG_EXAMPLES_CANSPEED_INTERVAL_MS);
  work_queue(LPWORK, &g_work, canspeed_worker, NULL, delay);
}

static void canspeed_stop_worker(FAR void *arg)
{
  (void)arg;
  canspeed_close_dev();
  printf("canspeed: stopped\n");
}

static void canspeed_stop(void)
{
  g_running = false;
  work_cancel(LPWORK, &g_work);

  /* close() must run on LPWORK (same task that opened the fd). */

  work_queue(LPWORK, &g_work, canspeed_stop_worker, NULL, 0);
}

static int canspeed_start_bg(void)
{
  clock_t delay;
  int ret;

  if (g_running)
    {
      printf("canspeed: already running (use: canspeed stop)\n");
      return 0;
    }

  canspeed_init_msg();
  g_speed = 0;
  g_sent = 0;
  g_limit = CONFIG_EXAMPLES_CANSPEED_COUNT;
  g_printed_bt = false;
  g_running = true;

  printf("canspeed: BG TX id=0x%x period=%d ms count=%d on LPWORK\n",
         (unsigned)CONFIG_EXAMPLES_CANSPEED_ID,
         CONFIG_EXAMPLES_CANSPEED_INTERVAL_MS,
         g_limit);
  printf("canspeed: main returns; open/write run on worker (NSH free)\n");
  printf("canspeed: stop with: canspeed stop\n");

  /* First open happens inside worker, not here. */

  delay = 0;
  ret = work_queue(LPWORK, &g_work, canspeed_worker, NULL, delay);
  if (ret < 0)
    {
      g_running = false;
      printf("canspeed: work_queue failed: %d\n", ret);
      return ret;
    }

  return 0;
}

#endif /* LPWORK || HPWORK */

static int canspeed_run_fg(void)
{
  int n;
  int limit = CONFIG_EXAMPLES_CANSPEED_COUNT;

  canspeed_init_msg();
  g_speed = 0;
  g_sent = 0;
  g_printed_bt = false;

  if (canspeed_open_dev() < 0)
    {
      return 1;
    }

  printf("canspeed: FG mode blocks NSH until done (~%d ms)\n",
         limit * CONFIG_EXAMPLES_CANSPEED_INTERVAL_MS);

  for (n = 0; limit == 0 || n < limit; n++)
    {
      if (canspeed_send_one() < 0)
        {
          canspeed_close_dev();
          return 2;
        }

      usleep(CONFIG_EXAMPLES_CANSPEED_INTERVAL_MS * 1000);
    }

  canspeed_close_dev();
  printf("canspeed: done\n");
  return 0;
}

/****************************************************************************
 * main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc > 1 && argv[1] != NULL)
    {
      if (strcmp(argv[1], "stop") == 0 ||
          (argv[1][0] == 's' && argv[1][1] == 't'))
        {
#if defined(CONFIG_SCHED_LPWORK) || defined(CONFIG_SCHED_HPWORK)
          canspeed_stop();
          return 0;
#else
          printf("canspeed: stop needs work queue\n");
          return 1;
#endif
        }

      if (strcmp(argv[1], "fg") == 0)
        {
          return canspeed_run_fg();
        }
    }

#if defined(CONFIG_SCHED_LPWORK) || defined(CONFIG_SCHED_HPWORK)
  return canspeed_start_bg();
#else
  printf("canspeed: no LPWORK — falling back to foreground\n");
  return canspeed_run_fg();
#endif
}
