#ifndef __IAP_FLASH_H
#define __IAP_FLASH_H
#include "sys.h"

u8 IAP_Flash_InAppRange(u32 addr, u32 len);
u8 IAP_Flash_Erase(u32 addr, u32 len);
u8 IAP_Flash_Write(u32 addr, const u8 *data, u32 len);

#endif
