/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_SMSC3503_H__
#define __LINUX_SMSC3503_H__

#define SMSC3503_VENDORID	0x00 
#define SMSC3503_PRODUCTID	0x02 
#define SMSC3503_DEVICEID	0x04 

#define SMSC3503_CONFIG_BYTE_1	0x06 
#define PORT_PWR	(1<<0)
#define EOP_DISABLE	(1<<3)
#define MTT_ENABLE	(1<<4)
#define HS_DISABLE	(1<<5)
#define SELF_BUS_PWR	(1<<7)

#define SMSC3503_CONFIG_BYTE_2	0x07 
#define SMSC3503_LANGID		0x11 
#define SMSC3503_MFRSL		0x13 
#define SMSC3503_PRDSL		0x14 
#define SMSC3503_SERSL		0x15 
#define SMSC3503_MANSTR		0x16 
#define SMSC3503_PRDSTR		0x54 
#define SMSC3503_SERSTR		0x92 

#define SMSC3503_SP_ILOCK	0xE7 
#define CONFIG_N	(1<<0)
#define CONNECT_N	(1<<1)
#define PRTPWRPINSEL	(1<<4)
#define OCSPINSEL	(1<<5)

struct smsc_hub_platform_data {
	unsigned hub_reset;
};

#endif
