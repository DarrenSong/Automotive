/****************************************************************************
 * canlight_main.c - Week8 CAN RX → USERLED (WarShip)
 *
 * Copy to: ~/automotive-nuttx/apps/examples/canlight/
 *
 * Protocol:
 *   ID  = CONFIG_EXAMPLES_CANLIGHT_ID (default 0x202)
 *   DLC >= 1
 *   data[0] bit0 → LED0 (PB5), bit1 → LED1 (PE5)  (1 = logical ON)
 *
 * Usage:
 *   nsh> canlight           # MUST first: start RX daemon
 *   nsh> canlight 3         # local LEDs + TX 0x202 (daemon prints RX if loopback)
 *   nsh> canlight 1 / 0
 *   nsh> canlight stop
 *
 * "canlight <mask>" always drives USERLED locally first, then CAN TX.
 * Remote/loopback RX still needs the daemon + CONFIG_CAN_LOOPBACK for
 * single-board RX path. Without loopback, write() may block waiting ACK.
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sched.h>
#include <sys/ioctl.h>

#include <nuttx/sched.h>
#include <nuttx/can/can.h>
#include <nuttx/leds/userled.h>

#ifndef CONFIG_EXAMPLES_CANLIGHT_DEVPATH
#  define CONFIG_EXAMPLES_CANLIGHT_DEVPATH "/dev/can0"
#endif

#ifndef CONFIG_EXAMPLES_CANLIGHT_ID
#  define CONFIG_EXAMPLES_CANLIGHT_ID 0x202
#endif

#ifndef CONFIG_EXAMPLES_CANLIGHT_DAEMON_PRIORITY
#  define CONFIG_EXAMPLES_CANLIGHT_DAEMON_PRIORITY 100
#endif

#ifndef CONFIG_EXAMPLES_CANLIGHT_DAEMON_STACKSIZE
#  define CONFIG_EXAMPLES_CANLIGHT_DAEMON_STACKSIZE 2048
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool g_running;
static volatile pid_t g_daemon_pid;
static int g_ledfd = -1;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int canlight_open_leds(void)
{
  if (g_ledfd >= 0)
    {
      return 0;
    }

  g_ledfd = open("/dev/userleds", O_WRONLY);
  if (g_ledfd < 0)
    {
      printf("canlight: open /dev/userleds failed: %d\n", errno);
      return -1;
    }

  return 0;
}

static void canlight_apply_mask(userled_set_t mask)
{
  mask &= 0x3;
  if (g_ledfd >= 0)
    {
      ioctl(g_ledfd, ULEDIOC_SETALL, mask);
    }

  printf("canlight: LEDs mask=0x%x (LED0=%d LED1=%d)\n",
         (unsigned)mask, (mask & 0x1) ? 1 : 0, (mask & 0x2) ? 1 : 0);
}

static int canlight_send_mask(unsigned mask)
{
  int fd;
  int ledfd;
  struct can_msg_s msg;
  size_t msgsize;
  ssize_t nbytes;

  mask &= 0x3;

  /* 1) Always set LEDs locally — do not depend on RX daemon / loopback. */

  ledfd = open("/dev/userleds", O_WRONLY);
  if (ledfd < 0)
    {
      printf("canlight: open /dev/userleds failed: %d\n", errno);
    }
  else
    {
      ioctl(ledfd, ULEDIOC_SETALL, (userled_set_t)mask);
      close(ledfd);
      printf("canlight: local LEDs mask=0x%x (LED0=%d LED1=%d)\n",
             (unsigned)mask, (mask & 0x1) ? 1 : 0, (mask & 0x2) ? 1 : 0);
    }

  if (!g_running || g_daemon_pid <= 0)
    {
      printf("canlight: note: RX daemon not running (optional for local LED)\n");
      printf("canlight:       start with: canlight\n");
    }

  /* 2) TX on CAN — may block a long time if no ACK and no loopback. */

  printf("canlight: TX id=0x%x data=0x%02x ...\n",
         (unsigned)CONFIG_EXAMPLES_CANLIGHT_ID, (unsigned)mask);

  fd = open(CONFIG_EXAMPLES_CANLIGHT_DEVPATH, O_WRONLY);
  if (fd < 0)
    {
      printf("canlight: open %s for TX failed: %d\n",
             CONFIG_EXAMPLES_CANLIGHT_DEVPATH, errno);
      return 1;
    }

  memset(&msg, 0, sizeof(msg));
  msg.cm_hdr.ch_id  = CONFIG_EXAMPLES_CANLIGHT_ID;
  msg.cm_hdr.ch_rtr = 0;
  msg.cm_hdr.ch_dlc = 1;
#ifdef CONFIG_CAN_EXTID
  msg.cm_hdr.ch_extid = 0;
#endif
  msg.cm_data[0] = (uint8_t)mask;
  msgsize = CAN_MSGLEN(1);

  nbytes = write(fd, &msg, msgsize);
  close(fd);

  if (nbytes != (ssize_t)msgsize)
    {
      printf("canlight: TX failed nbytes=%ld errno=%d\n",
             (long)nbytes, errno);
      printf("canlight: if TX hangs/slow: enable CONFIG_CAN_LOOPBACK=y\n");
      return 2;
    }

  printf("canlight: TX ok\n");
  return 0;
}

/* Runs as its own task — owns CAN fd and LED fd. */

static int canlight_daemon(int argc, FAR char *argv[])
{
  int canfd;
  struct can_msg_s rx;
  ssize_t nbytes;

  (void)argc;
  (void)argv;

  if (canlight_open_leds() < 0)
    {
      g_running = false;
      g_daemon_pid = 0;
      return 1;
    }

  canfd = open(CONFIG_EXAMPLES_CANLIGHT_DEVPATH, O_RDONLY);
  if (canfd < 0)
    {
      printf("canlight: daemon open %s failed: %d\n",
             CONFIG_EXAMPLES_CANLIGHT_DEVPATH, errno);
      close(g_ledfd);
      g_ledfd = -1;
      g_running = false;
      g_daemon_pid = 0;
      return 1;
    }

  printf("canlight: daemon listening id=0x%x on %s\n",
         (unsigned)CONFIG_EXAMPLES_CANLIGHT_ID,
         CONFIG_EXAMPLES_CANLIGHT_DEVPATH);

  while (g_running)
    {
      nbytes = read(canfd, &rx, sizeof(rx));
      if (!g_running)
        {
          break;
        }

      if (nbytes < (ssize_t)CAN_MSGLEN(0))
        {
          if (errno == EINTR)
            {
              continue;
            }

          printf("canlight: read err nbytes=%ld errno=%d\n",
                 (long)nbytes, errno);
          usleep(50 * 1000);
          continue;
        }

      if (rx.cm_hdr.ch_id != CONFIG_EXAMPLES_CANLIGHT_ID)
        {
          continue; /* ignore canspeed 0x201 etc. */
        }

      if (rx.cm_hdr.ch_dlc < 1)
        {
          continue;
        }

      printf("canlight: RX id=0x%x data0=0x%02x\n",
             (unsigned)rx.cm_hdr.ch_id, rx.cm_data[0]);
      canlight_apply_mask((userled_set_t)(rx.cm_data[0] & 0x3));
    }

  close(canfd);
  if (g_ledfd >= 0)
    {
      ioctl(g_ledfd, ULEDIOC_SETALL, (userled_set_t)0);
      close(g_ledfd);
      g_ledfd = -1;
    }

  g_daemon_pid = 0;
  printf("canlight: daemon exit\n");
  return 0;
}

static int canlight_start(void)
{
  int pid;

  if (g_running && g_daemon_pid > 0)
    {
      printf("canlight: already running pid=%d\n", (int)g_daemon_pid);
      return 0;
    }

  g_running = true;
  pid = task_create("canlightd",
                    CONFIG_EXAMPLES_CANLIGHT_DAEMON_PRIORITY,
                    CONFIG_EXAMPLES_CANLIGHT_DAEMON_STACKSIZE,
                    canlight_daemon,
                    NULL);
  if (pid < 0)
    {
      g_running = false;
      printf("canlight: task_create failed: %d\n", errno);
      return 1;
    }

  g_daemon_pid = (pid_t)pid;
  printf("canlight: started daemon pid=%d\n", pid);
  printf("canlight: test: canlight 3   (both on via TX+loopback)\n");
  printf("canlight:       canlight 0   (both off)\n");
  printf("canlight:       canlight stop\n");
  return 0;
}

static int canlight_stop(void)
{
  pid_t pid = g_daemon_pid;

  g_running = false;
  if (pid > 0)
    {
      /* Unblock read() by deleting the daemon task. */

      task_delete(pid);
      g_daemon_pid = 0;
    }

  printf("canlight: stop requested\n");
  return 0;
}

static bool is_mask_arg(FAR const char *s)
{
  if (s == NULL || s[0] == '\0')
    {
      return false;
    }

  if (strcmp(s, "stop") == 0 || strcmp(s, "start") == 0)
    {
      return false;
    }

  /* accept 0, 1, 2, 3, 0x1, 0x3 ... */

  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
      return true;
    }

  return isdigit((unsigned char)s[0]) != 0;
}

/****************************************************************************
 * main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc > 1 && argv[1] != NULL)
    {
      if (strcmp(argv[1], "stop") == 0)
        {
          return canlight_stop();
        }

      if (strcmp(argv[1], "start") == 0)
        {
          return canlight_start();
        }

      if (is_mask_arg(argv[1]))
        {
          unsigned long mask = strtoul(argv[1], NULL, 0);
          return canlight_send_mask((unsigned)mask);
        }
    }

  return canlight_start();
}
