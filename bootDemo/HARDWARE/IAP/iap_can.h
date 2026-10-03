#ifndef __IAP_CAN_H
#define __IAP_CAN_H
#include "sys.h"
#include "iap_config.h"

#if IAP_CAN_ENABLE
u8 IAP_Can_Init(void);
void IAP_Can_Poll(void);
void IAP_Can_Send(const u8 *data, u32 len);
#else
#define IAP_Can_Init()    ((u8)0)
#define IAP_Can_Poll()    ((void)0)
#define IAP_Can_Send(d,l) ((void)0)
#endif

#endif
