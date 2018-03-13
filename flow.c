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
#include "flow.h"
#include "board.h"
#include "uui.h"
#include "config.h"
#include "transducer.h"

typedef enum _sampling_process_event_t
{
	sampling_process_event_init,
	sampling_process_event_run,
	sampling_process_event_complete
}
sampling_process_event_t;

typedef struct _wave_track_t
{
	max3510x_time_t last[MAX3510X_MAX_HITCOUNT];
	uint8_t			hitwaves[MAX3510X_MAX_HITCOUNT];
}
wave_track_t;

static flow_sampling_mode_t		s_flow_sampling_mode;
static flow_sampling_mode_t		s_last_flow_sampling_mode;
static flow_sos_method_t 		s_sos_method;

static bool 					s_response_pending;
static float_t					s_sampling_freq;

static flow_sampling_mode_t s_requested_flow_sampling_mode;

static uint32_t s_last_sample_time;
static int16_t 	s_tof_temp;
static int16_t 	s_tof_temp_count;
static max3510x_event_timing_mode_t s_event_timing_mode;
static void (*s_p_next_measurement)(max3510x_t);
static uint8_t s_hitcount;

static void interleave( void )
{
	if( s_tof_temp > 0  )
	{
		s_tof_temp_count++;
		if( s_tof_temp_count >= s_tof_temp )
		{
			s_tof_temp_count = 0;
		}
	}
	else if( s_tof_temp <= 0  )
	{
		s_tof_temp_count--;
		if( s_tof_temp_count <= s_tof_temp )
		{
			s_tof_temp_count = 0;
		}
	}
}

static void tof_diff( void *v )
{
    max3510x_tof_diff(NULL);
}

static void start_next_measurement( bool clock )
{

	if( s_requested_flow_sampling_mode != flow_sampling_mode_invalid )
	{
		s_flow_sampling_mode = s_requested_flow_sampling_mode;
		s_requested_flow_sampling_mode = flow_sampling_mode_invalid;
	}
	if( s_flow_sampling_mode != flow_sampling_mode_host )
	{
		if( s_last_flow_sampling_mode == flow_sampling_mode_host )
			board_clock_enable(false);
	}
	if( s_last_flow_sampling_mode == flow_sampling_mode_event  &&
			s_flow_sampling_mode != flow_sampling_mode_event )
		max3510x_halt(NULL);

	if( (s_flow_sampling_mode == flow_sampling_mode_max) )
	{
		if( s_tof_temp > 0  )
		{
			if( s_tof_temp_count == 1 )
				max3510x_temperature(NULL);
			else
			{
				tof_diff(NULL);
			}
		}
		else if( s_tof_temp < 0 )
		{
			if( s_tof_temp_count == -1 )
				tof_diff(NULL);
			else
			{
				max3510x_temperature(NULL);
			}
		}
		else
		{
			tof_diff(NULL);
		}
		interleave();
		s_response_pending = true;
	}
	else if( s_flow_sampling_mode == flow_sampling_mode_host )
	{
		if( s_last_flow_sampling_mode != flow_sampling_mode_host )
		{
			board_clock_enable(true);
		}
		if (clock)
		{
			interleave();
			s_p_next_measurement = NULL;
			if( s_tof_temp >= 0  )
			{
				tof_diff(NULL);
				if( s_tof_temp_count == 1 )
					s_p_next_measurement = max3510x_temperature;
			}
			else if( s_tof_temp < 0 )
			{
				max3510x_temperature(NULL);
				if( s_tof_temp_count == -1)
					s_p_next_measurement = tof_diff;
			}
			s_response_pending = true;
		}
		else if( s_p_next_measurement )
		{
			s_p_next_measurement(NULL);
			s_p_next_measurement = NULL;
			s_response_pending = true;
		}
	}
	else if( s_flow_sampling_mode == flow_sampling_mode_event  )
	{
		if( s_last_flow_sampling_mode != flow_sampling_mode_event )
		{
			max3510x_event_timing(NULL,s_event_timing_mode);
		}
	}
	if( (s_last_flow_sampling_mode == flow_sampling_mode_invalid ||
		s_last_flow_sampling_mode == flow_sampling_mode_idle) &&
		(s_flow_sampling_mode != flow_sampling_mode_invalid && 
		 s_flow_sampling_mode != flow_sampling_mode_idle ) )
	{
		s_hitcount = MAX3510X_REG_TOF2_STOP(MAX3510X_READ_BITFIELD(NULL,TOF2,STOP));
		
	}
	s_last_flow_sampling_mode = s_flow_sampling_mode;
}

static void process_flow(uint16_t status)
{

	float_t t;
	uint8_t i;
	if( s_flow_sampling_mode == flow_sampling_mode_idle )
	{
		uui_report_tof_temp(status);
		return;
	}

	if( status & MAX3510X_REG_INTERRUPT_STATUS_TOF )
	{
		max3510x_tof_results_t tof_fixed;

		max3510x_read_tof_results( NULL, &tof_fixed );

		float_t up[MAX3510X_MAX_HITCOUNT];
		float_t down[MAX3510X_MAX_HITCOUNT];
		for(i=0;i<s_hitcount;i++)
		{
			up[i] = max3510x_fixed_to_float( &tof_fixed.up.hit[i] );
			down[i] = max3510x_fixed_to_float( &tof_fixed.down.hit[i] );
		}
		s_last_sample_time = board_elapsed_time( s_last_sample_time, &t );
		start_next_measurement( false );
		uui_report_results(&up[0], &down[0], t, s_hitcount, 0 );
	}
	if( status & MAX3510X_REG_INTERRUPT_STATUS_TEMP_EVTMG )
	{
		max3510x_register7_t temp_regs;
		max3510x_read_registers( NULL, MAX3510X_REG_TEMP_CYCLE_COUNT, (max3510x_register_t*)&temp_regs, sizeof(temp_regs) );
		start_next_measurement(false);
		float_t therm = max3510x_fixed_to_float((const max3510x_fixed_t*)&temp_regs.value[1]);
		float_t ref = max3510x_fixed_to_float((const max3510x_fixed_t*)&temp_regs.value[5]);
		float_t r = board_temp_sensor_resistance( therm, ref );
	}
	else if( status & MAX3510X_REG_INTERRUPT_STATUS_TE )
	{
		max3510x_register6_t temp_regs;
		max3510x_read_registers( NULL, MAX3510X_REG_T1INT, (max3510x_register_t*)&temp_regs, sizeof(temp_regs) );
		start_next_measurement(false);
		float_t therm = max3510x_fixed_to_float((const max3510x_fixed_t*)&temp_regs.value[0]);
		float_t ref = max3510x_fixed_to_float((const max3510x_fixed_t*)&temp_regs.value[4]);
		float_t r = board_temp_sensor_resistance( therm, ref );
		s_last_sample_time = board_elapsed_time( s_last_sample_time, &t );
	}

}


void flow_init(void)
{
	max3510x_reset(NULL);
	max3510x_wait_for_reset_complete(NULL);
	max3510x_registers_t *p_config = config_get_max3510x_regs();
	max3510x_write_config(NULL, p_config);
#ifdef MAX35104
	if( (MAX3510X_REG_GET( AFE1_AFE_BP, MAX3510X_ENDIAN(p_config->max35104_registers.afe1) ) == MAX3510X_REG_AFE1_AFE_BP_DISABLED ) && 
		(MAX3510X_REG_GET( AFE2_BP_BYPASS, MAX3510X_ENDIAN(p_config->max35104_registers.afe2) ) == MAX3510X_REG_AFE2_BP_BYPASS_DISABLED)  &&
		(MAX3510X_REG_GET( TOF1_DPL, MAX3510X_ENDIAN(p_config->common.tof1) ) >= MAX3510X_REG_TOF1_DPL_1MHZ ) )
	{
		// issue bandpass filter calibrate command only when necessary
		max3510x_bandpass_calibrate(NULL);
		board_wait_ms( 3 );	// wait for bandpass calibrate to complete.
	}	
#endif                                
	start_next_measurement(true);
}

static bool timeout_check( uint16_t status )
{
	// check for timeouts
	bool timeout = false;
	if( status & MAX3510X_REG_INTERRUPT_STATUS_TO )
	{
		if( status & MAX3510X_REG_INTERRUPT_STATUS_TOF )
		{
			// transducer possibly disconnected
			timeout = true;
			board_led( 0, true );
		}
		else
		{
			board_led( 0, false );
		}
		if( status & MAX3510X_REG_INTERRUPT_STATUS_TE )
		{
			// temperature sensor possibly disconnected
			timeout = true;
			board_led( 1, true );
		}
		else
		{
			board_led( 1, false );
		}
	}
	else
	{
			board_led( 0, false );
			board_led( 1, false );
	}
	return timeout;
}


void flow_event( uint32_t event )
{
	if( event & BOARD_EVENT_SYSTICK )
	{
		if( s_flow_sampling_mode == flow_sampling_mode_host )
		{
			start_next_measurement(true);
		}
	}

	if( event & BOARD_EVENT_MAX35104 )
	{
		uint16_t status = board_max3510x_interrupt_status();
		s_response_pending = false;
		if( timeout_check( status ) )
		{
			start_next_measurement(false);
		}
		else
		{
			process_flow(status);
		}
	}
}

flow_sampling_mode_t flow_get_sampling_mode( void )
{
	if( s_requested_flow_sampling_mode != flow_sampling_mode_invalid )
	{
		return s_requested_flow_sampling_mode;
	}
	return s_flow_sampling_mode;
}

void flow_set_sampling_frequency( float_t sampling_freq )
{
	s_sampling_freq = board_clock_set( sampling_freq );
}

float_t flow_get_sampling_frequency( void )
{
	return s_sampling_freq;
}

void flow_set_sampling_mode( flow_sampling_mode_t mode )
{
	if( s_response_pending )
	{
		s_requested_flow_sampling_mode = mode;
	}
	else
	{
		s_flow_sampling_mode = mode;
		start_next_measurement(true);
	}
}

void flow_set_sos_method( flow_sos_method_t method )
{
	s_sos_method = method;
}

flow_sos_method_t flow_get_sos_method( void )
{
	return s_sos_method;
}

void flow_set_tof_temp( int16_t tof_temp )
{
	s_tof_temp = tof_temp;
	s_tof_temp_count = 0;
}

int16_t flow_get_tof_temp( void )
{
	return s_tof_temp;
}

max3510x_event_timing_mode_t flow_get_event_timing_mode( void )
{
	return s_event_timing_mode;
}

void flow_set_event_timing_mode( max3510x_event_timing_mode_t mode )
{
	s_event_timing_mode = mode;
}
