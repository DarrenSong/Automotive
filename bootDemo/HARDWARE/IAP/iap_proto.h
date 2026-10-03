#ifndef __IAP_PROTO_H
#define __IAP_PROTO_H
#include "sys.h"

void IAP_Proto_Init(void);
void IAP_Proto_Poll(void);
void IAP_Jump_To_App(void);

/* Full uart-style frame: A5 5A | CMD | SEQ | LEN | PAYLOAD | CRC16
 * via_can=1 -> reply on CAN (and UART); via_can=0 -> UART only */
void IAP_Frame_Dispatch(const u8 *frame, u16 len, u8 via_can);

#endif
