/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifdef CONFIG_SPI_QUP
#include <linux/spi/spi.h>
#endif
#include <linux/leds.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_novatek.h"
#include "mdp4.h"
#include <mach/panel_id.h>
#include <mach/debug_display.h>

static struct mipi_dsi_panel_platform_data *mipi_novatek_pdata;

static struct dsi_buf novatek_tx_buf;
static struct dsi_buf novatek_rx_buf;
static int mipi_novatek_lcd_init(void);

struct dsi_cmd_desc *novatek_display_on_cmds = NULL;
struct dsi_cmd_desc *novatek_display_off_cmds = NULL;
int novatek_display_on_cmds_size = 0;
int novatek_display_off_cmds_size = 0;
char ptype[60] = "Panel Type = ";

static int wled_trigger_initialized;
static atomic_t lcd_power_state;
static atomic_t lcd_backlight_off;

#define MIPI_DSI_NOVATEK_SPI_DEVICE_NAME	"dsi_novatek_3d_panel_spi"
#define HPCI_FPGA_READ_CMD	0x84
#define HPCI_FPGA_WRITE_CMD	0x04

#define NOVATEK_CABC

///HTC:
#ifdef CONFIG_SPI_QUP
#undef CONFIG_SPI_QUP
#endif
///:HTC
#ifdef CONFIG_SPI_QUP
static struct spi_device *panel_3d_spi_client;

static void novatek_fpga_write(uint8 addr, uint16 value)
{
	char tx_buf[32];
	int  rc;
	struct spi_message  m;
	struct spi_transfer t;
	u8 data[4] = {0x0, 0x0, 0x0, 0x0};

	if (!panel_3d_spi_client) {
		pr_err("%s panel_3d_spi_client is NULL\n", __func__);
		return;
	}
	data[0] = HPCI_FPGA_WRITE_CMD;
	data[1] = addr;
	data[2] = ((value >> 8) & 0xFF);
	data[3] = (value & 0xFF);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	t.tx_buf = data;
	t.len = 4;
	spi_setup(panel_3d_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	rc = spi_sync(panel_3d_spi_client, &m);
	if (rc)
		pr_err("%s: SPI transfer failed\n", __func__);

	return;
}

static void novatek_fpga_read(uint8 addr)
{
	char tx_buf[32];
	int  rc;
	struct spi_message  m;
	struct spi_transfer t;
	struct spi_transfer rx;
	char rx_value[2];
	u8 data[4] = {0x0, 0x0};

	if (!panel_3d_spi_client) {
		pr_err("%s panel_3d_spi_client is NULL\n", __func__);
		return;
	}

	data[0] = HPCI_FPGA_READ_CMD;
	data[1] = addr;

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(&rx, 0, sizeof rx);
	memset(rx_value, 0, sizeof rx_value);
	t.tx_buf = data;
	t.len = 2;
	rx.rx_buf = rx_value;
	rx.len = 2;
	spi_setup(panel_3d_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	spi_message_add_tail(&rx, &m);

	rc = spi_sync(panel_3d_spi_client, &m);
	if (rc)
		pr_err("%s: SPI transfer failed\n", __func__);
	else
		pr_info("%s: rx_value = 0x%x, 0x%x\n", __func__,
						rx_value[0], rx_value[1]);

	return;
}

static int __devinit panel_3d_spi_probe(struct spi_device *spi)
{
	panel_3d_spi_client = spi;
	return 0;
}
static int __devexit panel_3d_spi_remove(struct spi_device *spi)
{
	panel_3d_spi_client = NULL;
	return 0;
}
static struct spi_driver panel_3d_spi_driver = {
	.probe         = panel_3d_spi_probe,
	.remove        = __devexit_p(panel_3d_spi_remove),
	.driver		   = {
		.name = "dsi_novatek_3d_panel_spi",
		.owner  = THIS_MODULE,
	}
};

#else

static void novatek_fpga_write(uint8 addr, uint16 value)
{
	return;
}

static void novatek_fpga_read(uint8 addr)
{
	return;
}

#endif


/* novatek blue panel */

#ifdef NOVETAK_COMMANDS_UNUSED
static char display_config_cmd_mode1[] = {
	/* TYPE_DCS_LWRITE */
	0x2A, 0x00, 0x00, 0x01,
	0x3F, 0xFF, 0xFF, 0xFF
};

static char display_config_cmd_mode2[] = {
	/* DTYPE_DCS_LWRITE */
	0x2B, 0x00, 0x00, 0x01,
	0xDF, 0xFF, 0xFF, 0xFF
};

static char display_config_cmd_mode3_666[] = {
	/* DTYPE_DCS_WRITE1 */
	0x3A, 0x66, 0x15, 0x80 /* 666 Packed (18-bits) */
};

static char display_config_cmd_mode3_565[] = {
	/* DTYPE_DCS_WRITE1 */
	0x3A, 0x55, 0x15, 0x80 /* 565 mode */
};

static char display_config_321[] = {
	/* DTYPE_DCS_WRITE1 */
	0x66, 0x2e, 0x15, 0x00 /* Reg 0x66 : 2E */
};

static char display_config_323[] = {
	/* DTYPE_DCS_WRITE */
	0x13, 0x00, 0x05, 0x00 /* Reg 0x13 < Set for Normal Mode> */
};

static char display_config_2lan[] = {
	/* DTYPE_DCS_WRITE */
	0x61, 0x01, 0x02, 0xff /* Reg 0x61 : 01,02 < Set for 2 Data Lane > */
};

static char display_config_exit_sleep[] = {
	/* DTYPE_DCS_WRITE */
	0x11, 0x00, 0x05, 0x80 /* Reg 0x11 < exit sleep mode> */
};

static char display_config_TE_ON[] = {
	/* DTYPE_DCS_WRITE1 */
	0x35, 0x00, 0x15, 0x80
};

static char display_config_39H[] = {
	/* DTYPE_DCS_WRITE */
	0x39, 0x00, 0x05, 0x80
};

static char display_config_set_tear_scanline[] = {
	/* DTYPE_DCS_LWRITE */
	0x44, 0x00, 0x00, 0xff
};

static char display_config_set_twolane[] = {
	/* DTYPE_DCS_WRITE1 */
	0xae, 0x03, 0x15, 0x80
};

static char display_config_set_threelane[] = {
	/* DTYPE_DCS_WRITE1 */
	0xae, 0x05, 0x15, 0x80
};

#else
#if 0
static char sw_reset[2] = {0x01, 0x00}; /* DTYPE_DCS_WRITE */
static char enter_sleep[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */
static char exit_sleep[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
static char display_off[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */
static char display_on[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */



static char rgb_888[2] = {0x3A, 0x77}; /* DTYPE_DCS_WRITE1 */

#if defined(NOVATEK_TWO_LANE)
static char set_num_of_lanes[2] = {0xae, 0x03}; /* DTYPE_DCS_WRITE1 */
#else  /* 1 lane */
static char set_num_of_lanes[2] = {0xae, 0x01}; /* DTYPE_DCS_WRITE1 */
#endif
/* commands by Novatke */
static char novatek_f4[2] = {0xf4, 0x55}; /* DTYPE_DCS_WRITE1 */
static char novatek_8c[16] = { /* DTYPE_DCS_LWRITE */
	0x8C, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x08, 0x00, 0x30, 0xC0, 0xB7, 0x37};
static char novatek_ff[2] = {0xff, 0x55 }; /* DTYPE_DCS_WRITE1 */

static char set_width[5] = { /* DTYPE_DCS_LWRITE */
	0x2A, 0x00, 0x00, 0x02, 0x1B}; /* 540 - 1 */
static char set_height[5] = { /* DTYPE_DCS_LWRITE */
	0x2B, 0x00, 0x00, 0x03, 0xBF}; /* 960 - 1 */
#endif
#endif
static char led_pwm1[2] = {0x51, 0xF0};	/* DTYPE_DCS_WRITE1 */
static char led_pwm2[2] = {0x53, 0x24}; /* DTYPE_DCS_WRITE1 */
#ifdef NOVATEK_CABC
static char dsi_novatek_dim_on[] = {0x53, 0x2C};/* DTYPE_DCS_LWRITE *///bkl_ctrl on and dimming on
static char dsi_novatek_dim_off[] = {0x53, 0x24};/* DTYPE_DCS_LWRITE *///bkl_ctrl on and dimming off

static struct dsi_cmd_desc novatek_dim_on_cmds[] = {
        {DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(dsi_novatek_dim_on), dsi_novatek_dim_on},
};

static struct dsi_cmd_desc novatek_dim_off_cmds[] = {
        {DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(dsi_novatek_dim_off), dsi_novatek_dim_off},
};
#endif
//static char led_pwm3[2] = {0x55, 0x00}; /* DTYPE_DCS_WRITE1 */

static struct dsi_cmd_desc novatek_cmd_backlight_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(led_pwm1), led_pwm1},
};
#if 0
static struct dsi_cmd_desc novatek_video_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50,
		sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(display_on), display_on},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(set_num_of_lanes), set_num_of_lanes},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(rgb_888), rgb_888},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(led_pwm3), led_pwm3},
};

static struct dsi_cmd_desc novatek_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50,
		sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(display_on), display_on},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 50,
		sizeof(novatek_f4), novatek_f4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 50,
		sizeof(novatek_8c), novatek_8c},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 50,
		sizeof(novatek_ff), novatek_ff},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(set_num_of_lanes), set_num_of_lanes},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 50,
		sizeof(set_width), set_width},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 50,
		sizeof(set_height), set_height},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(rgb_888), rgb_888},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1,
		sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1,
		sizeof(led_pwm3), led_pwm3},
};

static struct dsi_cmd_desc novatek_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(enter_sleep), enter_sleep}
};
#endif

/* K2 AUO panel initial setting */
static char k2_f0_1[] = {
    0xF0, 0x55, 0xAA, 0x52,
    0x08, 0x01}; /* DTYPE_DCS_LWRITE */
static char k2_b0_1[] = {
    0xB0, 0x0F}; /* DTYPE_DCS_WRITE1 */
static char k2_b1_1[] = {
    0xB1, 0x0F}; /* DTYPE_DCS_WRITE1 */
static char k2_b2[] = {
    0xB2, 0x00}; /* DTYPE_DCS_WRITE1 */
static char k2_b3[] = {
    0xB3, 0x07}; /* DTYPE_DCS_WRITE1 */
static char k2_b6_1[] = {
    0xB6, 0x14}; /* DTYPE_DCS_WRITE1 */
static char k2_b7_1[] = {
    0xB7, 0x15}; /* DTYPE_DCS_WRITE1 */
static char k2_b8_1[] = {
    0xB8, 0x24}; /* DTYPE_DCS_WRITE1 */
static char k2_b9[] = {
    0xB9, 0x36}; /* DTYPE_DCS_WRITE1 */
static char k2_ba[] = {
    0xBA, 0x24}; /* DTYPE_DCS_WRITE1 */
static char k2_bf[] = {
    0xBF, 0x01}; /* DTYPE_DCS_WRITE1 */
static char k2_c3[] = {
    0xC3, 0x11}; /* DTYPE_DCS_WRITE1 */
static char k2_c2[] = {
    0xC2, 0x00}; /* DTYPE_DCS_WRITE1 */
static char k2_c0[] = {
    0xC0, 0x00, 0x00}; /* DTYPE_DCS_LWRITE */
static char k2_bc_1[] = {
    0xBC, 0x00, 0x88, 0x00}; /* DTYPE_DCS_LWRITE */
static char k2_bd[] = {
    0xBD, 0x00, 0x88, 0x00}; /* DTYPE_DCS_LWRITE */

static char k2_d1[] = {
    0xD1, 0x00, 0x87, 0x00, 0x8F, 0x00, 0xA2, 0x00, 0xB1, 0x00,
    0xC1, 0x00, 0xDF, 0x00, 0xF5, 0x01, 0x1B, 0x01, 0x3E, 0x01,
    0x75, 0x01, 0xA3, 0x01, 0xEE, 0x02, 0x2A, 0x02, 0x2B, 0x02,
    0x62, 0x02, 0xA0, 0x02, 0xC9, 0x03, 0x01, 0x03, 0x20, 0x03,
    0x46, 0x03, 0x5F, 0x03, 0x7C, 0x03, 0x98, 0x03, 0xAC, 0x03,
    0xD4, 0x03, 0xF9};
static char k2_d2[] = {
    0xD2, 0x00, 0xD8, 0x00, 0xDC, 0x00, 0xEA, 0x00, 0xF4, 0x00,
    0xFF, 0x01, 0x12, 0x01, 0x24, 0x01, 0x44, 0x01, 0x60, 0x01,
    0x90, 0x01, 0xB8, 0x01, 0xFB, 0x02, 0x35, 0x02, 0x35, 0x02,
    0x6B, 0x02, 0xA7, 0x02, 0xD0, 0x03, 0x07, 0x03, 0x27, 0x03,
    0x4F, 0x03, 0x6A, 0x03, 0x8C, 0x03, 0xA7, 0x03, 0xC8, 0x03,
    0xE2, 0x03, 0xF8};
static char k2_d3[] = {
    0xD3, 0x00, 0x2B, 0x00, 0x3C, 0x00, 0x52, 0x00, 0x69, 0x00,
    0x7E, 0x00, 0xA0, 0x00, 0xC1, 0x00, 0xF0, 0x01, 0x19, 0x01,
    0x59, 0x01, 0x8D, 0x01, 0xDF, 0x02, 0x21, 0x02, 0x21, 0x02,
    0x5A, 0x02, 0x9B, 0x02, 0xC5, 0x02, 0xFF, 0x03, 0x1F, 0x03,
    0x49, 0x03, 0x66, 0x03, 0x97, 0x03, 0xBC, 0x03, 0xD1, 0x03,
    0xE1, 0x03, 0xFF};
static char k2_d4[] = {
    0xD4, 0x00, 0x87, 0x00, 0x8F, 0x00, 0xA2, 0x00, 0xB1, 0x00,
    0xC1, 0x00, 0xDF, 0x00, 0xF5, 0x01, 0x1B, 0x01, 0x3E, 0x01,
    0x75, 0x01, 0xA3, 0x01, 0xEE, 0x02, 0x2A, 0x02, 0x2B, 0x02,
    0x62, 0x02, 0xA0, 0x02, 0xC9, 0x03, 0x01, 0x03, 0x20, 0x03,
    0x46, 0x03, 0x5F, 0x03, 0x7C, 0x03, 0x98, 0x03, 0xAC, 0x03,
    0xD4, 0x03, 0xF9};
static char k2_d5[] = {
    0xD5, 0x00, 0xD8, 0x00, 0xDC, 0x00, 0xEA, 0x00, 0xF4, 0x00,
    0xFF, 0x01, 0x12, 0x01, 0x24, 0x01, 0x44, 0x01, 0x60, 0x01,
    0x90, 0x01, 0xB8, 0x01, 0xFB, 0x02, 0x35, 0x02, 0x35, 0x02,
    0x6B, 0x02, 0xA7, 0x02, 0xD0, 0x03, 0x07, 0x03, 0x27, 0x03,
    0x4F, 0x03, 0x6A, 0x03, 0x8C, 0x03, 0xA7, 0x03, 0xC8, 0x03,
    0xE2, 0x03, 0xF8};
static char k2_d6[] = {
    0xD6, 0x00, 0x2B, 0x00, 0x3C, 0x00, 0x52, 0x00, 0x69, 0x00,
    0x7E, 0x00, 0xA0, 0x00, 0xC1, 0x00, 0xF0, 0x01, 0x19, 0x01,
    0x59, 0x01, 0x8D, 0x01, 0xDF, 0x02, 0x21, 0x02, 0x21, 0x02,
    0x5A, 0x02, 0x9B, 0x02, 0xC5, 0x02, 0xFF, 0x03, 0x1F, 0x03,
    0x49, 0x03, 0x66, 0x03, 0x97, 0x03, 0xBC, 0x03, 0xD1, 0x03,
    0xE1, 0x03, 0xFF};

static char k2_f0_2[] = {
    0xF0, 0x55, 0xAA, 0x52,
    0x08, 0x00}; /* DTYPE_DCS_LWRITE */
static char k2_b6_2[] = {
    0xB6, 0x03}; /* DTYPE_DCS_WRITE1 */
static char k2_b7_2[] = {
    0xB7, 0x70, 0x70}; /* DTYPE_DCS_LWRITE */
static char k2_b8_2[] = {
    0xB8, 0x00, 0x02, 0x02,
    0x02}; /* DTYPE_DCS_LWRITE */
static char k2_bc_2[] = {
    0xBC, 0x00}; /* DTYPE_DCS_WRITE1 */
static char k2_b0_2[] = {
    0xB0, 0x00, 0x0A, 0x0E,
    0x09, 0x04}; /* DTYPE_DCS_LWRITE */
static char k2_b1_2[] = {
    0xB1, 0x60, 0x00, 0x01}; /* DTYPE_DCS_LWRITE */
static char k2_e0[] = {
    0xE0, 0x01, 0x01};/* DTYPE_DCS_LWRITE *//* PWM frequency = 39.06 KHz*/

#ifdef NOVATEK_CABC
static char k2_e3[] = {
    0xE3, 0xFF, 0xF7, 0xEF,
    0xE7, 0xDF, 0xD7, 0xCF,
    0xC7, 0xBF, 0xB7}; /* DTYPE_DCS_LWRITE */
static char k2_5e[] = {
    0x5E, 0x06}; /* DTYPE_DCS_WRITE1 */
static char led_cabc_pwm_on[] = {
    0x55, 0x02}; /* DTYPE_DCS_WRITE1 */
#ifdef CONFIG_MSM_ACL_ENABLE
static char led_cabc_pwm_off[] = {
    0x55, 0x00}; /* DTYPE_DCS_WRITE1 */
#endif
static char k2_f5 [] = {
    0xF5, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x00, 0xD9,
    0x17}; /* DTYPE_DCS_LWRITE */
#ifdef CONFIG_MSM_CABC_VIDEO_ENHANCE
static char k2_e3_video[] = {
    0xE3, 0xFF, 0xF1, 0xE0,
    0xCE, 0xBE, 0xAD, 0x9C,
    0x8A, 0x78, 0x66}; /* DTYPE_DCS_LWRITE */
static char k2_f5_video[] = {
    0xF5, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x00, 0xD9,
    0x17}; /* DTYPE_DCS_LWRITE */
#endif
#endif

static char k2_ff_1[] = {
    0xFF, 0xAA, 0x55, 0xA5,
    0x80}; /* DTYPE_DCS_LWRITE */
static char k2_f7[] = {
    0xF7, 0x63, 0x40, 0x00,
    0x00, 0x00, 0x01, 0xC4,
    0xA2, 0x00, 0x02, 0x64,
    0x54, 0x48, 0x00, 0xD0}; /* DTYPE_DCS_LWRITE */
static char k2_f8[] = {
    0xF8, 0x00, 0x00, 0x33,
    0x0F, 0x0F, 0x20, 0x00,
    0x01, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00}; /* DTYPE_DCS_LWRITE */

/* Vivid color & Skin tone start */
static char k2_b4[] = {
    0xB4, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /*DTYPE_DCS_LWRITE*/
static char k2_d6_1[] = {
    0xD6, 0x00, 0x05, 0x10, 0x17, 0x22, 0x26, 0x29, 0x29, 0x26, 0x23, 0x17, 0x12, 0x06, 0x02, 0x01, 0x00}; /*DTYPE_DCS_LWRITE*/
static char k2_d7[] = {
    0xD7, 0x30, 0x30, 0x30, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /*DTYPE_DCS_LWRITE*/
static char k2_d8[] = {
    0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x30, 0x00}; /*DTYPE_DCS_LWRITE*/
/* Vivid color & Skin tone end */

static char k2_f0_3[] = {
    0xF0, 0x55, 0xAA, 0x52,
    0x00, 0x00}; /* DTYPE_DCS_LWRITE */
static char k2_ff_2[] = {
    0xFF, 0xAA, 0x55, 0xA5,
    0x00}; /* DTYPE_DCS_LWRITE */

static char k2_peripheral_on[] = {0x00, 0x00}; /* DTYPE_PERIPHERAL_ON */
static char k2_peripheral_off[] = {0x00, 0x00}; /* DTYPE_PERIPHERAL_OFF */

static struct dsi_cmd_desc k2_auo_display_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f0_1), k2_f0_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b0_1), k2_b0_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b1_1), k2_b1_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b2), k2_b2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b3), k2_b3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b6_1), k2_b6_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b7_1), k2_b7_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b8_1), k2_b8_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b9), k2_b9},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_ba), k2_ba},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_bf), k2_bf},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_c3), k2_c3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_c2), k2_c2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_c0), k2_c0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_bc_1), k2_bc_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_bd), k2_bd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d1), k2_d1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d2), k2_d2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d3), k2_d3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d4), k2_d4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d5), k2_d5},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d6), k2_d6},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f0_2), k2_f0_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b6_2), k2_b6_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b7_2), k2_b7_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b8_2), k2_b8_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_bc_2), k2_bc_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b0_2), k2_b0_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b1_2), k2_b1_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_e0), k2_e0},
#ifdef NOVATEK_CABC
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_e3), k2_e3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_5e), k2_5e},
#ifdef CONFIG_MSM_ACL_ENABLE
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_cabc_pwm_off), led_cabc_pwm_off},
#else
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_cabc_pwm_on), led_cabc_pwm_on},
#endif
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_1), k2_ff_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f5), k2_f5},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_2), k2_ff_2},
#endif
	/* Only need for AUO cut1 */
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_1), k2_ff_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f7), k2_f7},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f8), k2_f8},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b4), k2_b4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d6_1), k2_d6_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d7), k2_d7},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d8), k2_d8},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f0_3), k2_f0_3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_2), k2_ff_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_pwm2), led_pwm2},
	{DTYPE_PERIPHERAL_ON, 1, 0, 1, 120, sizeof(k2_peripheral_on), k2_peripheral_on},
};

/* K2 AUO cut 2 panel initial setting */
static struct dsi_cmd_desc k2_auo_display_on_cmds_c2[] = {
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f0_1), k2_f0_1},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b0_1), k2_b0_1},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b1_1), k2_b1_1},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b2), k2_b2},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b3), k2_b3},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b6_1), k2_b6_1},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b7_1), k2_b7_1},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b8_1), k2_b8_1},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b9), k2_b9},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_ba), k2_ba},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_bf), k2_bf},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_c3), k2_c3},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_c2), k2_c2},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_c0), k2_c0},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_bc_1), k2_bc_1},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_bd), k2_bd},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d1), k2_d1},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d2), k2_d2},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d3), k2_d3},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d4), k2_d4},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d5), k2_d5},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d6), k2_d6},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f0_2), k2_f0_2},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_b6_2), k2_b6_2},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b7_2), k2_b7_2},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b8_2), k2_b8_2},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_bc_2), k2_bc_2},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b0_2), k2_b0_2},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b1_2), k2_b1_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_e0), k2_e0},
#ifdef NOVATEK_CABC
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_e3), k2_e3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_5e), k2_5e},
#ifdef CONFIG_MSM_ACL_ENABLE
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_cabc_pwm_off), led_cabc_pwm_off},
#else
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_cabc_pwm_on), led_cabc_pwm_on},
#endif
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_1), k2_ff_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f5), k2_f5},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_2), k2_ff_2},
#endif
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_b4), k2_b4},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d6_1), k2_d6_1},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d7), k2_d7},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_d8), k2_d8},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f0_3), k2_f0_3},
        {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_2), k2_ff_2},
        {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_pwm2), led_pwm2},
        {DTYPE_PERIPHERAL_ON, 1, 0, 1, 120, sizeof(k2_peripheral_on), k2_peripheral_on},
};

/* K2 JDI panel initial setting */
static char k2_jdi_f0[] = {
    0xF0, 0x55, 0xAA, 0x52,
    0x08, 0x00}; /* DTYPE_DCS_LWRITE */
static char k2_jdi_b1[] = {
    0xB1, 0x68, 0x00, 0x01}; /* DTYPE_GEN_WRITE2 */
/* Vivid color & Skin tone start */
static char k2_jdi_b4[] = {
    0xB4, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /*DTYPE_DCS_LWRITE*/
static char k2_jdi_d6_2[] = {
    0xD6, 0x00, 0x05, 0x10, 0x17, 0x22, 0x26, 0x29, 0x29, 0x26, 0x23, 0x17, 0x12, 0x06, 0x02, 0x01, 0x00}; /*DTYPE_DCS_LWRITE*/
static char k2_jdi_d7[] = {
    0xD7, 0x30, 0x30, 0x30, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /*DTYPE_DCS_LWRITE*/
static char k2_jdi_d8[] = {
    0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x30, 0x00}; /*DTYPE_DCS_LWRITE*/
/* Vivid color & Skin tone end */
static char k2_jdi_b6[] = {
    0xB6, 0x07}; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b7[] = {
    0xB7, 0x33, 0x03}; /* DTYPE_DCS_LWRITE */
static char k2_jdi_b8[] = {
    0xB8, 0x00, 0x00, 0x02, 0x00}; /* DTYPE_DCS_LWRITE */
static char k2_jdi_ba[] = {
    0xBA, 0x01}; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_bb[] = {
    0xBB, 0x44, 0x40}; /* DTYPE_DCS_LWRITE */
static char k2_jdi_c1[] = {
    0xC1, 0x01 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_c2[] = {
    0xC2, 0x00, 0x00, 0x55,
    0x55, 0x55, 0x00, 0x55,
    0x55};/* DTYPE_DCS_LWRITE */
static char k2_jdi_c7[] = {
    0xC7, 0x00 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_ca[] = {
    0xCA, 0x05, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x01, 0x00};/* DTYPE_DCS_LWRITE */
static char k2_jdi_e0[] = {
    0xE0, 0x01, 0x01};/* DTYPE_DCS_LWRITE *//* PWM frequency = 39.06 KHz*/
static char k2_jdi_e1[] = {
    0xE1, 0x00, 0xFF};/* DTYPE_DCS_LWRITE */
static char k2_jdi_f0_1[] = {
    0xF0, 0x55, 0xAA, 0x52,
    0x08, 0x01}; /* DTYPE_DCS_LWRITE */
static char k2_jdi_b0[] = {
    0xB0, 0x0A }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b1_1[] = {
    0xB1, 0x0A }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b2_1[] = {
    0xB2, 0x00 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b3[] = {
    0xB3, 0x08 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b4_1[] = {
    0xB4, 0x28 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b5[] = {
    0xB5, 0x05 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b6_1[] = {
    0xB6, 0x35};/* DTYPE_GEN_WRITE2 */
static char k2_jdi_b7_1[] = {
    0xB7, 0x35 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b8_1[] = {
    0xB8, 0x25 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_b9[] = {
    0xB9, 0x37 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_ba_1[] = {
    0xBA, 0x15 }; /* DTYPE_GEN_WRITE2 */
static char k2_jdi_cc[] = {
    0xCC, 0x64 }; /* DTYPE_GEN_WRITE2 */
//Gamma Table
static char k2_jdi_d1[] = {
    0xD1, 0x00, 0xC7, 0x00, 0xCF, 0x00, 0xE2, 0x00,
    0xE9, 0x00, 0xF8, 0x01, 0x0F, 0x01, 0x23, 0x01,
    0x45, 0x01, 0x62, 0x01, 0x93, 0x01, 0xBB, 0x01,
    0xFB, 0x02, 0x2D, 0x02, 0x2E, 0x02, 0x62, 0x02,
    0x98, 0x02, 0xBA, 0x02, 0xEB, 0x03, 0x0D, 0x03,
    0x38, 0x03, 0x53, 0x03, 0x7A, 0x03, 0x97, 0x03,
    0xA6, 0x03, 0xCA, 0x03, 0xD0};
static char k2_jdi_d2[] = {
    0xD2, 0x00, 0x98, 0x00, 0xA1, 0x00, 0xBA, 0x00,
    0xC8, 0x00, 0xD7, 0x00, 0xF3, 0x01, 0x0B, 0x01,
    0x32, 0x01, 0x52, 0x01, 0x87, 0x01, 0xB2, 0x01,
    0xF4, 0x02, 0x29, 0x02, 0x2A, 0x02, 0x5F, 0x02,
    0x96, 0x02, 0xB8, 0x02, 0xE9, 0x03, 0x0B, 0x03,
    0x37, 0x03, 0x53, 0x03, 0x7A, 0x03, 0x96, 0x03,
    0xAA, 0x03, 0xCA, 0x03, 0xD0};
static char k2_jdi_d3[] = {
    0xD3, 0x00, 0x3F, 0x00, 0x4C, 0x00, 0x71, 0x00,
    0x7E, 0x00, 0x94, 0x00, 0xBB, 0x00, 0xD8, 0x01,
    0x08, 0x01, 0x2D, 0x01, 0x6A, 0x01, 0x9B, 0x01,
    0xE6, 0x02, 0x1F, 0x02, 0x20, 0x02, 0x57, 0x02,
    0x91, 0x02, 0xB4, 0x02, 0xE7, 0x03, 0x09, 0x03,
    0x37, 0x03, 0x54, 0x03, 0x7B, 0x03, 0x93, 0x03,
    0xB3, 0x03, 0xCA, 0x03, 0xD0};
static char k2_jdi_d4[] = {
    0xD4, 0x00, 0xC7, 0x00, 0xCF, 0x00, 0xE2, 0x00,
    0xE9, 0x00, 0xF8, 0x01, 0x0F, 0x01, 0x23, 0x01,
    0x45, 0x01, 0x62, 0x01, 0x93, 0x01, 0xBB, 0x01,
    0xFB, 0x02, 0x2D, 0x02, 0x2E, 0x02, 0x62, 0x02,
    0x98, 0x02, 0xBA, 0x02, 0xEB, 0x03, 0x0D, 0x03,
    0x38, 0x03, 0x53, 0x03, 0x7A, 0x03, 0x97, 0x03,
    0xA6, 0x03, 0xCA, 0x03, 0xD0};
static char k2_jdi_d5[] = {
    0xD5, 0x00, 0x98, 0x00, 0xA1, 0x00, 0xBA, 0x00,
    0xC8, 0x00, 0xD7, 0x00, 0xF3, 0x01, 0x0B, 0x01,
    0x32, 0x01, 0x52, 0x01, 0x87, 0x01, 0xB2, 0x01,
    0xF4, 0x02, 0x29, 0x02, 0x2A, 0x02, 0x5F, 0x02,
    0x96, 0x02, 0xB8, 0x02, 0xE9, 0x03, 0x0B, 0x03,
    0x37, 0x03, 0x53, 0x03, 0x7A, 0x03, 0x96, 0x03,
    0xAA, 0x03, 0xCA, 0x03, 0xD0};
static char k2_jdi_d6_1[] = {
    0xD6, 0x00, 0x3F, 0x00, 0x4C, 0x00, 0x71, 0x00,
    0x7E, 0x00, 0x94, 0x00, 0xBB, 0x00, 0xD8, 0x01,
    0x08, 0x01, 0x2D, 0x01, 0x6A, 0x01, 0x9B, 0x01,
    0xE6, 0x02, 0x1F, 0x02, 0x20, 0x02, 0x57, 0x02,
    0x91, 0x02, 0xB4, 0x02, 0xE7, 0x03, 0x09, 0x03,
    0x37, 0x03, 0x54, 0x03, 0x7B, 0x03, 0x93, 0x03,
    0xB3, 0x03, 0xCA, 0x03, 0xD0};

static char k2_jdi_f0_2[] = {
    0xF0, 0x55, 0xAA, 0x52,
    0x00, 0x00}; /* DTYPE_DCS_LWRITE */
static char k2_jdi_ff_2[] = {
    0xFF, 0xAA, 0x55, 0xA5,
    0x00}; /* DTYPE_DCS_LWRITE */

static char k2_jdi_pwm2[] = {0x53, 0x24}; /* DTYPE_GEN_WRITE2 */

static char k2_jdi_peripheral_on[] = {0x00, 0x00}; /* DTYPE_PERIPHERAL_ON */
static char k2_jdi_peripheral_off[] = {0x00, 0x00};


static struct dsi_cmd_desc  k2_jdi_display_on_cmds[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_f0), k2_jdi_f0},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_b1), k2_jdi_b1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_b4), k2_jdi_b4},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d6_2), k2_jdi_d6_2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d7), k2_jdi_d7},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d8), k2_jdi_d8},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b6), k2_jdi_b6},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_b7), k2_jdi_b7},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_b8), k2_jdi_b8},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_ba), k2_jdi_ba},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_bb), k2_jdi_bb},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_c1), k2_jdi_c1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_c2), k2_jdi_c2},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_c7), k2_jdi_c7},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_ca), k2_jdi_ca},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_e0), k2_jdi_e0},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_e1), k2_jdi_e1},
#ifdef NOVATEK_CABC
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_e3), k2_e3},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(k2_5e), k2_5e},
#ifdef CONFIG_MSM_ACL_ENABLE
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_cabc_pwm_off), led_cabc_pwm_off},
#else
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_cabc_pwm_on), led_cabc_pwm_on},
#endif
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_1), k2_ff_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f5), k2_f5},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_2), k2_ff_2},
#endif
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_f0_1), k2_jdi_f0_1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b0), k2_jdi_b0},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b1_1), k2_jdi_b1_1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b2_1), k2_jdi_b2_1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b3), k2_jdi_b3},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b4_1), k2_jdi_b4_1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b5), k2_jdi_b5},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b6_1), k2_jdi_b6_1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b7_1), k2_jdi_b7_1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b8_1), k2_jdi_b8_1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_b9), k2_jdi_b9},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_ba_1), k2_jdi_ba_1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_cc), k2_jdi_cc},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d1), k2_jdi_d1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d2), k2_jdi_d2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d3), k2_jdi_d3},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d4), k2_jdi_d4},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d5), k2_jdi_d5},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_d6_1), k2_jdi_d6_1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_f0_2), k2_jdi_f0_2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(k2_jdi_ff_2), k2_jdi_ff_2},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(k2_jdi_pwm2), k2_jdi_pwm2},
    {DTYPE_PERIPHERAL_ON, 1, 0, 1, 120, sizeof(k2_jdi_peripheral_on), k2_jdi_peripheral_on},
};

static struct dsi_cmd_desc k2_jdi_display_off_cmds[] = {
	{DTYPE_PERIPHERAL_OFF, 1, 0, 1, 70, sizeof(k2_jdi_peripheral_off), k2_jdi_peripheral_off},
};

static struct dsi_cmd_desc k2_auo_display_off_cmds[] = {
	{DTYPE_PERIPHERAL_OFF, 1, 0, 1, 70, sizeof(k2_peripheral_off), k2_peripheral_off},
};

#ifdef CONFIG_MSM_ACL_ENABLE
static struct dsi_cmd_desc novatek_cabc_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(led_cabc_pwm_on), led_cabc_pwm_on},
};

static struct dsi_cmd_desc novatek_cabc_off_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(led_cabc_pwm_off), led_cabc_pwm_off},
};
#endif

#ifdef CONFIG_MSM_CABC_VIDEO_ENHANCE
static struct dsi_cmd_desc novatek_cabc_UI_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_e3), k2_e3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_1), k2_ff_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f5), k2_f5},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_2), k2_ff_2},
};
static struct dsi_cmd_desc novatek_cabc_video_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_e3_video), k2_e3_video},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_1), k2_ff_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_f5_video), k2_f5_video},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(k2_ff_2), k2_ff_2},
};
#endif

static char manufacture_id[2] = {0x04, 0x00}; /* DTYPE_DCS_READ */

static struct dsi_cmd_desc novatek_manufacture_id_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id), manufacture_id};

static uint32 mipi_novatek_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
	struct dsi_cmd_desc *cmd;
	uint8 *lp;
	int count;

	tp = &novatek_tx_buf;
	rp = &novatek_rx_buf;
	cmd = &novatek_manufacture_id_cmd;
	count = mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 3);
	lp = (uint8 *)rp->data;
	PR_DISP_INFO("%s: manufacture_id=0x%02x%02x%02x count=%d\n", __func__, lp[7], lp[8], lp[9], count);
	//pr_info("%s: addr=0x04 manufacture_id=0x%x\n", __func__, *lp);

	return *lp;
}

static int fpga_addr;
static int fpga_access_mode;
static bool support_3d;

static void mipi_novatek_3d_init(int addr, int mode)
{
	fpga_addr = addr;
	fpga_access_mode = mode;
}

static void mipi_dsi_enable_3d_barrier(int mode)
{
	void __iomem *fpga_ptr;
	uint32_t ptr_value = 0;

	if (!fpga_addr && support_3d) {
		pr_err("%s: fpga_addr not set. Failed to enable 3D barrier\n",
					__func__);
		return;
	}

	if (fpga_access_mode == FPGA_SPI_INTF) {
		if (mode == LANDSCAPE)
			novatek_fpga_write(fpga_addr, 1);
		else if (mode == PORTRAIT)
			novatek_fpga_write(fpga_addr, 3);
		else
			novatek_fpga_write(fpga_addr, 0);

		mb();
		novatek_fpga_read(fpga_addr);
	} else if (fpga_access_mode == FPGA_EBI2_INTF) {
		fpga_ptr = ioremap_nocache(fpga_addr, sizeof(uint32_t));
		if (!fpga_ptr) {
			pr_err("%s: FPGA ioremap failed."
				"Failed to enable 3D barrier\n",
						__func__);
			return;
		}

		ptr_value = readl_relaxed(fpga_ptr);
		if (mode == LANDSCAPE)
			writel_relaxed(((0xFFFF0000 & ptr_value) | 1),
								fpga_ptr);
		else if (mode == PORTRAIT)
			writel_relaxed(((0xFFFF0000 & ptr_value) | 3),
								fpga_ptr);
		else
			writel_relaxed((0xFFFF0000 & ptr_value),
								fpga_ptr);

		mb();
		iounmap(fpga_ptr);
	} else
		pr_err("%s: 3D barrier not configured correctly\n",
					__func__);
}

static void mipi_novatek_panel_type_detect(struct mipi_panel_info *mipi)
{
	if (panel_type == PANEL_ID_K2_WL_AUO) {
		PR_DISP_INFO("%s: panel_type=PANEL_ID_K2_WL_AUO\n", __func__);
		strcat(ptype, "PANEL_ID_K2_WL_AUO");
		if (mipi->mode == DSI_VIDEO_MODE) {
			novatek_display_on_cmds = k2_auo_display_on_cmds;
			novatek_display_on_cmds_size = ARRAY_SIZE(k2_auo_display_on_cmds);
			novatek_display_off_cmds = k2_auo_display_off_cmds;
			novatek_display_off_cmds_size = ARRAY_SIZE(k2_auo_display_off_cmds);
		}
	} else if (panel_type == PANEL_ID_K2_WL_AUO_C2) {
		PR_DISP_INFO("%s: panel_type=PANEL_ID_K2_WL_AUO_C2\n", __func__);
		strcat(ptype, "PANEL_ID_K2_WL_AUO_C2");
		if (mipi->mode == DSI_VIDEO_MODE) {
			novatek_display_on_cmds = k2_auo_display_on_cmds_c2;
			novatek_display_on_cmds_size = ARRAY_SIZE(k2_auo_display_on_cmds_c2);
			novatek_display_off_cmds = k2_auo_display_off_cmds;
			novatek_display_off_cmds_size = ARRAY_SIZE(k2_auo_display_off_cmds);
		}
	} else if (panel_type == PANEL_ID_K2_WL_JDI_NT) {
		PR_DISP_INFO("%s: panel_type=PANEL_ID_K2_WL_JDI\n", __func__);
		strcat(ptype, "PANEL_ID_K2_WL_JDI");
		if (mipi->mode == DSI_VIDEO_MODE) {
			novatek_display_on_cmds = k2_jdi_display_on_cmds;
			novatek_display_on_cmds_size = ARRAY_SIZE(k2_jdi_display_on_cmds);
			novatek_display_off_cmds = k2_jdi_display_off_cmds;
			novatek_display_off_cmds_size = ARRAY_SIZE(k2_jdi_display_off_cmds);
		}
        } else if (panel_type == PANEL_ID_K2_WL_JDI_NT_T02) {
		PR_DISP_INFO("%s: panel_type=PANEL_ID_K2_WL_JDI_T02\n", __func__);
		strcat(ptype, "PANEL_ID_K2_WL_JDI_T02");
		if (mipi->mode == DSI_VIDEO_MODE) {
			novatek_display_on_cmds = k2_jdi_display_on_cmds;
			novatek_display_on_cmds_size = ARRAY_SIZE(k2_jdi_display_on_cmds);
			novatek_display_off_cmds = k2_jdi_display_off_cmds;
			novatek_display_off_cmds_size = ARRAY_SIZE(k2_jdi_display_off_cmds);
		}
        }
}

static int mipi_novatek_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	struct msm_panel_info *pinfo;

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	pinfo = &mfd->panel_info;
	if (pinfo->is_3d_panel)
		support_3d = TRUE;

	mipi  = &mfd->panel_info.mipi;

	if (mfd->init_mipi_lcd == 0) {
		PR_DISP_DEBUG("Display On - 1st time\n");

		mipi_novatek_panel_type_detect(mipi);

		mfd->init_mipi_lcd = 1;
		return 0;
	}

	PR_DISP_DEBUG("Display On \n");
	PR_DISP_INFO("%s\n", ptype);

	mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_display_on_cmds,
		novatek_display_on_cmds_size);

	mipi_dsi_cmd_bta_sw_trigger(); /* clean up ack_err_status */

	mipi_novatek_manufacture_id(mfd);

	atomic_set(&lcd_power_state, 1);

	PR_DISP_DEBUG("Init done!\n");

	return 0;
}

static int mipi_novatek_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_display_off_cmds,
			novatek_display_off_cmds_size);

	atomic_set(&lcd_power_state, 0);

	return 0;
}

DEFINE_LED_TRIGGER(bkl_led_trigger);

static void mipi_novatek_set_backlight(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi;
///HTC:
	if (mipi_novatek_pdata && mipi_novatek_pdata->shrink_pwm)
		led_pwm1[1] = mipi_novatek_pdata->shrink_pwm(mfd->bl_level);
	else
		led_pwm1[1] = (unsigned char)(mfd->bl_level);
///:HTC

	if (mipi_novatek_pdata && (mipi_novatek_pdata->enable_wled_bl_ctrl)
	    && (wled_trigger_initialized)) {
		led_trigger_event(bkl_led_trigger, led_pwm1[1]);
		return;
	}
	mipi  = &mfd->panel_info.mipi;

	mutex_lock(&mfd->dma->ov_mutex);

	/* Check LCD power state */
	if (atomic_read(&lcd_power_state) == 0) {
		PR_DISP_DEBUG("%s: LCD is off. Skip backlight setting\n", __func__);
		mutex_unlock(&mfd->dma->ov_mutex);
		return;
	}

	if (mdp4_overlay_dsi_state_get() <= ST_DSI_SUSPEND) {
		mutex_unlock(&mfd->dma->ov_mutex);
		return;
	}
	/* mdp4_dsi_cmd_busy_wait: will turn on dsi clock also */
	mdp4_dsi_cmd_dma_busy_wait(mfd);
	mipi_dsi_mdp_busy_wait(mfd);

#ifdef NOVATEK_CABC
        /* Turn off dimming before suspend */
        if (led_pwm1[1] == 0) {
                atomic_set(&lcd_backlight_off, 1);
                mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_dim_off_cmds, ARRAY_SIZE(novatek_dim_off_cmds));
        } else
                atomic_set(&lcd_backlight_off, 0);
#endif
	mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_cmd_backlight_cmds,
			ARRAY_SIZE(novatek_cmd_backlight_cmds));
	mutex_unlock(&mfd->dma->ov_mutex);

///HTC:
#ifdef CONFIG_BACKLIGHT_WLED_CABC
	/* For WLED CABC, To switch on/off WLED module */
	if (wled_trigger_initialized) {
		led_trigger_event(bkl_led_trigger, mfd->bl_level);
	}
#endif
///:HTC
	return;
}

#ifdef NOVATEK_CABC
static void mipi_novatek_dim_on(struct msm_fb_data_type *mfd)
{
	if (atomic_read(&lcd_backlight_off)) {
		PR_DISP_DEBUG("%s: backlight is off. Skip dimming setting\n", __func__);
		return;
	}

	mutex_lock(&mfd->dma->ov_mutex);
	PR_DISP_DEBUG("%s\n",  __FUNCTION__);

	/* mdp4_dsi_cmd_busy_wait: If we skip this, the screen will be corrupted after sending commands */
	mdp4_dsi_cmd_dma_busy_wait(mfd);
	mipi_dsi_mdp_busy_wait(mfd);

	mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_dim_on_cmds, ARRAY_SIZE(novatek_dim_on_cmds));

	mutex_unlock(&mfd->dma->ov_mutex);
}

#ifdef CONFIG_MSM_ACL_ENABLE
static void mipi_novatek_cabc_on(int on, struct msm_fb_data_type *mfd)
{
	static int first_time = 1;

	mutex_lock(&mfd->dma->ov_mutex);
	PR_DISP_DEBUG("%s: ON=%d\n",  __FUNCTION__, on);

	/* mdp4_dsi_cmd_busy_wait: If we skip this, the screen will be corrupted after sending commands */
	mdp4_dsi_cmd_dma_busy_wait(mfd);
	mipi_dsi_mdp_busy_wait(mfd);

	if (on == 8 && !first_time) {
		mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_cabc_off_cmds, ARRAY_SIZE(novatek_cabc_off_cmds));
	} else {
		mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_cabc_on_cmds, ARRAY_SIZE(novatek_cabc_on_cmds));
	}
	first_time = 0;

	mutex_unlock(&mfd->dma->ov_mutex);
}
#endif

#ifdef CONFIG_MSM_CABC_VIDEO_ENHANCE
static void mipi_novatek_set_cabc(struct msm_fb_data_type *mfd, int mode)
{
	mutex_lock(&mfd->dma->ov_mutex);
	PR_DISP_DEBUG("%s: mode=%d\n",  __FUNCTION__, mode);

	/* mdp4_dsi_cmd_busy_wait: If we skip this, the screen will be corrupted after sending commands */
	mdp4_dsi_cmd_dma_busy_wait(mfd);
	mipi_dsi_mdp_busy_wait(mfd);

	if (mode == 0) {
	       mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_cabc_UI_cmds, ARRAY_SIZE(novatek_cabc_UI_cmds));
	} else {
	       mipi_dsi_cmds_tx(mfd, &novatek_tx_buf, novatek_cabc_video_cmds, ARRAY_SIZE(novatek_cabc_video_cmds));
	}

	mutex_unlock(&mfd->dma->ov_mutex);
}
#endif
#endif

static int mipi_dsi_3d_barrier_sysfs_register(struct device *dev);
static int barrier_mode;

static int __devinit mipi_novatek_lcd_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	struct platform_device *current_pdev;
	static struct mipi_dsi_phy_ctrl *phy_settings;
	static char dlane_swap;

	if (pdev->id == 0) {
		mipi_novatek_pdata = pdev->dev.platform_data;

		if (mipi_novatek_pdata
			&& mipi_novatek_pdata->phy_ctrl_settings) {
			phy_settings = (mipi_novatek_pdata->phy_ctrl_settings);
		}

		if (mipi_novatek_pdata
			&& mipi_novatek_pdata->dlane_swap) {
			dlane_swap = (mipi_novatek_pdata->dlane_swap);
		}

		if (mipi_novatek_pdata
			 && mipi_novatek_pdata->fpga_3d_config_addr)
			mipi_novatek_3d_init(mipi_novatek_pdata
	->fpga_3d_config_addr, mipi_novatek_pdata->fpga_ctrl_mode);

		/* create sysfs to control 3D barrier for the Sharp panel */
		if (mipi_dsi_3d_barrier_sysfs_register(&pdev->dev)) {
			pr_err("%s: Failed to register 3d Barrier sysfs\n",
						__func__);
			return -ENODEV;
		}
		barrier_mode = 0;

		return 0;
	}

	current_pdev = msm_fb_add_device(pdev);

	if (current_pdev) {
		mfd = platform_get_drvdata(current_pdev);
		if (!mfd)
			return -ENODEV;
		if (mfd->key != MFD_KEY)
			return -EINVAL;

		mipi  = &mfd->panel_info.mipi;

		if (phy_settings != NULL)
			mipi->dsi_phy_db = phy_settings;

		if (dlane_swap)
			mipi->dlane_swap = dlane_swap;
	}
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_novatek_lcd_probe,
	.driver = {
		.name   = "mipi_novatek",
	},
};

static struct msm_fb_panel_data novatek_panel_data = {
	.on		= mipi_novatek_lcd_on,
	.off		= mipi_novatek_lcd_off,
	.set_backlight	= mipi_novatek_set_backlight,
#ifdef NOVATEK_CABC
	.dimming_on	= mipi_novatek_dim_on,
#ifdef CONFIG_MSM_ACL_ENABLE
	.acl_enable	= mipi_novatek_cabc_on,
#endif
#ifdef CONFIG_MSM_CABC_VIDEO_ENHANCE
	.set_cabc	= mipi_novatek_set_cabc,
#endif
#endif
};

static ssize_t mipi_dsi_3d_barrier_read(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf((char *)buf, sizeof(buf), "%u\n", barrier_mode);
}

static ssize_t mipi_dsi_3d_barrier_write(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	int ret = -1;
	u32 data = 0;

	if (sscanf((char *)buf, "%u", &data) != 1) {
		dev_err(dev, "%s\n", __func__);
		ret = -EINVAL;
	} else {
		barrier_mode = data;
		if (data == 1)
			mipi_dsi_enable_3d_barrier(LANDSCAPE);
		else if (data == 2)
			mipi_dsi_enable_3d_barrier(PORTRAIT);
		else
			mipi_dsi_enable_3d_barrier(0);
	}

	return count;
}

static struct device_attribute mipi_dsi_3d_barrier_attributes[] = {
	__ATTR(enable_3d_barrier, 0664, mipi_dsi_3d_barrier_read,
					 mipi_dsi_3d_barrier_write),
};

static int mipi_dsi_3d_barrier_sysfs_register(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mipi_dsi_3d_barrier_attributes); i++)
		if (device_create_file(dev, mipi_dsi_3d_barrier_attributes + i))
			goto error;

	return 0;

error:
	for (; i >= 0 ; i--)
		device_remove_file(dev, mipi_dsi_3d_barrier_attributes + i);
	pr_err("%s: Unable to create interface\n", __func__);

	return -ENODEV;
}

static int ch_used[3];

int mipi_novatek_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_novatek_lcd_init();
	if (ret) {
		pr_err("mipi_novatek_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_novatek", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	novatek_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &novatek_panel_data,
		sizeof(novatek_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_novatek_lcd_init(void)
{
    if(panel_type == PANEL_ID_NONE || board_mfg_mode() == 5) {
        PR_DISP_INFO("%s panel ID = PANEL_ID_NONE\n", __func__);
        return 0;
    }
#ifdef CONFIG_SPI_QUP
	int ret;
	ret = spi_register_driver(&panel_3d_spi_driver);

	if (ret) {
		pr_err("%s: spi register failed: rc=%d\n", __func__, ret);
		platform_driver_unregister(&this_driver);
	} else
		pr_info("%s: SUCCESS (SPI)\n", __func__);
#endif

	led_trigger_register_simple("bkl_trigger", &bkl_led_trigger);
	pr_info("%s: SUCCESS (WLED TRIGGER)\n", __func__);
	wled_trigger_initialized = 1;
	atomic_set(&lcd_power_state, 1);

	mipi_dsi_buf_alloc(&novatek_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&novatek_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
