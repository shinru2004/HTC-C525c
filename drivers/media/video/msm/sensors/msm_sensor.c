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
 */

#include "msm_sensor.h"
#include "msm.h"
#include "msm_ispif.h"
#include "msm_camera_i2c_mux.h"

#ifdef CONFIG_RAWCHIP
#include "rawchip/rawchip.h"
#endif
static int first_init;
/*=============================================================*/
int32_t msm_sensor_adjust_frame_lines(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	uint16_t cur_line = 0;
	uint16_t exp_fl_lines = 0;
	CDBG("%s: called\n", __func__);

	if (s_ctrl->sensor_exp_gain_info) {
		if (s_ctrl->prev_gain && s_ctrl->prev_line &&
			s_ctrl->func_tbl->sensor_write_exp_gain)
			s_ctrl->func_tbl->sensor_write_exp_gain(
				s_ctrl,
				s_ctrl->prev_gain,
				s_ctrl->prev_line);

		msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			&cur_line,
			MSM_CAMERA_I2C_WORD_DATA);
		exp_fl_lines = cur_line +
			s_ctrl->sensor_exp_gain_info->vert_offset;
		if (exp_fl_lines > s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines)
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_output_reg_addr->
				frame_length_lines,
				exp_fl_lines,
				MSM_CAMERA_I2C_WORD_DATA);
		CDBG("%s cur_fl_lines %d, exp_fl_lines %d\n", __func__,
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines,
			exp_fl_lines);
	}
	return 0;
}

int32_t msm_sensor_write_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc;
	CDBG("%s: called\n", __func__);

	rc = msm_sensor_write_all_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->init_settings,
		s_ctrl->msm_sensor_reg->init_size);
	return rc;
}

int32_t msm_sensor_write_res_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int32_t rc;
	CDBG("%s: called\n", __func__);

	rc = msm_sensor_write_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->mode_settings, res);
	if (rc < 0) {
		pr_info("%s: failed to msm_sensor_write_conf_array return < 0\n", __func__);
		return rc;
	}

	rc = msm_sensor_write_output_settings(s_ctrl, res);
	if (rc < 0) {
		pr_info("%s: failed to msm_sensor_write_output_settings return < 0\n", __func__);
		return rc;
	}

	if (s_ctrl->func_tbl->sensor_adjust_frame_lines)
		rc = s_ctrl->func_tbl->sensor_adjust_frame_lines(s_ctrl, res);

	return rc;
}

int32_t msm_sensor_write_output_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int32_t rc = -EFAULT;
	struct msm_camera_i2c_reg_conf dim_settings[] = {
		{s_ctrl->sensor_output_reg_addr->x_output,
			s_ctrl->msm_sensor_reg->
			output_settings[res].x_output},
		{s_ctrl->sensor_output_reg_addr->y_output,
			s_ctrl->msm_sensor_reg->
			output_settings[res].y_output},
		{s_ctrl->sensor_output_reg_addr->line_length_pclk,
			s_ctrl->msm_sensor_reg->
			output_settings[res].line_length_pclk},
		{s_ctrl->sensor_output_reg_addr->frame_length_lines,
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines},
	};
	CDBG("%s: called\n", __func__);

	rc = msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client, dim_settings,
		ARRAY_SIZE(dim_settings), MSM_CAMERA_I2C_WORD_DATA);
	return rc;
}

void msm_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	pr_info("[CAM] %s\n", __func__);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->start_stream_conf,
		s_ctrl->msm_sensor_reg->start_stream_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);

	/* HTC_START - Rawchip debug */
#ifdef RAWCHIP_DEBUG_FRAME
	msleep(1);
	frame_counter();
	/* HTC_END */
#endif

}

void msm_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	pr_info("[CAM] %s\n", __func__);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->stop_stream_conf,
		s_ctrl->msm_sensor_reg->stop_stream_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

void msm_sensor_group_hold_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("%s: called\n", __func__);

	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->group_hold_on_conf,
		s_ctrl->msm_sensor_reg->group_hold_on_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

void msm_sensor_group_hold_off(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("%s: called\n", __func__);

	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->group_hold_off_conf,
		s_ctrl->msm_sensor_reg->group_hold_off_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

int32_t msm_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
						struct fps_cfg *fps)
{
	s_ctrl->fps_divider = fps->fps_div;

	return 0;
}

int32_t msm_sensor_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t offset;
	CDBG("%s: called\n", __func__);

	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_write_exp_gain2(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, ll_pclk, ll_ratio;
	uint8_t offset;
	CDBG("%s: called\n", __func__);

	fl_lines = s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider / Q10;
	ll_pclk = s_ctrl->curr_line_length_pclk;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset)) {
		ll_ratio = (line * Q10) / (fl_lines - offset);
		ll_pclk = ll_pclk * ll_ratio / Q10;
		line = fl_lines - offset;
	}

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk, ll_pclk,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_setting3(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;
	CDBG("%s: called\n", __func__);

	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		csi_config = 0;
		msm_camera_i2c_write(
			s_ctrl->sensor_i2c_client,
			0x0e, 0x08,
			MSM_CAMERA_I2C_BYTE_DATA);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		if (res == 0)
			return 0;
		if (!csi_config) {
			msm_sensor_write_conf_array(
				s_ctrl->sensor_i2c_client,
				s_ctrl->msm_sensor_reg->mode_settings, res);
			msleep(30);
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIC_CFG,
				s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(30);
			msm_camera_i2c_write(
					s_ctrl->sensor_i2c_client,
					0x0e, 0x00,
					MSM_CAMERA_I2C_BYTE_DATA);
			csi_config = 1;
		}
		msleep(50);
	}
	return rc;
}

int32_t msm_sensor_write_exp_gain1_ex(struct msm_sensor_ctrl_t *s_ctrl,
		int mode, uint16_t gain, uint16_t dig_gain, uint32_t line) /* HTC Angie 20111019 - Fix FPS */
{
	uint32_t fl_lines;
	uint8_t offset;

/* HTC_START Angie 20111019 - Fix FPS */
	uint32_t fps_divider = Q10;
	CDBG("%s: called\n", __func__);

	if (mode == SENSOR_PREVIEW_MODE)
		fps_divider = s_ctrl->fps_divider;

/* HTC_START ben 20120229 */
	if(line > s_ctrl->sensor_exp_gain_info->sensor_max_linecount)
		line = s_ctrl->sensor_exp_gain_info->sensor_max_linecount;
/* HTC_END */

	fl_lines = s_ctrl->curr_frame_length_lines;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line * Q10 > (fl_lines - offset) * fps_divider)
		fl_lines = line + offset;
	else
		fl_lines = (fl_lines * fps_divider) / Q10;
/* HTC_END */

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	/* HTC_START pg digi gain 20100710 */
	if (s_ctrl->func_tbl->sensor_set_dig_gain)
		s_ctrl->func_tbl->sensor_set_dig_gain(s_ctrl, dig_gain);
	/* HTC_END pg digi gain 20100710 */
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_write_exp_gain2_ex(struct msm_sensor_ctrl_t *s_ctrl,
		int mode, uint16_t gain, uint32_t line) /* HTC Angie 20111019 - Fix FPS */
{
	uint32_t fl_lines, ll_pclk, ll_ratio;
	uint8_t offset;
/* HTC_START Angie 20111019 - Fix FPS */
	uint32_t fps_divider = Q10;
	CDBG("%s: called\n", __func__);

	if (mode == SENSOR_PREVIEW_MODE)
		fps_divider = s_ctrl->fps_divider;
	fl_lines = s_ctrl->curr_frame_length_lines;
	ll_pclk = s_ctrl->curr_line_length_pclk;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line * Q10 > (fl_lines - offset) * fps_divider) {
		ll_ratio = (line * Q10) / (fl_lines - offset);
		ll_pclk = ll_pclk * ll_ratio / Q10;
		line = fl_lines - offset;
	} else {
		ll_pclk = ll_pclk * fps_divider / Q10;
		line = line / fps_divider;
	}
/* HTC_END */

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk, ll_pclk,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_setting1(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;
	CDBG("%s: called\n", __func__);

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		csi_config = 0;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->mode_settings, res);
		msleep(30);
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIC_CFG,
				s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(30);
			csi_config = 1;
		}
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;
}
int32_t msm_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;

#ifdef CONFIG_RAWCHIP
	struct rawchip_sensor_data rawchip_data;
	struct timespec ts_start, ts_end;
#endif

	pr_info("%s: update_type=%d, res=%d\n", __func__, update_type, res);

	v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
		NOTIFY_ISPIF_STREAM, (void *)ISPIF_STREAM(
		PIX_0, ISPIF_OFF_IMMEDIATELY));
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		first_init = 1;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		/*TODO : pointer to each sensor driver to get correct delay time*/
		if(!first_init)
			mdelay(50);
		first_init = 0;

		msm_sensor_write_res_settings(s_ctrl, res);
		if (s_ctrl->curr_csi_params != s_ctrl->csi_params[res]) {
			s_ctrl->curr_csi_params = s_ctrl->csi_params[res];
			s_ctrl->curr_csi_params->csid_params.lane_assign =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_assign;
			s_ctrl->curr_csi_params->csiphy_params.lane_mask =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_mask;
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSID_CFG,
				&s_ctrl->curr_csi_params->csid_params);
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
						NOTIFY_CID_CHANGE, NULL);
			mb();
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIPHY_CFG,
				&s_ctrl->curr_csi_params->csiphy_params);
			mb();
			msleep(20);
		}
#ifdef CONFIG_RAWCHIP
			if (s_ctrl->sensordata->use_rawchip) {
				rawchip_data.sensor_name = s_ctrl->sensordata->sensor_name;
				CDBG("[CAM] sensor_name = %s\n", rawchip_data.sensor_name);
				rawchip_data.datatype = s_ctrl->curr_csi_params->csid_params.lut_params.vc_cfg->dt;
				CDBG("[CAM] datatype = %x\n", rawchip_data.datatype);
				rawchip_data.lane_cnt = s_ctrl->curr_csi_params->csid_params.lane_cnt;
				CDBG("[CAM] lane_cnt = %d\n", rawchip_data.lane_cnt);
				rawchip_data.pixel_clk = s_ctrl->msm_sensor_reg->output_settings[res].op_pixel_clk;
				CDBG("[CAM] pixel_clk = %d\n", rawchip_data.pixel_clk);

				if (strcmp(rawchip_data.sensor_name, "ov5693") == 0)
					rawchip_data.mirror_flip = CAMERA_SENSOR_NONE;
				else
					rawchip_data.mirror_flip = s_ctrl->mirror_flip;
				CDBG("[CAM] mirror_flip = %d\n", rawchip_data.mirror_flip);
				rawchip_data.width = s_ctrl->msm_sensor_reg->output_settings[res].x_output;
				CDBG("[CAM] width = %d\n", rawchip_data.width);
				rawchip_data.height = s_ctrl->msm_sensor_reg->output_settings[res].y_output;
				CDBG("[CAM] height = %d\n", rawchip_data.height);
				rawchip_data.line_length_pclk = s_ctrl->msm_sensor_reg->output_settings[res].line_length_pclk;
				CDBG("[CAM] line_length_pclk = %d\n", rawchip_data.line_length_pclk);
				rawchip_data.frame_length_lines = s_ctrl->msm_sensor_reg->output_settings[res].frame_length_lines;
				CDBG("[CAM] frame_length_lines = %d\n", rawchip_data.frame_length_lines);
				rawchip_data.x_addr_start = s_ctrl->msm_sensor_reg->output_settings[res].x_addr_start;
				CDBG("[CAM] x_addr_start = %d\n", rawchip_data.x_addr_start);
				rawchip_data.y_addr_start = s_ctrl->msm_sensor_reg->output_settings[res].y_addr_start;
				CDBG("[CAM] y_addr_start = %d\n", rawchip_data.y_addr_start);
				rawchip_data.x_addr_end = s_ctrl->msm_sensor_reg->output_settings[res].x_addr_end;
				CDBG("[CAM] x_addr_end = %d\n", rawchip_data.x_addr_end);
				rawchip_data.y_addr_end = s_ctrl->msm_sensor_reg->output_settings[res].y_addr_end;
				CDBG("[CAM] y_addr_end = %d\n", rawchip_data.y_addr_end);
				rawchip_data.x_even_inc = s_ctrl->msm_sensor_reg->output_settings[res].x_even_inc;
				CDBG("[CAM] x_even_inc = %d\n", rawchip_data.x_even_inc);
				rawchip_data.x_odd_inc = s_ctrl->msm_sensor_reg->output_settings[res].x_odd_inc;
				CDBG("[CAM] x_odd_inc = %d\n", rawchip_data.x_odd_inc);
				rawchip_data.y_even_inc = s_ctrl->msm_sensor_reg->output_settings[res].y_even_inc;
				CDBG("[CAM] y_even_inc = %d\n", rawchip_data.y_even_inc);
				rawchip_data.y_odd_inc = s_ctrl->msm_sensor_reg->output_settings[res].y_odd_inc;
				CDBG("[CAM] y_odd_inc = %d\n", rawchip_data.y_odd_inc);
				rawchip_data.binning_rawchip = s_ctrl->msm_sensor_reg->output_settings[res].binning_rawchip;
				CDBG("[CAM] binning_rawchip = %x\n", rawchip_data.binning_rawchip);
				rawchip_data.fullsize_width = s_ctrl->msm_sensor_reg->output_settings[MSM_SENSOR_RES_FULL].x_output;
				CDBG("[CAM] fullsize_width = %d\n", rawchip_data.fullsize_width);
				rawchip_data.fullsize_height = s_ctrl->msm_sensor_reg->output_settings[MSM_SENSOR_RES_FULL].y_output;
				CDBG("[CAM] fullsize_height = %d\n", rawchip_data.fullsize_height);
				rawchip_data.fullsize_line_length_pclk =
					s_ctrl->msm_sensor_reg->output_settings[MSM_SENSOR_RES_FULL].line_length_pclk;
				CDBG("[CAM] fullsize_line_length_pclk = %d\n", rawchip_data.fullsize_line_length_pclk);
				rawchip_data.fullsize_frame_length_lines =
					s_ctrl->msm_sensor_reg->output_settings[MSM_SENSOR_RES_FULL].frame_length_lines;
				CDBG("[CAM] rawchip_data.fullsize_frame_length_lines = %d\n", rawchip_data.fullsize_frame_length_lines);
				rawchip_data.use_rawchip = s_ctrl->sensordata->use_rawchip;/* HTC_START_Simon.Ti_Liu_20120702_Enhance_bypass */

				ktime_get_ts(&ts_start);
				rawchip_set_size(rawchip_data);
				ktime_get_ts(&ts_end);
				pr_info("[CAM] %s: %ld ms\n", __func__,
					(ts_end.tv_sec-ts_start.tv_sec)*1000+(ts_end.tv_nsec-ts_start.tv_nsec)/1000000);
			}
#endif


		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_ISPIF_STREAM, (void *)ISPIF_STREAM(
			PIX_0, ISPIF_ON_FRAME_BOUNDARY));
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(30);
	}
	return rc;
}

int32_t msm_sensor_set_sensor_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int mode, int res)
{
	int32_t rc = 0;
	pr_info("[CAM] %s, res=%d, mode=%d\n", __func__, res, mode);

	if (s_ctrl->curr_res != res) {
		s_ctrl->curr_frame_length_lines =
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines;

		s_ctrl->curr_line_length_pclk =
			s_ctrl->msm_sensor_reg->
			output_settings[res].line_length_pclk;

		if (s_ctrl->sensordata->pdata->is_csic ||
			!s_ctrl->sensordata->csi_if)
			rc = s_ctrl->func_tbl->sensor_csi_setting(s_ctrl,
				MSM_SENSOR_UPDATE_PERIODIC, res);
		else
			rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
				MSM_SENSOR_UPDATE_PERIODIC, res);
		if (rc < 0)
			return rc;
		s_ctrl->curr_res = res;
	}

	return rc;
}

int32_t msm_sensor_mode_init(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info)
{
	int32_t rc = 0;
	s_ctrl->fps_divider = Q10;
	s_ctrl->cam_mode = MSM_SENSOR_MODE_INVALID;

	pr_info("[CAM] %s\n", __func__);

	if (mode != s_ctrl->cam_mode) {
		s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
		s_ctrl->cam_mode = mode;

		if (s_ctrl->sensordata->pdata->is_csic ||
			!s_ctrl->sensordata->csi_if)
			rc = s_ctrl->func_tbl->sensor_csi_setting(s_ctrl,
				MSM_SENSOR_REG_INIT, 0);
		else
			rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
				MSM_SENSOR_REG_INIT, 0);
	}
	return rc;
}

int32_t msm_sensor_get_output_info(struct msm_sensor_ctrl_t *s_ctrl,
		struct sensor_output_info_t *sensor_output_info)
{
	int rc = 0;
	CDBG("%s: called\n", __func__);

	sensor_output_info->num_info = s_ctrl->msm_sensor_reg->num_conf;
/* HTC_START Angie 20111019 - Fix FPS */
	sensor_output_info->vert_offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	sensor_output_info->min_vert = s_ctrl->sensor_exp_gain_info->min_vert;
	sensor_output_info->mirror_flip = s_ctrl->mirror_flip;

/* HTC_START ben 20120229 */
	/* FIXME: assign sensor_max_linecount in each sensor file.
	*  sensor_max_linecount depends on sensor's linecount register size. */

	/* WORKAROUND: set default sensor_max_linecount as max unsigned 32-bit value */
	if(s_ctrl->sensor_exp_gain_info->sensor_max_linecount == 0)
		s_ctrl->sensor_exp_gain_info->sensor_max_linecount = 0xFFFFFFFF;

	sensor_output_info->sensor_max_linecount = s_ctrl->sensor_exp_gain_info->sensor_max_linecount;
/* HTC_END */
	if (copy_to_user((void *)sensor_output_info->output_info,
		s_ctrl->msm_sensor_reg->output_settings,
		sizeof(struct msm_sensor_output_info_t) *
		s_ctrl->msm_sensor_reg->num_conf))
		rc = -EFAULT;

	return rc;
}

int32_t msm_sensor_release(struct msm_sensor_ctrl_t *s_ctrl)
{
	long fps = 0;
	uint32_t delay = 0;
	pr_info("%s: called\n", __func__);

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	if (s_ctrl->curr_res != MSM_SENSOR_INVALID_RES) {
		fps = s_ctrl->msm_sensor_reg->
			output_settings[s_ctrl->curr_res].vt_pixel_clk /
			s_ctrl->curr_frame_length_lines /
			s_ctrl->curr_line_length_pclk;
		delay = 1000 / fps;
		CDBG("%s fps = %ld, delay = %d\n", __func__, fps, delay);
		msleep(delay);
	}
	return 0;
}

long msm_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	void __user *argp = (void __user *)arg;
	CDBG("%s: cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return s_ctrl->func_tbl->sensor_config(s_ctrl, argp);

	case VIDIOC_MSM_SENSOR_RELEASE:
		return msm_sensor_release(s_ctrl);

	default:
		return -ENOIOCTLCMD;
	}
}

int32_t msm_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s: msm_sensor_config: cfgtype = %d\n", __func__, cdata.cfgtype);

		switch (cdata.cfgtype) {
		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			if (s_ctrl->func_tbl->
			sensor_set_fps == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_fps(
				s_ctrl,
				&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			if (s_ctrl->func_tbl->
			sensor_write_exp_gain_ex == NULL) {
				rc = -EFAULT;
				break;
			}
			rc =
				s_ctrl->func_tbl->
				sensor_write_exp_gain_ex(
					s_ctrl,
					cdata.mode, /* HTC Angie 20111019 - Fix FPS */
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.dig_gain, /* HTC_START pg digi gain 20100710 */
					cdata.cfg.exp_gain.line);
			s_ctrl->prev_gain = cdata.cfg.exp_gain.gain;
			s_ctrl->prev_line = cdata.cfg.exp_gain.line;
			break;

		case CFG_SET_PICT_EXP_GAIN:
			if (s_ctrl->func_tbl->
			sensor_write_snapshot_exp_gain_ex == NULL) {
				rc = -EFAULT;
				break;
			}
			rc =
				s_ctrl->func_tbl->
				sensor_write_snapshot_exp_gain_ex(
					s_ctrl,
					cdata.mode, /* HTC Angie 20111019 - Fix FPS */
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.dig_gain, /* HTC_START pg digi gain 20100710 */
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			if (s_ctrl->func_tbl->
			sensor_set_sensor_mode == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_sensor_mode(
					s_ctrl,
					cdata.mode,
					cdata.rs);
			break;

		case CFG_SET_EFFECT:
			break;

		case CFG_SENSOR_INIT:
			if (s_ctrl->func_tbl->
			sensor_mode_init == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_mode_init(
				s_ctrl,
				cdata.mode,
				&(cdata.cfg.init_info));
			break;

		case CFG_GET_OUTPUT_INFO:
			if (s_ctrl->func_tbl->
			sensor_get_output_info == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_get_output_info(
				s_ctrl,
				&cdata.cfg.output_info);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_EEPROM_DATA:
			if (s_ctrl->sensor_eeprom_client == NULL ||
				s_ctrl->sensor_eeprom_client->
				func_tbl.eeprom_get_data == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->sensor_eeprom_client->
				func_tbl.eeprom_get_data(
				s_ctrl->sensor_eeprom_client,
				&cdata.cfg.eeprom_data);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_eeprom_data_t)))
				rc = -EFAULT;
			break;
		/* HTC_START*/
		case CFG_I2C_IOCTL_R_OTP:{
			pr_info("Line:%d CFG_I2C_IOCTL_R_OTP \n", __LINE__);
			if (s_ctrl->func_tbl->sensor_i2c_read_fuseid == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->sensor_i2c_read_fuseid(&cdata, s_ctrl);
			if (copy_to_user(argp, &cdata, sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		}
		break;
		/* HTC_END*/

		default:
			rc = -EFAULT;
			break;
		}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	CDBG("%s: called\n", __func__);

	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_INIT, NULL);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_CFG, (void *)&i2c_conf->i2c_mux_mode);
	return 0;
}

int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	CDBG("%s: called\n", __func__);

	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
				VIDIOC_MSM_I2C_MUX_RELEASE, NULL);
	return 0;
}

int32_t msm_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s: called\n", __func__);

	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: failed to could not allocate mem for regulators\n", __func__);
		return -ENOMEM;
	}

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}

	usleep_range(1000, 2000);
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

	return rc;
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
}

int32_t msm_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s: called %d\n", __func__, __LINE__);

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
	return 0;
}

int32_t msm_sensor_set_power_up(struct msm_sensor_ctrl_t *s_ctrl)//(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	int32_t gpio = 0;
	struct msm_camera_sensor_info *data = NULL;

	pr_info("[CAM] %s\n", __func__);
	if (s_ctrl && s_ctrl->sensordata)
		data = s_ctrl->sensordata;
	else {
		pr_err("[CAM] %s: failed, s_ctrl sensordata is NULL\n", __func__);
		return (-1);
	}

	msm_camio_clk_rate_set(MSM_SENSOR_MCLK_24HZ);

	if (data->sensor_platform_info->sensor_reset_enable)
		gpio = data->sensor_platform_info->sensor_reset;
	else
		gpio = data->sensor_platform_info->sensor_pwd;

	rc = gpio_request(gpio, "SENSOR_NAME");
	if (!rc) {
		pr_info("[CAM] %s: reset sensor: gpio: %d\n", __func__, gpio);
		gpio_direction_output(gpio, 0);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(gpio, 1);
		usleep_range(4000, 5000);
	} else {
		pr_err("[CAM] %s: gpio request fail, gpio: %d\n", __func__, gpio);
	}

	return rc;
}

int32_t msm_sensor_set_power_down(struct msm_sensor_ctrl_t *s_ctrl)//(const struct msm_camera_sensor_info *data)
{
	int32_t gpio = 0;
	struct msm_camera_sensor_info *data = NULL;

	pr_info("[CAM] %s\n", __func__);
	if (s_ctrl && s_ctrl->sensordata)
		data = s_ctrl->sensordata;
	else {
		pr_err("[CAM] %s: failed to s_ctrl sensordata NULL\n", __func__);
		return (-1);
	}

	if (data->sensor_platform_info->sensor_reset_enable)
		gpio = data->sensor_platform_info->sensor_reset;
	else {
		gpio = data->sensor_platform_info->sensor_pwd;
		pr_info("[CAM] sensor_pwd gpio: %d", gpio);
	}

	gpio_set_value_cansleep(gpio, 0);
	usleep_range(1000, 2000);
	gpio_free(gpio);
	return 0;
}

int32_t msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	int i = 10;
	pr_info("[CAM] %s\n", __func__);

	while(i--) {
		rc = msm_camera_i2c_read(
				s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
				MSM_CAMERA_I2C_WORD_DATA);

		if (rc >= 0) {
			if (chipid != s_ctrl->sensor_id_info->sensor_id) {
				pr_info("[CAM] sensor id: 0x%X?\n", chipid);
			} else
				break;
		}

		pr_info("[CAM] retry %d\n", i);
	}
	if (rc < 0) {
		pr_err("[CAM] %s: read id failed\n", __func__);
		return rc;
	}

	pr_info("[CAM] msm_sensor read id: 0x%X, sensor id: 0x%X\n", chipid, s_ctrl->sensor_id_info->sensor_id);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_warning("[CAM] msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

struct msm_sensor_ctrl_t *get_sctrl(struct v4l2_subdev *sd)
{
	return container_of(sd, struct msm_sensor_ctrl_t, sensor_v4l2_subdev);
}

int32_t msm_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;
	pr_info("[CAM] %s\n", __func__);
	pr_info("[CAM] sensor_i2c_probe called - name: %s\n", client->name);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		pr_err("[CAM] %s %s sensor_i2c_client NULL\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	if (s_ctrl->sensordata == NULL) {
		pr_err("[CAM] %s %s NULL sensor data\n", __func__, client->name);
		return -EFAULT;
	}

	msm_csi_i2c_driver_probeup(1); // HTC Glenn Lin 20120718 - To avoid close 8038 L2 of display on boot stage
	msm_camio_probe_on(s_ctrl);

	if (s_ctrl->sensordata->use_rawchip && s_ctrl->sensordata->camera_type == BACK_CAMERA_2D) {
#ifdef CONFIG_RAWCHIP
		rc = rawchip_probe_init();
		if (rc < 0) {
			msm_camio_probe_off(s_ctrl);
			msm_csi_i2c_driver_probeup(0); // HTC Glenn Lin 20120718 - To avoid close 8038 L2 of display on boot stage
			pr_err("[CAM] %s: rawchip probe init failed\n", __func__);
			return rc;
		}
#endif
	}

	if (s_ctrl->func_tbl && s_ctrl->func_tbl->sensor_power_up)
		rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);

	if (rc < 0)
		goto probe_fail;

	rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		goto probe_fail;

	if (s_ctrl->sensor_eeprom_client != NULL) {
		struct msm_camera_eeprom_client *eeprom_client =
			s_ctrl->sensor_eeprom_client;
		if (eeprom_client->func_tbl.eeprom_init != NULL &&
			eeprom_client->func_tbl.eeprom_release != NULL) {
			rc = eeprom_client->func_tbl.eeprom_init(
				eeprom_client,
				s_ctrl->sensor_i2c_client->client->adapter);
			if (rc < 0)
				goto probe_fail;

			rc = msm_camera_eeprom_read_tbl(eeprom_client,
			eeprom_client->read_tbl, eeprom_client->read_tbl_size);
			eeprom_client->func_tbl.eeprom_release(eeprom_client);
			if (rc < 0)
				goto probe_fail;
		}
	}

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);

/* HTC_START steven multiple VCM 20120604 */
    if (s_ctrl->func_tbl->sensor_i2c_read_vcm_driver_ic != NULL)
      s_ctrl->func_tbl->sensor_i2c_read_vcm_driver_ic(s_ctrl);
/* HTC_END steven multiple VCM 20120604 */

	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	goto power_down;
probe_fail:
	pr_err("[CAM] %s %s_i2c_probe failed\n", __func__, client->name);
power_down:
	if (rc > 0)
		rc = 0;

	if (s_ctrl->func_tbl && s_ctrl->func_tbl->sensor_power_down)
		s_ctrl->func_tbl->sensor_power_down(s_ctrl);

#ifdef CONFIG_RAWCHIP /* HTC_START  sync the previous project */
	if (s_ctrl->sensordata->use_rawchip && s_ctrl->sensordata->camera_type == BACK_CAMERA_2D)
		rawchip_probe_deinit();
#endif	/* HTC_END */
	msm_csi_i2c_driver_probeup(0); // HTC Glenn Lin 20120718 - To avoid close 8038 L2 of display on boot stage

	return rc;
}

int32_t msm_sensor_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	CDBG("%s: called\n", __func__);

	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (on)
		rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	else
		rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}

int32_t msm_sensor_v4l2_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	CDBG("%s: called\n", __func__);

	if ((unsigned int)index >= s_ctrl->sensor_v4l2_subdev_info_size)
		return -EINVAL;

	*code = s_ctrl->sensor_v4l2_subdev_info[index].code;
	return 0;
}

int32_t msm_sensor_v4l2_s_ctrl(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int rc = -1, i = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	struct msm_sensor_v4l2_ctrl_info_t *v4l2_ctrl =
		s_ctrl->msm_sensor_v4l2_ctrl_info;
	CDBG("%s: ctrl->id=%d\n", __func__, ctrl->id);

	if (v4l2_ctrl == NULL) {
		pr_info("%s: failed to v4l2_ctrl == NULL\n", __func__);
		return rc;
	}

	for (i = 0; i < s_ctrl->num_v4l2_ctrl; i++) {
		if (v4l2_ctrl[i].ctrl_id == ctrl->id) {
			if (v4l2_ctrl[i].s_v4l2_ctrl != NULL) {
				CDBG("\n calling msm_sensor_s_ctrl_by_enum\n");
				rc = v4l2_ctrl[i].s_v4l2_ctrl(
					s_ctrl,
					&s_ctrl->msm_sensor_v4l2_ctrl_info[i],
					ctrl->value);
			}
			break;
		}
	}

	return rc;
}

int32_t msm_sensor_v4l2_query_ctrl(
	struct v4l2_subdev *sd, struct v4l2_queryctrl *qctrl)
{
	int rc = -1, i = 0;
	struct msm_sensor_ctrl_t *s_ctrl =
		(struct msm_sensor_ctrl_t *) sd->dev_priv;
	CDBG("%s id: %d\n", __func__, qctrl->id);

	if (s_ctrl->msm_sensor_v4l2_ctrl_info == NULL)
		return rc;

	for (i = 0; i < s_ctrl->num_v4l2_ctrl; i++) {
		if (s_ctrl->msm_sensor_v4l2_ctrl_info[i].ctrl_id == qctrl->id) {
			qctrl->minimum =
				s_ctrl->msm_sensor_v4l2_ctrl_info[i].min;
			qctrl->maximum =
				s_ctrl->msm_sensor_v4l2_ctrl_info[i].max;
			qctrl->flags = 1;
			rc = 0;
			break;
		}
	}

	return rc;
}

int msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	CDBG("%s enter\n", __func__);

	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	return rc;
}

static int msm_sensor_debugfs_stream_s(void *data, u64 val)
{
	struct msm_sensor_ctrl_t *s_ctrl = (struct msm_sensor_ctrl_t *) data;
	CDBG("%s called\n", __func__);

	if (val)
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
	else
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_stream, NULL,
			msm_sensor_debugfs_stream_s, "%llu\n");

static int msm_sensor_debugfs_test_s(void *data, u64 val)
{
	CDBG("%s: val: %llu\n", __func__, val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_test, NULL,
			msm_sensor_debugfs_test_s, "%llu\n");

int msm_sensor_enable_debugfs(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct dentry *debugfs_base, *sensor_dir;
	CDBG("%s: called\n", __func__);

	debugfs_base = debugfs_create_dir("msm_sensor", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	sensor_dir = debugfs_create_dir
		(s_ctrl->sensordata->sensor_name, debugfs_base);
	if (!sensor_dir)
		return -ENOMEM;

	if (!debugfs_create_file("stream", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_stream))
		return -ENOMEM;

	if (!debugfs_create_file("test", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_test))
		return -ENOMEM;

	return 0;
}
