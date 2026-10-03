/************************************************
 * app_boot side checklist (ASCII comments for Keil)
 * naming: bootloader @ 0x08000000 / 32KB
 *         app_boot   @ 0x08008000 / 0x78000
 * Keil IROM: Start=0x08008000, Size=0x78000
 ************************************************/

#include "sys.h"

int main(void)
{
	/* Must be first: relocate vector table to app_boot base */
	SCB->VTOR = 0x08008000u;
	__enable_irq();

	/* then: delay_init / uart_init / LED_Init / ... */
	while (1)
	{
	}
}

/*
 * Keil -> Options for Target -> Target
 *   IROM1 Start = 0x08008000
 *   IROM1 Size  = 0x78000
 *
 * Flash Download Algorithm: still 0x08000000 / 0x80000 (full chip)
 * Flash order: download app_boot first, then bootloader.
 * Normal reset -> app_boot; hold KEY0 then reset -> stay in bootloader.
 */
