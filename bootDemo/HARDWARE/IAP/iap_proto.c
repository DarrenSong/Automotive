#include "iap_proto.h"
#include "iap_config.h"
#include "iap_flash.h"
#include "iap_can.h"
#include "delay.h"
#include "usart.h"
#include "stm32f10x_usart.h"

typedef void (*pFunction)(void);

#define APP_STACK_MASK    0x2FFE0000u
#define APP_STACK_EXPECT  0x20000000u

static u8 s_rx[8u + IAP_MAX_PAYLOAD + 2u];
static u8 s_tx[16u + 32u];
static u8 s_reply_can;

/* RX ring: IRQ pushes bytes so host burst won't overrun 1-byte DR */
#define IAP_RB_SIZE 512u
static volatile u8  s_rb[IAP_RB_SIZE];
static volatile u16 s_rb_w = 0;
static volatile u16 s_rb_r = 0;

extern volatile u8 IAP_UseRawRx;

void IAP_RxPush(u8 b)
{
	u16 next = (u16)((s_rb_w + 1u) % IAP_RB_SIZE);
	if (next == s_rb_r)
		return; /* drop if full */
	s_rb[s_rb_w] = b;
	s_rb_w = next;
}

static u16 IAP_Crc16(const u8 *data, u32 len)
{
	u16 crc = 0xFFFFu;
	u32 i;
	u8 bit;

	for (i = 0; i < len; i++)
	{
		crc ^= data[i];
		for (bit = 0; bit < 8u; bit++)
		{
			if (crc & 1u)
				crc = (crc >> 1) ^ 0xA001u;
			else
				crc >>= 1;
		}
	}
	return crc;
}

static u8 App_IsValid(u32 app_addr)
{
	u32 sp = *(volatile u32 *)app_addr;
	u32 rv = *(volatile u32 *)(app_addr + 4u);

	if ((sp & APP_STACK_MASK) != APP_STACK_EXPECT)
		return 0;
	if ((rv & 0xFF000000u) != 0x08000000u)
		return 0;
	if ((rv & 1u) == 0u)
		return 0;
	return 1;
}

void IAP_Jump_To_App(void)
{
	u32 sp;
	u32 rv;
	pFunction jump;

	if (!App_IsValid(IAP_APP_ADDR))
	{
		printf("APP invalid, stay IAP\r\n");
		return;
	}

	sp = *(volatile u32 *)IAP_APP_ADDR;
	rv = *(volatile u32 *)(IAP_APP_ADDR + 4u);
	jump = (pFunction)rv;

	printf("Jump to APP...\r\n");
	delay_ms(80);

	__disable_irq();
	USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
	USART_DeInit(USART1);

	__set_MSP(sp);
	jump();

	while (1)
	{
	}
}

static void Uart_Flush(void)
{
	__disable_irq();
	s_rb_r = s_rb_w;
	__enable_irq();
	while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
		(void)USART_ReceiveData(USART1);
}

static u8 Uart_GetByte(u8 *out, u32 timeout_ms)
{
	u32 t = 0;

	while (t < timeout_ms)
	{
		if (s_rb_r != s_rb_w)
		{
			*out = s_rb[s_rb_r];
			s_rb_r = (u16)((s_rb_r + 1u) % IAP_RB_SIZE);
			return 1;
		}
		delay_ms(1);
		t++;
	}
	return 0;
}

static void Uart_Send(const u8 *data, u32 len)
{
	u32 i;
	for (i = 0; i < len; i++)
	{
		while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
		{
		}
		USART_SendData(USART1, data[i]);
	}
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
	{
	}
}

static void IAP_SendResp(u8 cmd, u8 seq, u8 status, const u8 *payload, u16 plen)
{
	u16 crc;
	u16 i;

	if (plen > 32u)
		plen = 32u;

	s_tx[0] = IAP_ACK0;
	s_tx[1] = IAP_ACK1;
	s_tx[2] = cmd;
	s_tx[3] = seq;
	s_tx[4] = status;
	s_tx[5] = (u8)(plen >> 8);
	s_tx[6] = (u8)(plen & 0xFFu);
	for (i = 0; i < plen; i++)
		s_tx[7u + i] = payload[i];

	crc = IAP_Crc16(&s_tx[2], (u32)(5u + plen));
	s_tx[7u + plen] = (u8)(crc >> 8);
	s_tx[8u + plen] = (u8)(crc & 0xFFu);
	Uart_Send(s_tx, (u32)(9u + plen));
#if IAP_CAN_ENABLE
	/* Only CAN-reply when request arrived on CAN (no ACK on empty bus). */
	if (s_reply_can)
		IAP_Can_Send(s_tx, (u32)(9u + plen));
#endif
}

static u32 rd_be32(const u8 *p)
{
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static void wr_be32(u8 *p, u32 v)
{
	p[0] = (u8)(v >> 24);
	p[1] = (u8)(v >> 16);
	p[2] = (u8)(v >> 8);
	p[3] = (u8)(v);
}

static void IAP_Handle(u8 cmd, u8 seq, const u8 *payload, u16 plen)
{
	u8 info[16];
	u32 addr;
	u32 size;
	u8 st;

	switch (cmd)
	{
	case IAP_CMD_INFO:
		wr_be32(&info[0], IAP_APP_ADDR);
		wr_be32(&info[4], IAP_APP_SIZE);
		wr_be32(&info[8], IAP_PAGE_SIZE);
		wr_be32(&info[12], (u32)IAP_PROTO_VER);
		IAP_SendResp(cmd, seq, IAP_OK, info, 16u);
		break;

	case IAP_CMD_ERASE:
		if (plen != 8u)
		{
			IAP_SendResp(cmd, seq, IAP_ERR_PARAM, 0, 0);
			break;
		}
		addr = rd_be32(&payload[0]);
		size = rd_be32(&payload[4]);
		st = IAP_Flash_Erase(addr, size);
		if (st == 0)
			IAP_SendResp(cmd, seq, IAP_OK, 0, 0);
		else if (st == 1)
			IAP_SendResp(cmd, seq, IAP_ERR_RANGE, 0, 0);
		else
			IAP_SendResp(cmd, seq, IAP_ERR_FLASH, 0, 0);
		break;

	case IAP_CMD_WRITE:
		if (plen < 5u)
		{
			IAP_SendResp(cmd, seq, IAP_ERR_PARAM, 0, 0);
			break;
		}
		addr = rd_be32(&payload[0]);
		st = IAP_Flash_Write(addr, &payload[4], (u32)(plen - 4u));
		if (st == 0)
			IAP_SendResp(cmd, seq, IAP_OK, 0, 0);
		else if (st == 1)
			IAP_SendResp(cmd, seq, IAP_ERR_RANGE, 0, 0);
		else
			IAP_SendResp(cmd, seq, IAP_ERR_FLASH, 0, 0);
		break;

	case IAP_CMD_JUMP:
		if (!App_IsValid(IAP_APP_ADDR))
		{
			IAP_SendResp(cmd, seq, IAP_ERR_APP, 0, 0);
			break;
		}
		IAP_SendResp(cmd, seq, IAP_OK, 0, 0);
		delay_ms(50);
		IAP_Jump_To_App();
		break;

	default:
		IAP_SendResp(cmd, seq, IAP_ERR_CMD, 0, 0);
		break;
	}
}

void IAP_Frame_Dispatch(const u8 *frame, u16 len, u8 via_can)
{
	u8 cmd, seq;
	u16 plen;
	u16 crc_rx, crc_calc;

	s_reply_can = via_can ? 1u : 0u;

	if (frame == 0 || len < 9u)
		return;
	if (frame[0] != IAP_SOH0 || frame[1] != IAP_SOH1)
		return;

	cmd = frame[2];
	seq = frame[3];
	plen = ((u16)frame[4] << 8) | (u16)frame[5];
	if (plen > IAP_MAX_PAYLOAD)
	{
		IAP_SendResp(cmd, seq, IAP_ERR_PARAM, 0, 0);
		s_reply_can = 0;
		return;
	}
	if (len != (u16)(8u + plen)) /* SOH2+CMD+SEQ+LEN2+PAYLOAD+CRC2 = 8+plen */
	{
		IAP_SendResp(cmd, seq, IAP_ERR_PARAM, 0, 0);
		s_reply_can = 0;
		return;
	}

	crc_rx = ((u16)frame[6u + plen] << 8) | (u16)frame[7u + plen];
	crc_calc = IAP_Crc16(&frame[2], (u32)(4u + plen));
	if (crc_rx != crc_calc)
	{
		IAP_SendResp(cmd, seq, IAP_ERR_CRC, 0, 0);
		s_reply_can = 0;
		return;
	}

	IAP_Handle(cmd, seq, &frame[6], plen);
	s_reply_can = 0;
}

void IAP_Proto_Init(void)
{
	/* binary frames via IRQ ring buffer (do NOT disable RXNE) */
	USART_RX_STA = 0;
	IAP_UseRawRx = 1;
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	Uart_Flush();
	printf("IAP ready: UART @115200\r\n");
	printf("CMD: INFO=01 ERASE=02 WRITE=03 JUMP=04\r\n");
	printf("Tip: hold KEY0 if PC open resets board\r\n");

#if IAP_CAN_ENABLE
	printf("CAN IAP: ID RX=0x%03X TX=0x%03X @500k (chunked)\r\n",
	       (unsigned)IAP_CAN_RX_ID, (unsigned)IAP_CAN_TX_ID);
	if (IAP_Can_Init() == 0)
#if IAP_CAN_LOOPBACK
		printf("CAN IAP: init OK (LoopBack)\r\n");
#else
		printf("CAN IAP: init OK (Normal; test later with USB-CAN)\r\n");
#endif
	else
		printf("CAN IAP: init FAIL (UART still works)\r\n");
#endif
}

void IAP_Proto_Poll(void)
{
	u8 b0, b1;
	u8 cmd, seq;
	u16 plen;
	u16 i;
	u16 flen;

#if IAP_CAN_ENABLE
	IAP_Can_Poll();
#endif

	/* UART path: assemble one uart-style frame then dispatch */
	if (!Uart_GetByte(&b0, 5u))
		return;
	if (b0 != IAP_SOH0)
		return;
	if (!Uart_GetByte(&b1, 50u))
		return;
	if (b1 != IAP_SOH1)
		return;

	if (!Uart_GetByte(&cmd, 50u))
		return;
	if (!Uart_GetByte(&seq, 50u))
		return;
	if (!Uart_GetByte(&b0, 50u))
		return;
	if (!Uart_GetByte(&b1, 50u))
		return;
	plen = ((u16)b0 << 8) | (u16)b1;
	if (plen > IAP_MAX_PAYLOAD)
	{
		IAP_SendResp(cmd, seq, IAP_ERR_PARAM, 0, 0);
		Uart_Flush();
		return;
	}

	s_rx[0] = IAP_SOH0;
	s_rx[1] = IAP_SOH1;
	s_rx[2] = cmd;
	s_rx[3] = seq;
	s_rx[4] = (u8)(plen >> 8);
	s_rx[5] = (u8)(plen & 0xFFu);
	for (i = 0; i < plen; i++)
	{
		if (!Uart_GetByte(&s_rx[6u + i], 200u))
		{
			IAP_SendResp(cmd, seq, IAP_ERR_TIMEOUT, 0, 0);
			return;
		}
	}
	if (!Uart_GetByte(&s_rx[6u + plen], 50u))
		return;
	if (!Uart_GetByte(&s_rx[7u + plen], 50u))
		return;
	flen = (u16)(8u + plen);
	IAP_Frame_Dispatch(s_rx, flen, 0);
}
