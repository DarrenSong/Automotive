#ifndef __IAP_CONFIG_H
#define __IAP_CONFIG_H

/* Partition (WarShip F103ZE 512KB) */
#define IAP_BL_ADDR           0x08000000u
#define IAP_BL_SIZE           0x00008000u   /* 32KB bootloader */
#define IAP_APP_ADDR          0x08008000u
#define IAP_APP_SIZE          0x00078000u   /* rest of flash */
#define IAP_FLASH_END         0x08080000u
#define IAP_PAGE_SIZE         0x00000800u   /* 2KB, STM32F103 HD */

#define IAP_PROTO_VER         0x0001u
/* max frame payload bytes (includes 4-byte addr for WRITE) */
#define IAP_MAX_PAYLOAD       260u
#define IAP_WRITE_DATA_MAX    (IAP_MAX_PAYLOAD - 4u)  /* 256 */

/* Frame */
#define IAP_SOH0              0xA5u
#define IAP_SOH1              0x5Au
#define IAP_ACK0              0x5Au
#define IAP_ACK1              0xA5u

/* CMD */
#define IAP_CMD_INFO          0x01u
#define IAP_CMD_ERASE         0x02u
#define IAP_CMD_WRITE         0x03u
#define IAP_CMD_JUMP          0x04u

/* STATUS */
#define IAP_OK                0x00u
#define IAP_ERR_CRC           0x01u
#define IAP_ERR_PARAM         0x02u
#define IAP_ERR_FLASH         0x03u
#define IAP_ERR_RANGE         0x04u
#define IAP_ERR_APP           0x05u
#define IAP_ERR_TIMEOUT       0x06u
#define IAP_ERR_CMD           0x07u

/* CAN IAP transport (WarShip: PA11=RX PA12=TX, P9 jumper). Test when you have USB-CAN. */
#define IAP_CAN_ENABLE        1
#define IAP_CAN_LOOPBACK      0       /* 1 = loopback self-test without bus/tool */
#define IAP_CAN_RX_ID         0x100u  /* host -> MCU */
#define IAP_CAN_TX_ID         0x101u  /* MCU  -> host */
/* 500kbps @ 36MHz APB1: 36M/(4*(1+9+8))=500k ; bit timing in iap_can.c */
#define IAP_CAN_PRESCALER     4

/* CAN chunk header (classic CAN 8-byte data) */
#define IAP_CAN_FIRST         0x01u
#define IAP_CAN_CONT          0x02u

#endif
