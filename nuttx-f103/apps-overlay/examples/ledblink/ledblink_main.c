/****************************************************************************
 * ledblink_main.c - Week7 NuttX user task demo (WarShip / stm32f103)
 *
 * Copy to: ~/automotive-nuttx/apps/examples/ledblink/
 *
 * Board must expose 2 USERLEDs:
 *   BOARD_NLEDS=2, g_ledcfg[] = { GPIO_LED1 (PB5), GPIO_LED2 (PE5) }
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#ifdef CONFIG_USERLED
#  include <nuttx/leds/userled.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#endif

#ifndef CONFIG_EXAMPLES_LEDBLINK_INTERVAL_MS
#  define CONFIG_EXAMPLES_LEDBLINK_INTERVAL_MS 500
#endif

/****************************************************************************
 * main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  unsigned long tick = 0;
  bool on = false;
  bool same = false;
#ifdef CONFIG_USERLED
  int fd = -1;
  userled_set_t ledset;
  userled_set_t supported = 0;
#endif

  if (argc > 1 && argv[1] != NULL && argv[1][0] == 's')
    {
      same = true;
    }

  printf("ledblink: start (interval=%d ms)\n",
         CONFIG_EXAMPLES_LEDBLINK_INTERVAL_MS);
  /* 0=opposite (default), 1=both same phase — set via argv[1]="same" */
  printf("ledblink: mode=%s (bits 0=PB5, 1=PE5)\n",
         (argc > 1 && argv[1][0] == 's') ? "same" : "opposite");

#ifdef CONFIG_USERLED
  fd = open("/dev/userleds", O_WRONLY);
  if (fd < 0)
    {
      printf("ledblink: open /dev/userleds failed, printf-only mode\n");
    }
  else if (ioctl(fd, ULEDIOC_SUPPORTED, (unsigned long)&supported) >= 0)
    {
      printf("ledblink: supported mask=0x%x\n", (unsigned)supported);
    }
#else
  printf("ledblink: CONFIG_USERLED off -> printf-only\n");
#endif

  while (1)
    {
      tick++;
      on = !on;

#ifdef CONFIG_USERLED
      if (fd >= 0)
        {
          if (same)
            {
              /* both on / both off together — good for wiring check */
              ledset = on ? (userled_set_t)0x3 : (userled_set_t)0x0;
            }
          else
            {
              /* opposite phase */
              ledset = on ? (userled_set_t)0x1 : (userled_set_t)0x2;
            }

          ioctl(fd, ULEDIOC_SETALL, ledset);
        }
#endif

      if (same)
        {
          printf("ledblink: tick=%lu both=%d\n", tick, on ? 1 : 0);
        }
      else
        {
          printf("ledblink: tick=%lu led0=%d led1=%d\n",
                 tick, on ? 1 : 0, on ? 0 : 1);
        }
      usleep(CONFIG_EXAMPLES_LEDBLINK_INTERVAL_MS * 1000);
    }

  return 0;
}
