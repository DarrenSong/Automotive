#include "iap_can.h"

#if IAP_CAN_ENABLE

#include "iap_config.h"
#include "iap_proto.h"
#include "stm32f10x_can.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "delay.h"

/* Reassemble host->MCU uart-style frame from CAN chunks */
static u8  s_asm[16u + IAP_MAX_PAYLOAD];
static u16 s_asm_need;
static u16 s_asm_got;
static u8  s_asm_idx;
static u8  s_can_ok;

u8 IAP_Can_Init(void)
{
	GPIO_InitTypeDef gi;
	CAN_InitTypeDef ci;
	CAN_FilterInitTypeDef fi;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

	/* WarShip default map: CANRX=PA11, CANTX=PA12 (no remap) */
	GPIO_PinRemapConfig(GPIO_Remap1_CAN1, DISABLE);

	gi.GPIO_Pin = GPIO_Pin_11;
	gi.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOA, &gi);

	gi.GPIO_Pin = GPIO_Pin_12;
	gi.GPIO_Speed = GPIO_Speed_50MHz;
	gi.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &gi);

	CAN_DeInit(CAN1);
	CAN_StructInit(&ci);
	ci.CAN_TTCM = DISABLE;
	ci.CAN_ABOM = ENABLE;
	ci.CAN_AWUM = DISABLE;
	ci.CAN_NART = DISABLE;
	ci.CAN_RFLM = DISABLE;
	ci.CAN_TXFP = DISABLE;
#if IAP_CAN_LOOPBACK
	ci.CAN_Mode = CAN_Mode_LoopBack;
#else
	ci.CAN_Mode = CAN_Mode_Normal;
#endif
	ci.CAN_SJW = CAN_SJW_1tq;
	ci.CAN_BS1 = CAN_BS1_9tq;
	ci.CAN_BS2 = CAN_BS2_8tq;
	ci.CAN_Prescaler = IAP_CAN_PRESCALER;
	if (CAN_Init(CAN1, &ci) == CAN_InitStatus_Failed)
	{
		s_can_ok = 0;
		return 1;
	}

	/* Accept only std ID IAP_CAN_RX_ID into FIFO0 */
	fi.CAN_FilterNumber = 0;
	fi.CAN_FilterMode = CAN_FilterMode_IdMask;
	fi.CAN_FilterScale = CAN_FilterScale_32bit;
	fi.CAN_FilterIdHigh = (u16)(IAP_CAN_RX_ID << 5);
	fi.CAN_FilterIdLow = 0x0000;
	fi.CAN_FilterMaskIdHigh = (u16)(0x7FF << 5);
	fi.CAN_FilterMaskIdLow = 0x0000;
	fi.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
	fi.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&fi);

	s_asm_need = 0;
	s_asm_got = 0;
	s_asm_idx = 0;
	s_can_ok = 1;
	return 0;
}

static u8 Can_Tx8(const u8 *data, u8 dlc)
{
	CanTxMsg tx;
	u8 mbox;
	u16 t = 0;

	if (!s_can_ok)
		return 1;

	tx.StdId = IAP_CAN_TX_ID;
	tx.ExtId = 0;
	tx.IDE = CAN_Id_Standard;
	tx.RTR = CAN_RTR_Data;
	tx.DLC = dlc;
	tx.Data[0] = data[0];
	tx.Data[1] = data[1];
	tx.Data[2] = data[2];
	tx.Data[3] = data[3];
	tx.Data[4] = data[4];
	tx.Data[5] = data[5];
	tx.Data[6] = data[6];
	tx.Data[7] = data[7];

	mbox = CAN_Transmit(CAN1, &tx);
	while ((CAN_TransmitStatus(CAN1, mbox) != CANTXOK) && (t < 5000u))
		t++;
	if (t >= 5000u)
		return 2;
	return 0;
}

void IAP_Can_Send(const u8 *data, u32 len)
{
	u8 chunk[8];
	u32 off;
	u8 n;
	u8 idx;

	if (data == 0 || len == 0u || len > 600u)
		return;

	/* FIRST: type, len_hi, len_lo, up to 5 payload bytes */
	chunk[0] = IAP_CAN_FIRST;
	chunk[1] = (u8)((len >> 8) & 0xFFu);
	chunk[2] = (u8)(len & 0xFFu);
	n = (u8)((len > 5u) ? 5u : len);
	for (off = 0; off < n; off++)
		chunk[3u + off] = data[off];
	(void)Can_Tx8(chunk, (u8)(3u + n));
	off = n;
	idx = 1;

	while (off < len)
	{
		chunk[0] = IAP_CAN_CONT;
		chunk[1] = idx++;
		n = (u8)(((len - off) > 6u) ? 6u : (len - off));
		{
			u8 i;
			for (i = 0; i < n; i++)
				chunk[2u + i] = data[off + i];
		}
		(void)Can_Tx8(chunk, (u8)(2u + n));
		off += n;
		delay_ms(1); /* give USB-CAN / bus a breath */
	}
}

static void Can_OnChunk(const CanRxMsg *rx)
{
	u16 total;
	u8 n;
	u8 i;

	if (rx->DLC < 1u)
		return;

	if (rx->Data[0] == IAP_CAN_FIRST)
	{
		if (rx->DLC < 3u)
			return;
		total = ((u16)rx->Data[1] << 8) | (u16)rx->Data[2];
		if (total < 9u || total > (u16)sizeof(s_asm))
		{
			s_asm_need = 0;
			return;
		}
		s_asm_need = total;
		s_asm_got = 0;
		s_asm_idx = 1;
		n = (u8)(rx->DLC - 3u);
		if (n > 5u)
			n = 5u;
		for (i = 0; i < n; i++)
			s_asm[s_asm_got++] = rx->Data[3u + i];
	}
	else if (rx->Data[0] == IAP_CAN_CONT)
	{
		if (s_asm_need == 0u || rx->DLC < 2u)
			return;
		if (rx->Data[1] != s_asm_idx)
		{
			s_asm_need = 0; /* sequence gap -> abort */
			return;
		}
		s_asm_idx++;
		n = (u8)(rx->DLC - 2u);
		if (n > 6u)
			n = 6u;
		for (i = 0; i < n; i++)
		{
			if (s_asm_got >= s_asm_need)
				break;
			s_asm[s_asm_got++] = rx->Data[2u + i];
		}
	}
	else
	{
		return;
	}

	if (s_asm_need != 0u && s_asm_got >= s_asm_need)
	{
		total = s_asm_need;
		s_asm_need = 0;
		IAP_Frame_Dispatch(s_asm, total, 1);
	}
}

void IAP_Can_Poll(void)
{
	CanRxMsg rx;

	if (!s_can_ok)
		return;

	while (CAN_MessagePending(CAN1, CAN_FIFO0) != 0)
	{
		CAN_Receive(CAN1, CAN_FIFO0, &rx);
		if (rx.IDE != CAN_Id_Standard)
			continue;
		if (rx.StdId != IAP_CAN_RX_ID)
			continue;
		Can_OnChunk(&rx);
	}
}

#endif /* IAP_CAN_ENABLE */
