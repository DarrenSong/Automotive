#include "iap_flash.h"
#include "iap_config.h"
#include "stm32f10x_flash.h"

u8 IAP_Flash_InAppRange(u32 addr, u32 len)
{
	u32 end;

	if (len == 0u)
		return 0;
	if (addr < IAP_APP_ADDR)
		return 0;
	end = addr + len;
	if (end < addr)
		return 0; /* overflow */
	if (end > IAP_FLASH_END)
		return 0;
	return 1;
}

u8 IAP_Flash_Erase(u32 addr, u32 len)
{
	u32 page;
	u32 end;
	FLASH_Status st;

	if (!IAP_Flash_InAppRange(addr, len))
		return 1;
	if ((addr % IAP_PAGE_SIZE) != 0u)
		return 1;

	end = addr + len;
	/* round up erase end to page boundary */
	if ((end % IAP_PAGE_SIZE) != 0u)
		end = (end + IAP_PAGE_SIZE - 1u) & ~(IAP_PAGE_SIZE - 1u);
	if (end > IAP_FLASH_END)
		return 1;

	FLASH_Unlock();
	for (page = addr; page < end; page += IAP_PAGE_SIZE)
	{
		st = FLASH_ErasePage(page);
		if (st != FLASH_COMPLETE)
		{
			FLASH_Lock();
			return 2;
		}
	}
	FLASH_Lock();
	return 0;
}

u8 IAP_Flash_Write(u32 addr, const u8 *data, u32 len)
{
	u32 i;
	u16 half;
	FLASH_Status st;

	if (data == 0)
		return 1;
	if (!IAP_Flash_InAppRange(addr, len))
		return 1;
	if ((addr & 1u) != 0u)
		return 1;

	FLASH_Unlock();
	for (i = 0; i < len; i += 2u)
	{
		if ((i + 1u) < len)
			half = (u16)data[i] | ((u16)data[i + 1u] << 8);
		else
			half = (u16)data[i] | 0xFF00u; /* pad odd last byte */

		st = FLASH_ProgramHalfWord(addr + i, half);
		if (st != FLASH_COMPLETE)
		{
			FLASH_Lock();
			return 2;
		}

		if (*(volatile u16 *)(addr + i) != half)
		{
			FLASH_Lock();
			return 3;
		}
	}
	FLASH_Lock();
	return 0;
}
