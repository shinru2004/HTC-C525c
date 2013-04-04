/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_novatek.h"

static struct msm_panel_info pinfo;

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
       /* DSI_BIT_CLK at 604MHz, 2 lane, RGB888 */
       /* regulator *//* off=0x0500 */
       {0x09, 0x08, 0x05, 0x00, 0x20},
       /* timing *//* off=0x0440 */
       {0xA9, 0x18, 0x1A, 0x00, 0x34, 0x38, 0x14,
       0x1B, 0x15, 0x03, 0x04, 0x00},
       /* phy ctrl *//* off=0x0470 */
       {0x5F, 0x00, 0x00, 0x10},
       /* strength *//* off=0x0480 */
       {0xFF, 0x00, 0x06, 0x00},
       /* pll control *//* off=0x0200 */
       {0x00, 0x37, 0x30, 0xC4, 0x00, 0x20, 0x0A, 0x62,
       0x71, 0x88, 0x99,
       0x00, 0x14, 0x03, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01 },
};

static int __init mipi_video_novatek_wvga_pt_init(void)
{
	int ret;

	if (msm_fb_detect_client("mipi_video_novatek_wvga"))
		return 0;

	pinfo.xres = 480;
	pinfo.yres = 800;
	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.width = 56;
	pinfo.height = 93;
	pinfo.camera_backlight = 200;

	/* The sum of H porch MUST be even number
	   to avoid incorrect MIPI packet start position issue */
#if defined (CONFIG_MACH_K2_CL)
    pinfo.lcdc.h_back_porch = 14;
    pinfo.lcdc.h_front_porch = 16;
    pinfo.lcdc.h_pulse_width = 4;
    pinfo.lcdc.v_back_porch = 16;
    pinfo.lcdc.v_front_porch = 20;
    pinfo.lcdc.v_pulse_width = 4;
    pinfo.clk_rate = 312000000;
#else
    pinfo.lcdc.h_back_porch = 10;
    pinfo.lcdc.h_front_porch = 14;
    pinfo.lcdc.h_pulse_width = 4;
    pinfo.lcdc.v_back_porch = 10;
    pinfo.lcdc.v_front_porch = 8;
    pinfo.lcdc.v_pulse_width = 2;
    pinfo.clk_rate = 300000000;
#endif

	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = TRUE;
	pinfo.mipi.hfp_power_stop = FALSE;
	pinfo.mipi.hbp_power_stop = FALSE;
	pinfo.mipi.hsa_power_stop = FALSE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_NON_BURST_SYNCH_PULSE;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_BGR;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.esc_byte_ratio = 3;

	pinfo.mipi.tx_eot_append = TRUE;
	pinfo.mipi.t_clk_post = 0x04;
	pinfo.mipi.t_clk_pre = 0x1c;
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;

	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;

	ret = mipi_novatek_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_WVGA_PT);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_novatek_wvga_pt_init);
