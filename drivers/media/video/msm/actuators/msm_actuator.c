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

#include <linux/module.h>
#include "msm_actuator.h"

static struct msm_actuator_ctrl_t msm_actuator_t;
static struct msm_actuator msm_vcm_actuator_table;
static struct msm_actuator msm_piezo_actuator_table;

static struct msm_actuator *actuators[] = {
	&msm_vcm_actuator_table,
	&msm_piezo_actuator_table,
};

static int32_t msm_actuator_piezo_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;

	if (a_ctrl->curr_step_pos != 0) {
		a_ctrl->i2c_tbl_index = 0;
		rc = a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			a_ctrl->initial_code, 0, 0);
		rc = a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			a_ctrl->initial_code, 0, 0);
		rc = msm_camera_i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, a_ctrl->i2c_reg_tbl,
			a_ctrl->i2c_tbl_index, a_ctrl->i2c_data_type);
		if (rc < 0) {
			pr_err("%s: i2c write error:%d\n",
				__func__, rc);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->curr_step_pos = 0;
	}
	return rc;
}
int use_eeprom_make_table;

static int32_t msm_actuator_parse_i2c_params(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, uint32_t hw_params, uint16_t delay)
{
	struct msm_actuator_reg_params_t *write_arr = a_ctrl->reg_tbl;
	uint32_t hw_dword = hw_params;
	uint16_t i2c_byte1 = 0, i2c_byte2 = 0;
	uint16_t value = 0;
	uint32_t size = a_ctrl->reg_tbl_size, i = 0;
	int32_t rc = 0;
	struct msm_camera_i2c_reg_tbl *i2c_tbl = a_ctrl->i2c_reg_tbl;
	uint8_t hw_reg_write = 1;
	CDBG("%s: IN\n", __func__);
        if (a_ctrl->curr_hwparams == hw_params)
                hw_reg_write = 0;

	for (i = 0; i < size; i++) {
		if (write_arr[i].reg_write_type == MSM_ACTUATOR_WRITE_DAC) {
		    // Flow Here
			value = (next_lens_position <<
				write_arr[i].data_shift) |
				((hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift);

			if (write_arr[i].reg_addr != 0xFFFF) {
				i2c_byte1 = write_arr[i].reg_addr;
				i2c_byte2 = value;
				if (size != (i+1)) {
					i2c_byte2 = value & 0xFF;
					CDBG("%s: byte1:0x%x, byte2:0x%x\n",
					__func__, i2c_byte1, i2c_byte2);
					i2c_tbl[a_ctrl->i2c_tbl_index].
						reg_addr = i2c_byte1;
					i2c_tbl[a_ctrl->i2c_tbl_index].
						reg_data = i2c_byte2;
					i2c_tbl[a_ctrl->i2c_tbl_index].
						delay = 0;
					a_ctrl->i2c_tbl_index++;
					i++;
					i2c_byte1 = write_arr[i].reg_addr;
					i2c_byte2 = (value & 0xFF00) >> 8;
				}
			} else {
			    // Flow Here
				i2c_byte1 = (value & 0xFF00) >> 8;
				i2c_byte2 = value & 0xFF;
			}
			CDBG("%s: i2c_byte1:0x%x, i2c_byte2:0x%x\n", __func__,
				i2c_byte1, i2c_byte2);
			i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1;
			i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
			i2c_tbl[a_ctrl->i2c_tbl_index].delay = delay;
			a_ctrl->i2c_tbl_index++;
		} else {
			if (hw_reg_write) {
				i2c_byte1 = write_arr[i].reg_addr;
				i2c_byte2 = (hw_dword & write_arr[i].hw_mask) >>
					write_arr[i].hw_shift;
				CDBG("%s: i2c_byte1:0x%x, i2c_byte2:0x%x\n", __func__,
					i2c_byte1, i2c_byte2);
				i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1;
				i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
				i2c_tbl[a_ctrl->i2c_tbl_index].delay = delay;
				a_ctrl->i2c_tbl_index++;
			}
		}
	}
	CDBG("%s: OUT\n", __func__);
	if (rc == 0)
		a_ctrl->curr_hwparams = hw_params;
	return rc;
}

static int32_t msm_actuator_init_focus(struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, enum msm_actuator_data_type type,
	struct reg_settings_t *settings)
{
	int32_t rc = -EFAULT;
	int32_t i = 0;
	CDBG("%s called\n", __func__);

	for (i = 0; i < size; i++) {
		switch (type) {
		case MSM_ACTUATOR_BYTE_DATA:
			rc = msm_camera_i2c_write(
				&a_ctrl->i2c_client,
				settings[i].reg_addr,
				settings[i].reg_data, MSM_CAMERA_I2C_BYTE_DATA);
			break;
		case MSM_ACTUATOR_WORD_DATA:
			rc = msm_camera_i2c_write(
				&a_ctrl->i2c_client,
				settings[i].reg_addr,
				settings[i].reg_data, MSM_CAMERA_I2C_WORD_DATA);
			break;
		default:
			pr_err("%s: Unsupport data type: %d\n",
				__func__, type);
			break;
		}
		if (rc < 0)
			break;
	}

	a_ctrl->curr_step_pos = 0;
	CDBG("%s Exit:%d\n", __func__, rc);
	return rc;
}

static int32_t msm_actuator_write_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t curr_lens_pos,
	struct damping_params_t *damping_params,
	int8_t sign_direction,
	int16_t code_boundary)
{
	int32_t rc = 0;
	int16_t next_lens_pos = 0;
	uint16_t damping_code_step = 0;
	uint16_t wait_time = 0;
/* LGE_CHANGE_S, AF offset enable, 2012-09-28, sungmin.woo@lge.com */
    uint16_t AF_offset_direction=0;
	uint16_t AF_offset = 0;
/* LGE_CHANGE_E, AF offset enable, 2012-09-28, sungmin.woo@lge.com */

	damping_code_step = damping_params->damping_step;
	wait_time = damping_params->damping_delay;

       // Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2013-01-05 Rearrange too many log & unused code
	// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround
	//printk("damping_code_step = %d\n",damping_code_step);
	//printk("wait_time = %d\n",wait_time);
	//printk("curr_lens_pos = %d\n",curr_lens_pos);
	//printk("sign_direction = %d\n",sign_direction);
	//printk("code_boundary = %d\n",code_boundary);
	//printk("damping_params->hw_params = %d\n",damping_params->hw_params);
	// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2013-01-05 Rearrange too many log & unused code

	if (damping_code_step ==0)
	{
		printk("[ERROR][%s] damping_code_step = %d ---> 255\n",__func__,damping_code_step);
		damping_code_step = 255;
	}
	if (wait_time ==0)
	{
		printk("[ERROR][%s] wait_time = %d ---> 4500\n",__func__,damping_code_step);
		wait_time = 4500;
	}
	// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround

	/* Write code based on damping_code_step in a loop */
	if (!use_eeprom_make_table)
	{
		for (next_lens_pos =
			curr_lens_pos + (sign_direction * damping_code_step);
			(sign_direction * next_lens_pos) <=
				(sign_direction * code_boundary);
			next_lens_pos =
				(next_lens_pos +
					(sign_direction * damping_code_step))) {
			rc = a_ctrl->func_tbl->
				actuator_parse_i2c_params(a_ctrl, next_lens_pos,
					damping_params->hw_params, wait_time);
			if (rc < 0) {
				pr_err("%s: error:%d\n",
					__func__, rc);
				return rc;
			}
			curr_lens_pos = next_lens_pos;
		}
	}
// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2013-01-05 Rearrange too many log & unused code
/* LGE_CHANGE_S, AF offset enable, 2012-09-28, sungmin.woo@lge.com */
			//printk("#### code_boundary : %d, a_ctrl->af_status = %d ####\n", code_boundary, a_ctrl->af_status);
			if((a_ctrl->af_status==6)  && (a_ctrl->AF_defocus_enable==1))  //af_status : 6 = Last AF
			{
				AF_offset_direction = 0x8000 & ( a_ctrl->AF_LG_defocus_offset);
				AF_offset = 0x7FFF & ( a_ctrl->AF_LG_defocus_offset);

				if (0x8000==AF_offset_direction)
				{
					AF_offset = ~(AF_offset |0x8000) + 1;

					if (AF_offset > 30)
						AF_offset =0;

					code_boundary = code_boundary -AF_offset;
				}
				else
				{
					if (AF_offset > 30)
						AF_offset =0;
					code_boundary = code_boundary + AF_offset;

				}

				printk("#### Last AF 1, code : %d, offset : %d !!! ####\n", code_boundary, a_ctrl->AF_LG_defocus_offset);
			}
			//printk("#### %s : code_boundary = %d, state = %d ####\n",__func__, code_boundary, a_ctrl->af_status);
/* LGE_CHANGE_E, AF offset enable, 2012-09-28, sungmin.woo@lge.com */
// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2013-01-05 Rearrange too many log & unused code

	if (curr_lens_pos != code_boundary) {
		rc = a_ctrl->func_tbl->
			actuator_parse_i2c_params(a_ctrl, code_boundary,
				damping_params->hw_params, wait_time);
	}
	return rc;
}

static int32_t msm_actuator_piezo_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t dest_step_position = move_params->dest_step_pos;
	int32_t rc = 0;
	int32_t num_steps = move_params->num_steps;

	if (num_steps == 0)
		return rc;

	a_ctrl->i2c_tbl_index = 0;
	rc = a_ctrl->func_tbl->
		actuator_parse_i2c_params(a_ctrl,
		(num_steps *
		a_ctrl->region_params[0].code_per_step),
		move_params->ringing_params[0].hw_params, 0);

	rc = msm_camera_i2c_write_table_w_microdelay(&a_ctrl->i2c_client,
		a_ctrl->i2c_reg_tbl, a_ctrl->i2c_tbl_index,
		a_ctrl->i2c_data_type);
	if (rc < 0) {
		pr_err("%s: i2c write error:%d\n",
			__func__, rc);
		return rc;
	}
	a_ctrl->i2c_tbl_index = 0;
	a_ctrl->curr_step_pos = dest_step_position;
	return rc;
}

static int32_t msm_actuator_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	int8_t sign_dir = move_params->sign_dir;
	uint16_t step_boundary = 0;
	uint16_t target_step_pos = 0;
	uint16_t target_lens_pos = 0;
	int16_t dest_step_pos = move_params->dest_step_pos;
	uint16_t curr_lens_pos = 0;
	int dir = move_params->dir;
	int32_t num_steps = move_params->num_steps;

    // Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround
	int count_actuator_write = 0;
	 // Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2013-01-05 Rearrange too many log & unused code
	//printk("%s: a_ctrl->curr_region_index = %d\n",__func__,a_ctrl->curr_region_index);
	// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround
	CDBG("%s called, dir %d, num_steps %d\n",
		__func__,
		dir,
		num_steps);
	// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2013-01-05 Rearrange too many log & unused code
       /* LGE_CHANGE_S, AF offset enable, 2012-09-28, sungmin.woo@lge.com */
	a_ctrl->af_status = move_params->af_status;
/* LGE_CHANGE_E, AF offset enable, 2012-09-28, sungmin.woo@lge.com */

	if (dest_step_pos == a_ctrl->curr_step_pos)
		return rc;

	curr_lens_pos = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	a_ctrl->i2c_tbl_index = 0;
	// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2013-01-05 Rearrange too many log & unused code
	printk("curr_step_pos =%d dest_step_pos =%d curr_lens_pos=%d\n",
		a_ctrl->curr_step_pos, dest_step_pos, curr_lens_pos);
	// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2013-01-05 Rearrange too many log & unused code

	while (a_ctrl->curr_step_pos != dest_step_pos) {
		step_boundary =
			a_ctrl->region_params[a_ctrl->curr_region_index].
			step_bound[dir];
		if ((dest_step_pos * sign_dir) <=
			(step_boundary * sign_dir)) {

/* LGE_CHANGE_S, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
			struct damping_params_t damping_param, *usr_damping_param ; // namkyu2.kang
/* LGE_CHANGE_E, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
			target_step_pos = dest_step_pos;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
/* LGE_CHANGE_S, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
			usr_damping_param = &(move_params->ringing_params[a_ctrl->curr_region_index]) ;
			if (copy_from_user(&damping_param,
					(void *)usr_damping_param,
					sizeof(struct damping_params_t)))
			{
				pr_err("%s: ringing_param is on FAULT Address : %p\n",
						__func__, (void *)usr_damping_param ) ;
				return -EFAULT;
			}
/* LGE_CHANGE_E, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
			//pr_err("%s:[1] usr_damping_param = %p", __func__, usr_damping_param);
			//pr_err("%s:[1] &(move_params->ringing_params[a_ctrl->curr_region_index]) = %p\n",  __func__, &(move_params->ringing_params[a_ctrl->curr_region_index]));
			rc = a_ctrl->func_tbl->
				actuator_write_focus(
					a_ctrl,
					curr_lens_pos,
/* LGE_CHANGE_S, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
//					&(move_params->
//						ringing_params[a_ctrl->
//						curr_region_index]),
					&damping_param, // namkyu2.kang
/* LGE_CHANGE_E, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
					sign_dir,
					target_lens_pos);
			if (rc < 0) {
				pr_err("%s: error:%d\n",
					__func__, rc);
				return rc;
			}
			curr_lens_pos = target_lens_pos;
			// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround
			count_actuator_write ++;
			//printk("%s count_actuator_write = %d\n",__func__,count_actuator_write);
			// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround

		} else {
/* LGE_CHANGE_S, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
			struct damping_params_t damping_param, *usr_damping_param ; // namkyu2.kang
/* LGE_CHANGE_E, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
			target_step_pos = step_boundary;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
/* LGE_CHANGE_S, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
			usr_damping_param = &(move_params->ringing_params[a_ctrl->curr_region_index]) ;
			if (copy_from_user(&damping_param,
					(void *)usr_damping_param,
					sizeof(struct damping_params_t)))
			{
				pr_err("%s: ringing_param is on FAULT Address : %p\n",
						__func__, (void *)usr_damping_param ) ;
				return -EFAULT;
			}
/* LGE_CHANGE_E, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
			//pr_err("%s:[2] usr_damping_param = %p", __func__, usr_damping_param);
			//pr_err("%s:[2] &(move_params->ringing_params[a_ctrl->curr_region_index]) = %p\n",  __func__, &(move_params->ringing_params[a_ctrl->curr_region_index]));
			rc = a_ctrl->func_tbl->
				actuator_write_focus(
					a_ctrl,
					curr_lens_pos,
/* LGE_CHANGE_S, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
//					&(move_params->
//						ringing_params[a_ctrl->
//						curr_region_index]),
					&damping_param, // namkyu2.kang
/* LGE_CHANGE_E, fix kernel crash while AF from namkyu2.kang, 2012.01.21, jungryoul.choi@lge.com */
					sign_dir,
					target_lens_pos);
			if (rc < 0) {
				pr_err("%s: error:%d\n",
					__func__, rc);
				return rc;
			}
			curr_lens_pos = target_lens_pos;

			a_ctrl->curr_region_index += sign_dir;
			// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround
			if (a_ctrl->curr_region_index >= 2)
			{
			 printk("[ERROR][%s] a_ctrl->curr_region_index = %d ---> 1\n",__func__,a_ctrl->curr_region_index);
			 a_ctrl->curr_region_index = 1;
			}
			if (a_ctrl->curr_region_index < 0)
			{
			 printk("[ERROR][%s] a_ctrl->curr_region_index = %d ---> 0\n",__func__,a_ctrl->curr_region_index);
			 a_ctrl->curr_region_index = 0;
			}
			count_actuator_write ++;
			//printk("%s count_actuator_write = %d\n",__func__,count_actuator_write);
			// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround
		}
		a_ctrl->curr_step_pos = target_step_pos;
		// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround
		if (count_actuator_write > 2)
		{
		   printk("[ERROR][%s] count_actuator_write = %d ---> break\n",__func__,count_actuator_write);
		   break;
	    }
		// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-08-10 Add log for debug AF issue & WorkAround
	}

	rc = msm_camera_i2c_write_table_w_microdelay(&a_ctrl->i2c_client,
		a_ctrl->i2c_reg_tbl, a_ctrl->i2c_tbl_index,
		a_ctrl->i2c_data_type);
	if (rc < 0) {
		pr_err("%s: i2c write error:%d\n",
			__func__, rc);
		return rc;
	}
	a_ctrl->i2c_tbl_index = 0;

	return rc;
}

// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-07-20 Apply AF calibration data
static int32_t msm_actuator_init_step_table(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info);

#define ACT_MIN_MOVE_RANGE	200 // TBD
#define ACT_POSTURE_MARGIN   100 //100 /* LGE_CHANGE modified 2012.8.30 sungmin.woo@lge.com */

#if defined(CONFIG_IMX091)
    extern uint8_t imx091_afcalib_data[8];
#endif // #if defined(CONFIG_IMX091)

#if defined(CONFIG_IMX111)
    extern uint8_t imx111_afcalib_data[4];
#endif // #if defined(CONFIG_IMX111)

/* LGE_CHANGE_S, AF offset enable, 2012-09-28, sungmin.woo@lge.com */
extern uint8_t imx091_af_defocus_data[11];
/* LGE_CHANGE_E, AF offset enable, 2012-09-28, sungmin.woo@lge.com */
static int32_t msm_actuator_init_step_table_use_eeprom(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int32_t rc = 0;
	int16_t cur_code = 0;
	int16_t step_index = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	uint16_t act_start = 0, act_macro = 0, move_range = 0;

	for (; data_size > 0; data_size--)
		max_code_size *= 2;

	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;

	CDBG("%s called\n", __func__);
    // Start LGE_BSP_CAMERA::seongjo.kim@lge.com EEPROM Enable at IMX111&IMX091
	// set act_start, act_macro
	#if defined(CONFIG_IMX091)
	act_start = (uint16_t)(imx091_afcalib_data[1] << 8) |
			imx091_afcalib_data[0];
	act_macro = (uint16_t)(imx091_afcalib_data[3] << 8) |
			imx091_afcalib_data[2];
	printk("[QCTK_EEPROM][IMX091] %s: act_start = %d\n",__func__,act_start);
	printk("[QCTK_EEPROM][IMX091] %s: act_macro = %d\n",__func__,act_macro);
/* LGE_CHANGE_S, AF offset enable, 2012-09-28, sungmin.woo@lge.com */
    printk("####  imx091_af_defocus_data 0 = %x ####\n",imx091_af_defocus_data[0]);
    printk("####  imx091_af_defocus_data 1 = %x ####\n",imx091_af_defocus_data[1]);
    printk("####  imx091_af_defocus_data 2 = %x ####\n",imx091_af_defocus_data[2]);
    printk("####  imx091_af_defocus_data 3 = %x ####\n",imx091_af_defocus_data[3]);
    printk("####  imx091_af_defocus_data 4 = %x ####\n",imx091_af_defocus_data[4]);
    printk("####  imx091_af_defocus_data 5 = %x ####\n",imx091_af_defocus_data[5]);
    printk("####  imx091_af_defocus_data 6 = %x ####\n",imx091_af_defocus_data[6]);


    a_ctrl->AF_defocus_enable = (uint8_t) imx091_af_defocus_data[0];
    a_ctrl->AF_center_best_code = (uint16_t) (imx091_af_defocus_data[1] << 8) |imx091_af_defocus_data[2];
    a_ctrl->AF_balance_best_code = (uint16_t) (imx091_af_defocus_data[3] << 8) |imx091_af_defocus_data[4];
    a_ctrl->AF_defocus_offset = (uint16_t) (imx091_af_defocus_data[5] << 8) |imx091_af_defocus_data[6];
	a_ctrl->AF_LG_center_best_code = (uint16_t) (imx091_af_defocus_data[7] << 8) |imx091_af_defocus_data[8];
    a_ctrl->AF_LG_defocus_offset = (uint16_t) (imx091_af_defocus_data[9] << 8) |imx091_af_defocus_data[10];

    printk("####  AF_defocus_enable = %d ####\n",a_ctrl->AF_defocus_enable);
    printk("####  AF_center_best_code = %d ####\n",a_ctrl->AF_center_best_code);
    printk("####  AF_balance_best_code = %d ####\n",a_ctrl->AF_balance_best_code);
    printk("####  AF_defocus_offset = %d ####\n",a_ctrl->AF_defocus_offset);
	printk("####  AF_LG_center_best_code = %d ####\n",a_ctrl->AF_LG_center_best_code);
    printk("####  AF_LG_defocus_offset = %d ####\n",a_ctrl->AF_LG_defocus_offset);
/* LGE_CHANGE_E, AF offset enable, 2012-09-28, sungmin.woo@lge.com */
	#elif defined(CONFIG_IMX111)
	act_start = (uint16_t)(imx111_afcalib_data[1] << 8) |
			imx111_afcalib_data[0];
	act_macro = ((uint16_t)(imx111_afcalib_data[3] << 8) |
        imx111_afcalib_data[2])+20; 
	printk("[QCTK_EEPROM][IMX111] %s: act_start = %d\n",__func__,act_start);
	printk("[QCTK_EEPROM][IMX111] %s: act_macro = %d\n",__func__,act_macro);
	#endif
	// End LGE_BSP_CAMERA::seongjo.kim@lge.com EEPROM Enable at IMX111&IMX091

	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);

	if (a_ctrl->step_position_table == NULL)
		return -EFAULT;

	cur_code = set_info->af_tuning_params.initial_code;
	a_ctrl->step_position_table[step_index++] = cur_code;

	// start code - by calibration data
	if ( act_start > ACT_POSTURE_MARGIN )
		a_ctrl->step_position_table[1] = act_start - ACT_POSTURE_MARGIN;
	else
		a_ctrl->step_position_table[1] = act_start ;

	move_range = act_macro - a_ctrl->step_position_table[1];

	printk("[QCTK_EEPROM] move_range: %d\n", move_range);
	printk("[QCTK_EEPROM] a_ctrl->total_steps = %d\n",a_ctrl->total_steps);
	printk("[QCTK_EEPROM] set_info->af_tuning_params.total_steps = %d\n",set_info->af_tuning_params.total_steps);

    // Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-11-23 Unnecessary action remove when using eeprom
	if (move_range < ACT_MIN_MOVE_RANGE)
	{
	    printk("[QTCK_EEPROM] Not use eeprom\n");
	    use_eeprom_make_table = 0;
		goto act_cal_fail;
	}
	else
	{
	    printk("[QTCK_EEPROM] Use eeprom\n");
		use_eeprom_make_table = 1;
	}
	// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-11-23 Unnecessary action remove when using eeprom

	for (step_index = 2;step_index < set_info->af_tuning_params.total_steps;step_index++) {
		a_ctrl->step_position_table[step_index]
			= ((step_index - 1) * move_range + ((set_info->af_tuning_params.total_steps - 1) >> 1))
			/ (set_info->af_tuning_params.total_steps - 1) + a_ctrl->step_position_table[1];
	}

/* LGE_CHANGE_S, revert to original, 2012-04-27, sungmin.woo@lge.com */
	printk("Actuator Clibration table: start(%d),macro(%d) ==============\n",
	act_start, act_macro);

	for (step_index = 0; step_index < a_ctrl->total_steps; step_index++)
		printk("step_position_table[%d]= %d\n",step_index,
		a_ctrl->step_position_table[step_index]);
/* LGE_CHANGE_E, revert to original, 2012-04-27, sungmin.woo@lge.com */
	return rc;

act_cal_fail:
	pr_err("%s: calibration to default value not using eeprom data\n", __func__);
	rc = msm_actuator_init_step_table(a_ctrl, set_info);
	return rc;
}
// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-07-20 Apply AF calibration data

static int32_t msm_actuator_init_step_table(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int16_t code_per_step = 0;
	int32_t rc = 0;
	int16_t cur_code = 0;
	int16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	uint16_t i=0;
	CDBG("%s called\n", __func__);

	for (; data_size > 0; data_size--)
		max_code_size *= 2;

	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;

	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);

	if (a_ctrl->step_position_table == NULL)
		return -EFAULT;

	cur_code = set_info->af_tuning_params.initial_code;
	a_ctrl->step_position_table[step_index++] = cur_code;
	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		code_per_step =
			a_ctrl->region_params[region_index].code_per_step;
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		for (; step_index <= step_boundary;
			step_index++) {
			cur_code += code_per_step;
			if (cur_code < max_code_size)
				a_ctrl->step_position_table[step_index] =
					cur_code;
			else {
				for (; step_index <
					set_info->af_tuning_params.total_steps;
					step_index++)
					a_ctrl->
						step_position_table[
						step_index] =
						max_code_size;

				return rc;
			}
		}
	}
	// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-07-20 Apply AF calibration data
	printk("[AF] Actuator Clibration table: not apply calibration data ==============\n");
	for (step_index = 0; step_index < set_info->af_tuning_params.total_steps; step_index++)
		printk("[AF] step_position_table[%d]= %d\n",step_index,
		a_ctrl->step_position_table[step_index]);
	// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-07-20 Apply AF calibration data

	for (i=0; i<set_info->af_tuning_params.total_steps; i++) {
		printk("%s: Step_Pos_Table[%d]:%d\n", __func__, i,
			a_ctrl->step_position_table[i]);
	}
	return rc;
}

#define IMX091_ACT_STOP_POS 10 // [G][Camera][Common] Fix the Actuator Noise - "Tick!" - Issue by adding delay in proportion to distance of "Infinite"
static int32_t msm_actuator_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
//  Start - [G][Camera][Common] youngwook.song Fix the Actuator Noise - "Tick!" - Issue by adding delay in proportion to distance of "Infinite"
#if 1
	uint32_t hw_damping;
	unsigned int delay;
	int init_pos, cur_dac, mid_dac, cur_pos;
	CDBG("%s called\n", __func__);

	cur_pos = a_ctrl->curr_step_pos;
	CDBG("%s: E\n", __func__);
	if (a_ctrl->curr_step_pos != 0)
	{
		hw_damping = 0xA;
		delay = 0;
		init_pos = a_ctrl->initial_code;
		cur_dac = a_ctrl->step_position_table[cur_pos];
		mid_dac = a_ctrl->step_position_table[IMX091_ACT_STOP_POS];
		CDBG("%s : init_pos : %d, cur_dac : %d, mid_dac : %d\n", __func__, init_pos, cur_dac, mid_dac);

		if(cur_pos > IMX091_ACT_STOP_POS) {
			// Start - [G][Camera][Common] seongjo.kim Fix kernel crash when exit camera
			a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl, mid_dac, hw_damping, 0);
			rc = msm_camera_i2c_write_table_w_microdelay(&a_ctrl->i2c_client,
			a_ctrl->i2c_reg_tbl, a_ctrl->i2c_tbl_index,
			a_ctrl->i2c_data_type);
			if (rc < 0) {
				pr_err("%s: i2c write error:%d\n",
					__func__, rc);
				return rc;
			}
			a_ctrl->i2c_tbl_index = 0;
			// End - [G][Camera][Common] seongjo.kim Fix kernel crash when exit camera
			delay = (cur_dac - mid_dac) * 2 / 3;
			if(delay > 300) delay = 300;
			CDBG("%s: To mid - step = %d, position = %d, delay = %d", __func__, cur_pos, cur_dac, delay);
			mdelay(delay);
		} else
			mid_dac = cur_dac;

		hw_damping = 0x7;
		delay = mid_dac / 2;
		if(delay > 300) delay = 300;
		// Start - [G][Camera][Common] seongjo.kim Fix kernel crash when exit camera
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl, init_pos + 10, hw_damping, 0);
		rc = msm_camera_i2c_write_table_w_microdelay(&a_ctrl->i2c_client,
		a_ctrl->i2c_reg_tbl, a_ctrl->i2c_tbl_index,
		a_ctrl->i2c_data_type);
		if (rc < 0) {
			pr_err("%s: i2c write error:%d\n",
				__func__, rc);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
		// End - [G][Camera][Common] seongjo.kim Fix kernel crash when exit camera
		CDBG("%s: To end - step = 10, position = %d, delay = %d", __func__, mid_dac, delay);
		mdelay(delay); 	// delay can be changed but put max due to it's small enough
		a_ctrl->curr_step_pos = 0;
	}
#else // QCT Original Source.
		rc = a_ctrl->func_tbl->actuator_move_focus(a_ctrl, move_params);
#endif
//* End - [G][Camera][Common] youngwook.song Fix the Actuator Noise - "Tick!" - Issue by adding delay in proportion to distance of "Infinite"
	return rc;
}

static int32_t msm_actuator_power_down(struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	if (a_ctrl->vcm_enable) {
		rc = gpio_direction_output(a_ctrl->vcm_pwd, 0);
		if (!rc)
			gpio_free(a_ctrl->vcm_pwd);
	}

	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;
	kfree(a_ctrl->i2c_reg_tbl);
	a_ctrl->i2c_reg_tbl = NULL;
	a_ctrl->i2c_tbl_index = 0;
	return rc;
}

static int32_t msm_actuator_init(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info) {
	struct reg_settings_t *init_settings = NULL;
	int32_t rc = -EFAULT;
	uint16_t i = 0;
	CDBG("%s: IN\n", __func__);

	for (i = 0; i < ARRAY_SIZE(actuators); i++) {
		if (set_info->actuator_params.act_type ==
			actuators[i]->act_type) {
			a_ctrl->func_tbl = &actuators[i]->func_tbl;
			rc = 0;
		}
	}

	if (rc < 0) {
		pr_err("%s: Actuator function table not found\n", __func__);
		return rc;
	}

	a_ctrl->region_size = set_info->af_tuning_params.region_size;
	if (a_ctrl->region_size > MAX_ACTUATOR_REGION) {
		pr_err("%s: MAX_ACTUATOR_REGION is exceeded.\n", __func__);
		return -EFAULT;
	}
	a_ctrl->pwd_step = set_info->af_tuning_params.pwd_step;
	a_ctrl->total_steps = set_info->af_tuning_params.total_steps;

	if (copy_from_user(&a_ctrl->region_params,
		(void *)set_info->af_tuning_params.region_params,
		a_ctrl->region_size * sizeof(struct region_params_t)))
		return -EFAULT;

	a_ctrl->i2c_data_type = set_info->actuator_params.i2c_data_type;
	a_ctrl->i2c_client.client->addr = set_info->actuator_params.i2c_addr;
	a_ctrl->i2c_client.addr_type = set_info->actuator_params.i2c_addr_type;
	a_ctrl->reg_tbl_size = set_info->actuator_params.reg_tbl_size;
	if (a_ctrl->reg_tbl_size > MAX_ACTUATOR_REG_TBL_SIZE) {
		pr_err("%s: MAX_ACTUATOR_REG_TBL_SIZE is exceeded.\n",
			__func__);
		return -EFAULT;
	}

	a_ctrl->i2c_reg_tbl =
		kmalloc(sizeof(struct msm_camera_i2c_reg_tbl) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);
	if (!a_ctrl->i2c_reg_tbl) {
		pr_err("%s kmalloc fail\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(&a_ctrl->reg_tbl,
		(void *)set_info->actuator_params.reg_tbl_params,
		a_ctrl->reg_tbl_size *
		sizeof(struct msm_actuator_reg_params_t))) {
		kfree(a_ctrl->i2c_reg_tbl);
		return -EFAULT;
	}

	if (set_info->actuator_params.init_setting_size) {
		if (a_ctrl->func_tbl->actuator_init_focus) {
			init_settings = kmalloc(sizeof(struct reg_settings_t) *
				(set_info->actuator_params.init_setting_size),
				GFP_KERNEL);
			if (init_settings == NULL) {
				kfree(a_ctrl->i2c_reg_tbl);
				pr_err("%s Error allocating memory for init_settings\n",
					__func__);
				return -EFAULT;
			}
			if (copy_from_user(init_settings,
				(void *)set_info->actuator_params.init_settings,
				set_info->actuator_params.init_setting_size *
				sizeof(struct reg_settings_t))) {
				kfree(init_settings);
				kfree(a_ctrl->i2c_reg_tbl);
				pr_err("%s Error copying init_settings\n",
					__func__);
				return -EFAULT;
			}
			rc = a_ctrl->func_tbl->actuator_init_focus(a_ctrl,
				set_info->actuator_params.init_setting_size,
				a_ctrl->i2c_data_type,
				init_settings);
			kfree(init_settings);
			if (rc < 0) {
				kfree(a_ctrl->i2c_reg_tbl);
				pr_err("%s Error actuator_init_focus\n",
					__func__);
				return -EFAULT;
			}
		}
	}

	a_ctrl->initial_code = set_info->af_tuning_params.initial_code;
	if (a_ctrl->func_tbl->actuator_init_step_table)
		rc = a_ctrl->func_tbl->
			actuator_init_step_table(a_ctrl, set_info);

	a_ctrl->curr_step_pos = 0;
	a_ctrl->curr_region_index = 0;

	return rc;
}


static int32_t msm_actuator_config(struct msm_actuator_ctrl_t *a_ctrl,
							void __user *argp)
{
	struct msm_actuator_cfg_data cdata;
	int32_t rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct msm_actuator_cfg_data)))
		return -EFAULT;
	mutex_lock(a_ctrl->actuator_mutex);
	CDBG("%s called, type %d\n", __func__, cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SET_ACTUATOR_INFO:
		rc = msm_actuator_init(a_ctrl, &cdata.cfg.set_info);
		if (rc < 0)
			pr_err("%s init table failed %d\n", __func__, rc);
		break;

	case CFG_SET_DEFAULT_FOCUS:
		rc = a_ctrl->func_tbl->actuator_set_default_focus(a_ctrl,
			&cdata.cfg.move);
		if (rc < 0)
			pr_err("%s move focus failed %d\n", __func__, rc);
		break;

	case CFG_MOVE_FOCUS:
		rc = a_ctrl->func_tbl->actuator_move_focus(a_ctrl,
			&cdata.cfg.move);
		if (rc < 0)
			pr_err("%s move focus failed %d\n", __func__, rc);
		break;

	default:
		break;
	}
	mutex_unlock(a_ctrl->actuator_mutex);
	return rc;
}

static int32_t msm_actuator_i2c_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_actuator_ctrl_t *act_ctrl_t = NULL;
	CDBG("%s called\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	act_ctrl_t = (struct msm_actuator_ctrl_t *)(id->driver_data);
	CDBG("%s client = %x\n",
		__func__, (unsigned int) client);
	act_ctrl_t->i2c_client.client = client;

	/* Assign name for sub device */
	snprintf(act_ctrl_t->sdev.name, sizeof(act_ctrl_t->sdev.name),
			 "%s", act_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&act_ctrl_t->sdev,
		act_ctrl_t->i2c_client.client,
		act_ctrl_t->act_v4l2_subdev_ops);

	CDBG("%s succeeded\n", __func__);
	return rc;

probe_failure:
	pr_err("%s failed! rc = %d\n", __func__, rc);
	return rc;
}

static int32_t msm_actuator_power_up(struct msm_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("%s called\n", __func__);

	CDBG("vcm info: %x %x\n", a_ctrl->vcm_pwd,
		a_ctrl->vcm_enable);
	if (a_ctrl->vcm_enable) {
		rc = gpio_request(a_ctrl->vcm_pwd, "msm_actuator");
		if (!rc) {
			CDBG("Enable VCM PWD\n");
			gpio_direction_output(a_ctrl->vcm_pwd, 1);
		}
	}
	return rc;
}

DEFINE_MUTEX(msm_actuator_mutex);

static const struct i2c_device_id msm_actuator_i2c_id[] = {
	{"msm_actuator", (kernel_ulong_t)&msm_actuator_t},
	{ }
};

static struct i2c_driver msm_actuator_i2c_driver = {
	.id_table = msm_actuator_i2c_id,
	.probe  = msm_actuator_i2c_probe,
	.remove = __exit_p(msm_actuator_i2c_remove),
	.driver = {
		.name = "msm_actuator",
	},
};

static int __init msm_actuator_i2c_add_driver(
	void)
{
	CDBG("%s called\n", __func__);
	return i2c_add_driver(msm_actuator_t.i2c_driver);
}

static long msm_actuator_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_actuator_ctrl_t *a_ctrl = get_actrl(sd);
	void __user *argp = (void __user *)arg;
	switch (cmd) {
	case VIDIOC_MSM_ACTUATOR_CFG:
		return msm_actuator_config(a_ctrl, argp);
	default:
		return -ENOIOCTLCMD;
	}
}

static int32_t msm_actuator_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_actuator_ctrl_t *a_ctrl = get_actrl(sd);
	mutex_lock(a_ctrl->actuator_mutex);
	if (on)
		rc = msm_actuator_power_up(a_ctrl);
	else
		rc = msm_actuator_power_down(a_ctrl);
	mutex_unlock(a_ctrl->actuator_mutex);
	return rc;
}

struct msm_actuator_ctrl_t *get_actrl(struct v4l2_subdev *sd)
{
	return container_of(sd, struct msm_actuator_ctrl_t, sdev);
}

static struct v4l2_subdev_core_ops msm_actuator_subdev_core_ops = {
	.ioctl = msm_actuator_subdev_ioctl,
	.s_power = msm_actuator_power,
};

static struct v4l2_subdev_ops msm_actuator_subdev_ops = {
	.core = &msm_actuator_subdev_core_ops,
};

static struct msm_actuator_ctrl_t msm_actuator_t = {
	.i2c_driver = &msm_actuator_i2c_driver,
	.act_v4l2_subdev_ops = &msm_actuator_subdev_ops,

	.curr_step_pos = 0,
	.curr_region_index = 0,
	.actuator_mutex = &msm_actuator_mutex,

};

static struct msm_actuator msm_vcm_actuator_table = {
	.act_type = ACTUATOR_VCM,
	.func_tbl = {
		// Start LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-07-20 Apply AF calibration data
		.actuator_init_step_table = msm_actuator_init_step_table_use_eeprom,
		// End LGE_BSP_CAMERA::seongjo.kim@lge.com 2012-07-20 Apply AF calibration data
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
	},
};

static struct msm_actuator msm_piezo_actuator_table = {
	.act_type = ACTUATOR_PIEZO,
	.func_tbl = {
		.actuator_init_step_table = NULL,
		.actuator_move_focus = msm_actuator_piezo_move_focus,
		.actuator_write_focus = NULL,
		.actuator_set_default_focus =
			msm_actuator_piezo_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
	},
};

subsys_initcall(msm_actuator_i2c_add_driver);
MODULE_DESCRIPTION("MSM ACTUATOR");
MODULE_LICENSE("GPL v2");
