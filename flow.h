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
 
#include "max3510x.h"

void flow_init(void);
void flow_event( uint32_t event );

typedef enum _flow_sos_method_t
{
	flow_sos_method_direct,
	flow_sos_method_ideal_air,
}
flow_sos_method_t;

typedef enum _flow_sampling_mode_t
{
	flow_sampling_mode_invalid,
	flow_sampling_mode_idle,
	flow_sampling_mode_event,
	flow_sampling_mode_host,
	flow_sampling_mode_max
}
flow_sampling_mode_t;

void flow_set_sampling_mode( flow_sampling_mode_t mode );
void flow_set_sampling_frequency( float_t sampling_frequency );
float_t flow_get_sampling_frequency(void);


flow_sampling_mode_t flow_get_sampling_mode( void );
void flow_set_sos_method( flow_sos_method_t method );
flow_sos_method_t flow_get_sos_method( void );
int16_t flow_get_tof_temp( void );
void flow_set_tof_temp( int16_t tof_temp );
max3510x_event_timing_mode_t flow_get_event_timing_mode( void );
void flow_set_event_timing_mode( max3510x_event_timing_mode_t mode );
