/************************************************
 * WarShip F103 Bootloader + UART IAP demo
 * bootloader: 0x08000000 / 0x8000 (32KB)
 * app_boot  : 0x08008000
 *
 * Hold KEY0 / WK_UP ~2s -> stay bootloader (IAP mode)
 * Else if app valid -> jump app_boot
 * Else -> IAP mode (no valid app)
 *
 * PC: iap_uart_host.py  |  later: iap_can_host.py
 ************************************************/
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "iap_config.h"
#include "iap_proto.h"

static u8 App_IsValid_Quick(void)
{
	u32 sp = *(volatile u32 *)IAP_APP_ADDR;
	u32 rv = *(volatile u32 *)(IAP_APP_ADDR + 4u);

	if ((sp & 0x2FFE0000u) != 0x20000000u)
		return 0;
	if ((rv & 0xFF000000u) != 0x08000000u)
		return 0;
	if ((rv & 1u) == 0u)
		return 0;
	return 1;
}

int main(void)
{
	u8 force_boot = 0;
	u8 i;
	u8 k0, wu;
	u32 blink = 0;

	delay_init();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	uart_init(115200);
	LED_Init();
	KEY_Init();

	printf("\r\n=== BOOTLOADER 0x08000000 (32KB) + IAP ===\r\n");
	printf("Hold KEY0 or WK_UP for 2s to stay BOOTLOADER\r\n");

	for (i = 0; i < 20; i++)
	{
		k0 = KEY0;
		wu = WK_UP;
		printf("t=%d KEY0=%d WK_UP=%d\r\n", i, k0, wu);
		if (k0 == 0 || wu == 1)
			force_boot = 1;
		LED0 = !LED0;
		delay_ms(100);
	}

	if (!force_boot && App_IsValid_Quick())
	{
		IAP_Jump_To_App();
		printf("Jump failed\r\n");
	}
	else if (force_boot)
	{
		printf("Force BOOTLOADER / IAP\r\n");
	}
	else
	{
		printf("No valid APP -> IAP\r\n");
		printf("APP[0]=0x%08lX APP[1]=0x%08lX\r\n",
		       (unsigned long)(*(volatile u32 *)IAP_APP_ADDR),
		       (unsigned long)(*(volatile u32 *)(IAP_APP_ADDR + 4u)));
	}

	IAP_Proto_Init();

	while (1)
	{
		IAP_Proto_Poll();
		blink++;
		if ((blink % 200u) == 0u)
		{
			LED0 = !LED0;
		}
	}
}
