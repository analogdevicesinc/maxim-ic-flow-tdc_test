/*******************************************************************************
 * Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 ******************************************************************************/

#include "global.h"
#include "config.h"
#include "uui.h"
#include "board.h"
#include "flow.h"
#include "transducer.h"

#pragma pack(1)

typedef struct _header_t
{
	uint16_t	size;
	uint16_t	crc;
}
header_t;

typedef struct _data_t
{
	flow_sampling_mode_t			flow_sampling_mode;
	flow_sos_method_t	   			flow_sos_method;
	float_t 						sampling_frequency;
	int16_t							tof_temp;
	max3510x_event_timing_mode_t	event_timing_mode;
	max3510x_registers_t			chip_config;
}
data_t;

typedef struct _config_t
{
	header_t	header;
	union
	{
		data_t		data;
		uint32_t    pad[(sizeof(data_t)+3)/4];
	};
}
config_t;

#pragma pack()

static config_t s_config;

void config_save( void )
{
	s_config.header.size = sizeof(s_config.pad);
	s_config.data.flow_sampling_mode = flow_get_sampling_mode();
	s_config.data.flow_sos_method = flow_get_sos_method();
	s_config.data.sampling_frequency = flow_get_sampling_frequency();
	s_config.data.tof_temp = flow_get_tof_temp();
	s_config.data.event_timing_mode = flow_get_event_timing_mode();
	uint16_t crc = board_crc( &s_config.pad, sizeof(s_config.pad) );

	s_config.header.crc = crc;
	board_flash_write( &s_config, sizeof(s_config) );
}

static void apply( void )
{
	flow_set_sampling_mode( s_config.data.flow_sampling_mode );
	flow_set_sampling_frequency( s_config.data.sampling_frequency );
	flow_set_sos_method( s_config.data.flow_sos_method );
	flow_set_tof_temp( s_config.data.tof_temp );
	flow_set_event_timing_mode( s_config.data.event_timing_mode );
}

void config_default( void )
{
    memset( &s_config, 0, sizeof(s_config) );
	const max3510x_registers_t *p_default = transducer_config();
	memcpy( &s_config.data.chip_config, p_default, sizeof(s_config.data.chip_config) );
	s_config.data.flow_sampling_mode = flow_sampling_mode_idle;
	s_config.data.flow_sos_method = flow_sos_method_direct;
	s_config.data.sampling_frequency = 20.0f;
	s_config.data.tof_temp = 1;
	s_config.data.event_timing_mode = max3510x_event_timing_mode_tof;
	apply();
	config_save();
}

void config_load( void )
{
    memset( &s_config, 0, sizeof(s_config) );
	board_flash_read( &s_config, sizeof(s_config) );
	if(  s_config.header.size == sizeof(s_config.pad) )
	{
		uint16_t crc = board_crc( &s_config.pad, sizeof(s_config.pad) );
		if( crc == s_config.header.crc )
		{
			apply();
			return;
		}
	}
	// invalid image in flash -- setup defaults.
	config_default();
}

max3510x_registers_t* config_get_max3510x_regs( void )
{
	return &s_config.data.chip_config;
}
