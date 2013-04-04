/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/debug_display.h>

#include "mdp.h"
#include "msm_fb.h"
#include "mdp4.h"
#include "mipi_dsi.h"

static struct mdp4_overlay_pipe *dsi_pipe;
static struct msm_fb_data_type *dsi_mfd;
static int dsi_state;
static unsigned long  tout_expired;

#define TOUT_PERIOD	HZ	/* 1 second */
#define MS_100		(HZ/10)	/* 100 ms */

static int vsync_start_y_adjust = 4;

struct timer_list dsi_clock_timer;

void mdp4_overlay_dsi_state_set(int state)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	dsi_state = state;
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
}

int mdp4_overlay_dsi_state_get(void)
{
	return dsi_state;
}

static void dsi_clock_tout(unsigned long data)
{
	spin_lock(&dsi_clk_lock);
	if (mipi_dsi_clk_on) {
		if (dsi_state == ST_DSI_PLAYING) {
			mipi_dsi_turn_off_clks();
			mdp4_overlay_dsi_state_set(ST_DSI_CLK_OFF);
		}
	}
	spin_unlock(&dsi_clk_lock);
}

static __u32 msm_fb_line_length(__u32 fb_index, __u32 xres, int bpp)
{
	/*
	 * The adreno GPU hardware requires that the pitch be aligned to
	 * 32 pixels for color buffers, so for the cases where the GPU
	 * is writing directly to fb0, the framebuffer pitch
	 * also needs to be 32 pixel aligned
	 */

	if (fb_index == 0)
		return ALIGN(xres, 32) * bpp;
	else
		return xres * bpp;
}

void mdp4_dsi_cmd_del_timer(void)
{
	del_timer_sync(&dsi_clock_timer);
}

void mdp4_mipi_vsync_enable(struct msm_fb_data_type *mfd,
		struct mdp4_overlay_pipe *pipe, int which)
{
	uint32 start_y, data, tear_en;

	tear_en = (1 << which);

	if ((mfd->use_mdp_vsync) && (mfd->ibuf.vsync_enable) &&
		(mfd->panel_info.lcd.vsync_enable)) {

		if (vsync_start_y_adjust <= pipe->dst_y)
			start_y = pipe->dst_y - vsync_start_y_adjust;
		else
			start_y = (mfd->total_lcd_lines - 1) -
				(vsync_start_y_adjust - pipe->dst_y);
		if (which == 0)
			MDP_OUTP(MDP_BASE + 0x210, start_y);	/* primary */
		else
			MDP_OUTP(MDP_BASE + 0x214, start_y);	/* secondary */

		data = inpdw(MDP_BASE + 0x20c);
		data |= tear_en;
		MDP_OUTP(MDP_BASE + 0x20c, data);
	} else {
		data = inpdw(MDP_BASE + 0x20c);
		data &= ~tear_en;
		MDP_OUTP(MDP_BASE + 0x20c, data);
	}
}

void mdp4_overlay_update_dsi_cmd(struct msm_fb_data_type *mfd)
{
	MDPIBUF *iBuf = &mfd->ibuf;
	uint8 *src;
	int ptype;
	struct mdp4_overlay_pipe *pipe;
	struct msm_fb_data_type *tmp_dsi_mfd;
	int bpp;
	int ret;

	if (mfd->key != MFD_KEY)
		return;

	tmp_dsi_mfd = dsi_mfd;
	dsi_mfd = mfd;		/* keep it */

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	if (unlikely(dsi_pipe == NULL)) {
		ptype = mdp4_overlay_format2type(mfd->fb_imgType);
		if (unlikely(ptype < 0)) {
			printk(KERN_ERR "%s: format2type failed\n", __func__);
			mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
			dsi_mfd = tmp_dsi_mfd;
			return;
		}
		pipe = mdp4_overlay_pipe_alloc(ptype, MDP4_MIXER0);
		if (unlikely(pipe == NULL)) {
			printk(KERN_ERR "%s: pipe_alloc failed\n", __func__);
			mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
			dsi_mfd = tmp_dsi_mfd;
			return;
		}
		pipe->pipe_used++;
		pipe->mixer_stage  = MDP4_MIXER_STAGE_BASE;
		pipe->mixer_num  = MDP4_MIXER0;
		pipe->src_format = mfd->fb_imgType;
		mdp4_overlay_panel_mode(pipe->mixer_num, MDP4_PANEL_DSI_CMD);
		ret = mdp4_overlay_format2pipe(pipe);
		if (ret < 0)
			printk(KERN_INFO "%s: format2type failed\n", __func__);

		init_timer(&dsi_clock_timer);
		dsi_clock_timer.function = dsi_clock_tout;
		dsi_clock_timer.data = (unsigned long) mfd;;
		dsi_clock_timer.expires = 0xffffffff;
		add_timer(&dsi_clock_timer);
		tout_expired = jiffies;

		dsi_pipe = pipe; /* keep it */

		mdp4_init_writeback_buf(mfd, MDP4_MIXER0);
		pipe->ov_blt_addr = 0;
		pipe->dma_blt_addr = 0;

	} else {
		pipe = dsi_pipe;
	}
	/*
	 * configure dsi stream id
	 * dma_p = 0, dma_s = 1
	 */
	MDP_OUTP(MDP_BASE + 0x000a0, 0x10);
	/* disable dsi trigger */
	MDP_OUTP(MDP_BASE + 0x000a4, 0x00);
	/* whole screen for base layer */
	src = (uint8 *) iBuf->buf;


	{
		struct fb_info *fbi;

		fbi = mfd->fbi;
		if (pipe->is_3d) {
			bpp = fbi->var.bits_per_pixel / 8;
			pipe->src_height = pipe->src_height_3d;
			pipe->src_width = pipe->src_width_3d;
			pipe->src_h = pipe->src_height_3d;
			pipe->src_w = pipe->src_width_3d;
			pipe->dst_h = pipe->src_height_3d;
			pipe->dst_w = pipe->src_width_3d;
			pipe->srcp0_ystride = msm_fb_line_length(0,
						pipe->src_width, bpp);
		} else {
			 /* 2D */
			pipe->src_height = fbi->var.yres;
			pipe->src_width = fbi->var.xres;
			pipe->src_h = fbi->var.yres;
			pipe->src_w = fbi->var.xres;
			pipe->dst_h = fbi->var.yres;
			pipe->dst_w = fbi->var.xres;
			pipe->srcp0_ystride = fbi->fix.line_length;
		}
		pipe->src_y = 0;
		pipe->src_x = 0;
		pipe->dst_y = 0;
		pipe->dst_x = 0;
		pipe->srcp0_addr = (uint32)src;
	}


	mdp4_overlay_rgb_setup(pipe);

	mdp4_mixer_stage_up(pipe);

	mdp4_overlayproc_cfg(pipe);

	mdp4_overlay_dmap_xy(pipe);

	mdp4_overlay_dmap_cfg(mfd, 0);

	mdp4_mipi_vsync_enable(mfd, pipe, 0);

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	wmb();
}

/* 3D side by side */
void mdp4_dsi_cmd_3d_sbys(struct msm_fb_data_type *mfd,
				struct msmfb_overlay_3d *r3d)
{
	struct fb_info *fbi;
	struct mdp4_overlay_pipe *pipe;
	int bpp;
	uint8 *src = NULL;

	if (dsi_pipe == NULL)
		return;

	dsi_pipe->is_3d = r3d->is_3d;
	dsi_pipe->src_height_3d = r3d->height;
	dsi_pipe->src_width_3d = r3d->width;

	pipe = dsi_pipe;

	if (pipe->is_3d)
		mdp4_overlay_panel_3d(pipe->mixer_num, MDP4_3D_SIDE_BY_SIDE);
	else
		mdp4_overlay_panel_3d(pipe->mixer_num, MDP4_3D_NONE);

	if (mfd->panel_power_on)
		mdp4_dsi_cmd_dma_busy_wait(mfd);

	fbi = mfd->fbi;
	if (pipe->is_3d) {
		bpp = fbi->var.bits_per_pixel / 8;
		pipe->src_height = pipe->src_height_3d;
		pipe->src_width = pipe->src_width_3d;
		pipe->src_h = pipe->src_height_3d;
		pipe->src_w = pipe->src_width_3d;
		pipe->dst_h = pipe->src_height_3d;
		pipe->dst_w = pipe->src_width_3d;
		pipe->srcp0_ystride = msm_fb_line_length(0,
					pipe->src_width, bpp);
	} else {
		 /* 2D */
		pipe->src_height = fbi->var.yres;
		pipe->src_width = fbi->var.xres;
		pipe->src_h = fbi->var.yres;
		pipe->src_w = fbi->var.xres;
		pipe->dst_h = fbi->var.yres;
		pipe->dst_w = fbi->var.xres;
		pipe->srcp0_ystride = fbi->fix.line_length;
	}
	pipe->src_y = 0;
	pipe->src_x = 0;
	pipe->dst_y = 0;
	pipe->dst_x = 0;
	pipe->srcp0_addr = (uint32)src;

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	mdp4_overlay_rgb_setup(pipe);

	mdp4_mixer_stage_up(pipe);

	mdp4_overlayproc_cfg(pipe);

	mdp4_overlay_dmap_xy(pipe);

	mdp4_overlay_dmap_cfg(mfd, 0);

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

int mdp4_dsi_overlay_blt_start(struct msm_fb_data_type *mfd)
{
	unsigned long flag;

	pr_debug("%s: blt_end=%d ov_blt_addr=%x pid=%d\n",
	__func__, dsi_pipe->blt_end, (int)dsi_pipe->ov_blt_addr, current->pid);

	mdp4_allocate_writeback_buf(mfd, MDP4_MIXER0);

	if (mfd->ov0_wb_buf->write_addr == 0) {
		pr_info("%s: no blt_base assigned\n", __func__);
		return -EBUSY;
	}

	if (dsi_pipe->ov_blt_addr == 0) {
		mdp4_dsi_cmd_dma_busy_wait(mfd);
		spin_lock_irqsave(&mdp_spin_lock, flag);
		dsi_pipe->blt_end = 0;
		dsi_pipe->blt_cnt = 0;
		dsi_pipe->ov_cnt = 0;
		dsi_pipe->dmap_cnt = 0;
		dsi_pipe->ov_blt_addr = mfd->ov0_wb_buf->write_addr;
		dsi_pipe->dma_blt_addr = mfd->ov0_wb_buf->read_addr;
		mdp4_stat.blt_dsi_cmd++;
		spin_unlock_irqrestore(&mdp_spin_lock, flag);
		return 0;
	}

	return -EBUSY;
}

int mdp4_dsi_overlay_blt_stop(struct msm_fb_data_type *mfd)
{
	unsigned long flag;


	pr_debug("%s: blt_end=%d ov_blt_addr=%x\n",
		 __func__, dsi_pipe->blt_end, (int)dsi_pipe->ov_blt_addr);

	if ((dsi_pipe->blt_end == 0) && dsi_pipe->ov_blt_addr) {
		spin_lock_irqsave(&mdp_spin_lock, flag);
		dsi_pipe->blt_end = 1;	/* mark as end */
		spin_unlock_irqrestore(&mdp_spin_lock, flag);
		return 0;
	}

	return -EBUSY;
}

int mdp4_dsi_overlay_blt_offset(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	req->offset = 0;
	req->width = dsi_pipe->src_width;
	req->height = dsi_pipe->src_height;
	req->bpp = dsi_pipe->bpp;

	return sizeof(*req);
}

void mdp4_dsi_overlay_blt(struct msm_fb_data_type *mfd,
					struct msmfb_overlay_blt *req)
{
	if (req->enable)
		mdp4_dsi_overlay_blt_start(mfd);
	else if (req->enable == 0)
		mdp4_dsi_overlay_blt_stop(mfd);

}

static void mdp4_blt_overlay_xy_update(struct mdp4_overlay_pipe *pipe)
{
	uint32 off, addr;
	int bpp;
	char *overlay_base;

	if (pipe->ov_blt_addr == 0)
		return;

#ifdef BLT_RGB565
	bpp = 2; /* overlay ouput is RGB565 */
#else
	bpp = 3; /* overlay ouput is RGB888 */
#endif
	off = (pipe->blt_cnt & 0x01) ?
		pipe->src_height * pipe->src_width * bpp : 0;
	addr = pipe->ov_blt_addr + off;
	overlay_base = MDP_BASE + MDP4_OVERLAYPROC0_BASE;/* 0x10000 */
	wmb();
	outpdw(overlay_base + 0x000c, addr);
	outpdw(overlay_base + 0x001c, addr);
	wmb();
}

static void mdp4_blt_dmap_xy_update(struct mdp4_overlay_pipe *pipe)
{
	u32 off, addr;
	int bpp;

	if (pipe->ov_blt_addr == 0)
		return;
#ifdef BLT_RGB565
	bpp = 2; /* overlay ouput is RGB565 */
#else
	bpp = 3; /* overlay ouput is RGB888 */
#endif
	off = (pipe->blt_cnt & 0x01) ?
		0 : pipe->src_height * pipe->src_width * bpp;
	addr = pipe->ov_blt_addr + off;
	wmb();
	MDP_OUTP(MDP_BASE + 0x90008, addr);
	wmb();
}
/*
 * mdp4_dma_p_done_dsi: called from isr
 * DAM_P_DONE only used when blt enabled
 */
void mdp4_dma_p_done_dsi(struct mdp_dma_data *dma)
{
	spin_lock(&mdp_spin_lock);
	dma->dmap_busy = FALSE;
	spin_unlock(&mdp_spin_lock);
	complete(&dma->dmap_comp);
	mdp_disable_irq_nosync(MDP_DMA2_TERM);

	if (dsi_pipe && dsi_pipe->ov_blt_addr) {
		spin_lock(&mdp_spin_lock);
		mdp4_blt_dmap_xy_update(dsi_pipe);
		if (dsi_pipe->blt_end) {
			dsi_pipe->blt_end = 0;
			dsi_pipe->dma_blt_addr = 0;
			dsi_pipe->ov_blt_addr = 0;
		}
		spin_unlock(&mdp_spin_lock);
	}

	mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK, MDP_BLOCK_POWER_OFF, TRUE);
}

/*
 * mdp4_overlay0_done_dsi_cmd: called from isr
 */
void mdp4_overlay0_done_dsi_cmd(struct mdp_dma_data *dma)
{
	spin_lock(&mdp_spin_lock);
	dma->busy = FALSE;
	spin_unlock(&mdp_spin_lock);
	complete(&dma->comp);

	if (dsi_pipe && dsi_pipe->ov_blt_addr) {
		spin_lock(&mdp_spin_lock);
		mdp4_blt_overlay_xy_update(dsi_pipe);
		spin_unlock(&mdp_spin_lock);
	} else
		pr_err("%s: %d should not be here when dlt is not enabled.\n",
		       __func__, __LINE__);
}

void mdp4_dsi_cmd_overlay_restore(void)
{
	/* mutex holded by caller */
	if (dsi_mfd && dsi_pipe) {
		mdp4_dsi_cmd_dma_busy_wait(dsi_mfd);
		mipi_dsi_mdp_busy_wait(dsi_mfd);
		mdp4_overlay_update_dsi_cmd(dsi_mfd);
		mdp4_dsi_cmd_overlay_kickoff(dsi_mfd, dsi_pipe);
	}
}

static void mdp4_dump_status(void)
{
	PR_DISP_INFO("MDP_DISPLAY_STATUS:%x\n", inpdw(msm_mdp_base + 0x18));
	PR_DISP_INFO("MDP_INTR_STATUS:%x\n", inpdw(MDP_INTR_STATUS));
	PR_DISP_INFO("MDP_INTR_ENABLE:%x\n", inpdw(MDP_INTR_ENABLE));
	PR_DISP_INFO("MDP_LAYERMIXER_IN_CFG:%x\n", inpdw(msm_mdp_base + 0x10100));
	PR_DISP_INFO("MDP_OVERLAY_STATUS:%x\n", inpdw(msm_mdp_base + 0x10000));
	PR_DISP_INFO("MDP_OVERLAYPROC0_CFG:%x\n", inpdw(msm_mdp_base + 0x10004));
	PR_DISP_INFO("MDP_OVERLAYPROC1_CFG:%x\n", inpdw(msm_mdp_base + 0x18004));
	PR_DISP_INFO("RGB1(1):%x %x %x %x\n", inpdw(msm_mdp_base + 0x40000),
		inpdw(msm_mdp_base + 0x40004), inpdw(msm_mdp_base + 0x40008),
		inpdw(msm_mdp_base + 0x4000C));
	PR_DISP_INFO("RGB1(2):%x %x %x %x\n", inpdw(msm_mdp_base + 0x40010),
		inpdw(msm_mdp_base + 0x40014), inpdw(msm_mdp_base + 0x40018),
		inpdw(msm_mdp_base + 0x40040));
	PR_DISP_INFO("RGB1(3):%x %x %x %x\n", inpdw(msm_mdp_base + 0x40050),
		inpdw(msm_mdp_base + 0x40054), inpdw(msm_mdp_base + 0x40058),
		inpdw(msm_mdp_base + 0x4005C));
	PR_DISP_INFO("RGB1(4):%x %x %x %x\n", inpdw(msm_mdp_base + 0x40060),
		inpdw(msm_mdp_base + 0x41000), inpdw(msm_mdp_base + 0x41004),
		inpdw(msm_mdp_base + 0x41008));
	PR_DISP_INFO("RGB2(1):%x %x %x %x\n", inpdw(msm_mdp_base + 0x50000),
		inpdw(msm_mdp_base + 0x50004), inpdw(msm_mdp_base + 0x50008),
		inpdw(msm_mdp_base + 0x5000C));
	PR_DISP_INFO("RGB2(2):%x %x %x %x\n", inpdw(msm_mdp_base + 0x50010),
		inpdw(msm_mdp_base + 0x50014), inpdw(msm_mdp_base + 0x50018),
		inpdw(msm_mdp_base + 0x50040));
	PR_DISP_INFO("RGB2(3):%x %x %x %x\n", inpdw(msm_mdp_base + 0x50050),
		inpdw(msm_mdp_base + 0x50054), inpdw(msm_mdp_base + 0x50058),
		inpdw(msm_mdp_base + 0x5005C));
	PR_DISP_INFO("RGB2(4):%x %x %x %x\n", inpdw(msm_mdp_base + 0x50060),
		inpdw(msm_mdp_base + 0x51000), inpdw(msm_mdp_base + 0x51004),
		inpdw(msm_mdp_base + 0x51008));
	PR_DISP_INFO("VG1 (1):%x %x %x %x\n", inpdw(msm_mdp_base + 0x20000),
		inpdw(msm_mdp_base + 0x20004), inpdw(msm_mdp_base + 0x20008),
		inpdw(msm_mdp_base + 0x2000C));
	PR_DISP_INFO("VG1 (2):%x %x %x %x\n", inpdw(msm_mdp_base + 0x20010),
		inpdw(msm_mdp_base + 0x20014), inpdw(msm_mdp_base + 0x20018),
		inpdw(msm_mdp_base + 0x20040));
	PR_DISP_INFO("VG1 (3):%x %x %x %x\n", inpdw(msm_mdp_base + 0x20050),
		inpdw(msm_mdp_base + 0x20054), inpdw(msm_mdp_base + 0x20058),
		inpdw(msm_mdp_base + 0x2005C));
	PR_DISP_INFO("VG1 (4):%x %x %x %x\n", inpdw(msm_mdp_base + 0x20060),
		inpdw(msm_mdp_base + 0x21000), inpdw(msm_mdp_base + 0x21004),
		inpdw(msm_mdp_base + 0x21008));
	PR_DISP_INFO("VG2 (1):%x %x %x %x\n", inpdw(msm_mdp_base + 0x30000),
		inpdw(msm_mdp_base + 0x30004), inpdw(msm_mdp_base + 0x30008),
		inpdw(msm_mdp_base + 0x3000C));
	PR_DISP_INFO("VG2 (2):%x %x %x %x\n", inpdw(msm_mdp_base + 0x30010),
		inpdw(msm_mdp_base + 0x30014), inpdw(msm_mdp_base + 0x30018),
		inpdw(msm_mdp_base + 0x30040));
	PR_DISP_INFO("VG2 (3):%x %x %x %x\n",
		inpdw(msm_mdp_base + 0x30050), inpdw(msm_mdp_base + 0x30054),
		inpdw(msm_mdp_base + 0x30058), inpdw(msm_mdp_base + 0x3005C));
	PR_DISP_INFO("VG2 (4):%x %x %x %x\n",
		inpdw(msm_mdp_base + 0x30060), inpdw(msm_mdp_base + 0x31000),
		inpdw(msm_mdp_base + 0x31004), inpdw(msm_mdp_base + 0x31008));
}

static void mdp4_dsi_overlay_busy_wait(struct msm_fb_data_type *mfd)
{
	unsigned long flag;
	int need_wait = 0;
#if 1 /* HTC_CSP_START */
	int retry_count = 0;
	long timeout;
#endif /* HTC_CSP_END */

	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (mfd->dma->busy == TRUE) {
		INIT_COMPLETION(mfd->dma->comp);
		need_wait++;
	}
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
	if (need_wait) {
#if 1 /* HTC_CSP_START */
		mdp4_stat.busywait1++;
		timeout = wait_for_completion_timeout(&mfd->dma->comp, HZ/5);
		mdp4_stat.busywait1--;
		while (!timeout && retry_count++ < 15) {
			rmb();
			if (mfd->dma->busy == FALSE) {
				PR_DISP_INFO("%s(%d)timeout but dma not busy now\n", __func__, __LINE__);
				break;
			} else {
				PR_DISP_INFO("%s(%d)timeout but dma still busy\n", __func__, __LINE__);
				PR_DISP_INFO("### need_wait:%d pending pid=%d dsi_clk_on=%d\n", need_wait, current->pid, mipi_dsi_clk_on);

				mdp4_dump_status();

				mdp4_stat.busywait1++;
				timeout = wait_for_completion_timeout(&mfd->dma->comp, HZ/5);
				mdp4_stat.busywait1--;
			}
		}

		if (retry_count >= 15) {
			PR_DISP_INFO("###mdp busy wait retry timed out, mfd->dma->busy:%d\n",  mfd->dma->busy);
			spin_lock_irqsave(&mdp_spin_lock, flag);
			mfd->dma->busy = FALSE;
			spin_unlock_irqrestore(&mdp_spin_lock, flag);
		}
#else /* HTC_CSP_END */
		mdp4_stat.busywait1++;
		wait_for_completion(&mfd->dma->comp);
		mdp4_stat.busywait1--;
#endif
	}
}

static void mdp4_dsi_dmap_busy_wait(struct msm_fb_data_type *mfd)
{
	unsigned long flag;
	int need_wait = 0;
	int timeout;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (mfd->dma->dmap_busy == TRUE) {
		INIT_COMPLETION(mfd->dma->dmap_comp);
		need_wait++;
	}
	spin_unlock_irqrestore(&mdp_spin_lock, flag);

	if (need_wait) {
		mdp4_stat.busywait2++;
		timeout = wait_for_completion_timeout(&mfd->dma->dmap_comp, HZ);
		mdp4_stat.busywait2--;

		if (!timeout) {
			rmb();
			if (mfd->dma->dmap_busy == FALSE) {
				PR_DISP_INFO("%s(%d)timeout but dma not busy now\n", __func__, __LINE__);
			} else {
				PR_DISP_INFO("%s(%d)timeout but dma still busy\n", __func__, __LINE__);
				PR_DISP_INFO("### need_wait:%d pending pid=%d dsi_clk_on=%d\n", need_wait, current->pid, mipi_dsi_clk_on);

				mdp4_dump_status();

				spin_lock_irqsave(&mdp_spin_lock, flag);
				mfd->dma->dmap_busy = FALSE;
				spin_unlock_irqrestore(&mdp_spin_lock, flag);
			}
			mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK, MDP_BLOCK_POWER_OFF, TRUE);
		}
	}
}

/*
 * mdp4_dsi_cmd_dma_busy_wait: check dsi link activity
 * dsi link is a shared resource and it can only be used
 * while it is in idle state.
 * ov_mutex need to be acquired before call this function.
 */
void mdp4_dsi_cmd_dma_busy_wait(struct msm_fb_data_type *mfd)
{
	if (dsi_clock_timer.function) {
		if (time_after(jiffies, tout_expired)) {
			tout_expired = jiffies + TOUT_PERIOD;
			mod_timer(&dsi_clock_timer, tout_expired);
			tout_expired -= MS_100;
		}
	}

	pr_debug("%s: start pid=%d dsi_clk_on=%d\n",
			__func__, current->pid, mipi_dsi_clk_on);

	/* satrt dsi clock if necessary */
	spin_lock_bh(&dsi_clk_lock);
	if (mipi_dsi_clk_on == 0)
		mipi_dsi_turn_on_clks();
	spin_unlock_bh(&dsi_clk_lock);

	if (dsi_pipe && dsi_pipe->ov_blt_addr)
		mdp4_dsi_overlay_busy_wait(mfd);
	mdp4_dsi_dmap_busy_wait(mfd);
}

void mdp4_dsi_cmd_kickoff_video(struct msm_fb_data_type *mfd,
				struct mdp4_overlay_pipe *pipe)
{
	/*
	 * a video kickoff may happen before UI kickoff after
	 * blt enabled. mdp4_overlay_update_dsi_cmd() need
	 * to be called before kickoff.
	 * vice versa for blt disabled.
	 */
	if (dsi_pipe->ov_blt_addr && dsi_pipe->blt_cnt == 0)
		mdp4_overlay_update_dsi_cmd(mfd); /* first time */
	else if (dsi_pipe->ov_blt_addr == 0  && dsi_pipe->blt_cnt) {
		mdp4_overlay_update_dsi_cmd(mfd); /* last time */
		dsi_pipe->blt_cnt = 0;
	}

	pr_debug("%s: ov_blt_addr=%d blt_cnt=%d\n",
		__func__, (int)dsi_pipe->ov_blt_addr, dsi_pipe->blt_cnt);

	mdp4_dsi_cmd_overlay_kickoff(mfd, pipe);
}

void mdp4_dsi_cmd_kickoff_ui(struct msm_fb_data_type *mfd,
				struct mdp4_overlay_pipe *pipe)
{

	pr_debug("%s: pid=%d\n", __func__, current->pid);
	mdp4_dsi_cmd_overlay_kickoff(mfd, pipe);
}

void mdp4_dsi_cmd_overlay_kickoff(struct msm_fb_data_type *mfd,
				struct mdp4_overlay_pipe *pipe)
{
	unsigned long flag;

	mdp4_iommu_attach();
	/* change mdp clk */
	mdp4_set_perf_level();

	mipi_dsi_mdp_busy_wait(mfd);

	mipi_dsi_cmd_mdp_start();

	mdp4_overlay_dsi_state_set(ST_DSI_PLAYING);

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	spin_lock_irqsave(&mdp_spin_lock, flag);
	outp32(MDP_INTR_CLEAR, INTR_DMA_P_DONE);
	outp32(MDP_INTR_CLEAR, INTR_OVERLAY0_DONE);
	mdp_intr_mask |= INTR_DMA_P_DONE;
	if (dsi_pipe && dsi_pipe->ov_blt_addr) {
		mdp_intr_mask |= INTR_OVERLAY0_DONE;
		mfd->dma->busy = TRUE;
	} else
		mdp_intr_mask &= ~INTR_OVERLAY0_DONE;
	mfd->dma->dmap_busy = TRUE;
	outp32(MDP_INTR_ENABLE, mdp_intr_mask);
	mdp_enable_irq(MDP_DMA2_TERM);
	wmb();
	spin_unlock_irqrestore(&mdp_spin_lock, flag);

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	mdp_pipe_kickoff(MDP_OVERLAY0_TERM, mfd);
	wmb();
	mdp4_stat.kickoff_ov0++;
	if (dsi_pipe)
		dsi_pipe->ov_cnt++;
	if (dsi_pipe && dsi_pipe->ov_blt_addr) {
		wmb();
		outpdw(MDP_BASE + 0x000c, 0x0);
		mdp4_stat.kickoff_dmap++;
		wmb();
		dsi_pipe->dmap_cnt++;
		dsi_pipe->blt_cnt++;
	}
}

void mdp_dsi_cmd_overlay_suspend(void)
{
	/* dis-engage rgb0 from mixer0 */
	if (dsi_pipe) {
		mdp4_mixer_stage_down(dsi_pipe);
		mdp4_iommu_unmap(dsi_pipe);
	}
}

void mdp4_dsi_cmd_overlay(struct msm_fb_data_type *mfd)
{
	if (!mfd)
		return;

	mutex_lock(&mfd->dma->ov_mutex);

	if (mfd->panel_power_on) {
		mdp4_dsi_cmd_dma_busy_wait(mfd);

		mdp4_overlay_update_dsi_cmd(mfd);

		mdp4_dsi_cmd_kickoff_ui(mfd, dsi_pipe);
		mdp4_iommu_unmap(dsi_pipe);
	/* signal if pan function is waiting for the update completion */
		if (mfd->pan_waiting) {
			mfd->pan_waiting = FALSE;
			complete(&mfd->pan_comp);
		}
	}
	mutex_unlock(&mfd->dma->ov_mutex);
}
