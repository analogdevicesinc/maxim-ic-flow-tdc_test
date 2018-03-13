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
#include "board.h"
#include "uui.h"
#include "config.h"
#include "flow.h"

#include <tmr.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>


#define COMMAND_HISTORY_COUNT	4
#define COMMAND_SIZE			32

static char s_command_history[COMMAND_HISTORY_COUNT][COMMAND_SIZE];
static uint8_t s_command_ndx;
static uint8_t s_command_read_ndx;

static bool 		s_first;
static float_t 		s_accumulation_time;
static float_t		s_volume_last;
static float_t		s_volume_first;
static bool			s_output;
static bool 		s_results_report;
static uint32_t 	s_last_report_time;
static float_t 		s_time;

typedef enum _tdc_cmd_t
{
	tdc_cmd_none,
	tdc_cmd_tof_up,
	tdc_cmd_tof_down,
	tdc_cmd_tof_diff,
	tdc_cmd_temp,
	tdc_cmd_event_tof,
	tdc_cmd_event_temp,
	tdc_cmd_event_tof_temp,
	tdc_cmd_cal
}
tdc_cmd_t;

static tdc_cmd_t s_last_tdc_cmd;
static bool s_first_event;
static float_t s_time;

#define MAX3510X_CLOCK_FREQ	4000000
#define MAX3510X_VCC	3.3f

typedef struct _cmd_t
{
	const char * p_cmd;
	const char * p_help;
	bool (*p_set)(max3510x_t *,const char *);
	void (*p_get)(max3510x_t *);
}
cmd_t;

typedef struct _enum_t
{
	const char *p_tag;
	uint16_t	value;
}
enum_t;

static char s_rx_buf[COMMAND_SIZE];
static uint8_t s_rx_ndx;

#define DISPLAY_COUNT	100

static uint8_t 	s_display_count = DISPLAY_COUNT;



static const char * skip_space( const char *p)
{
	while(isspace(*p))
		p++;
	return p;
}

static bool get_enum_value( const char *p_arg, const enum_t * p_enum, uint8_t count, uint16_t *p_result )
{
	uint8_t i;
	for(i=0;i<count;i++)
	{
		if( !strcmp(p_arg,p_enum[i].p_tag) )
		{
			*p_result = p_enum[i].value;
			return true;
		}
	}
	return false;
}

static const char * get_enum_tag( const enum_t * p_enum, uint8_t count, uint16_t value )
{
	uint8_t i;
	for(i=0;i<count;i++)
	{
		if( value == p_enum[i].value )
		{
			return p_enum[i].p_tag;
		}
	}
	return NULL;
}

static bool binary(const char *p_arg, uint16_t *p_reg_value )
{
	if( *p_arg == '0' )
	{
		*p_reg_value = 0;
	}
	else if( *p_arg == '1' )
	{
		*p_reg_value = 1;
	}
	else
		return false;
	return true;
}

#if defined(MAX35104)

static const enum_t s_sfreq_enum[] =
{
	{ "100", MAX3510X_REG_SWITCHER1_SFREQ_100KHZ },
	{ "125", MAX3510X_REG_SWITCHER1_SFREQ_125KHZ },
	{ "166", MAX3510X_REG_SWITCHER1_SFREQ_166KHZ },
	{ "200", MAX3510X_REG_SWITCHER1_SFREQ_200KHZ },
};

static bool sfreq_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_sfreq_enum, ARRAY_COUNT(s_sfreq_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,SWITCHER1,SFREQ,result);
		return true;
	}
	return false;
}

static void sfreq_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER1,SFREQ);
	const char *p = get_enum_tag(s_sfreq_enum, ARRAY_COUNT(s_sfreq_enum),r);
	board_printf( "%skHz (%d)\r\n", p, r );
}

static const enum_t s_dreq_enum[] =
{
	{ "100", MAX3510X_REG_SWITCHER1_DREQ_100KHZ },
	{ "125", MAX3510X_REG_SWITCHER1_DREQ_125KHZ },
	{ "166", MAX3510X_REG_SWITCHER1_DREQ_166KHZ },
	{ "200", MAX3510X_REG_SWITCHER1_DREQ_200KHZ }
};

static bool dreq_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_dreq_enum, ARRAY_COUNT(s_dreq_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,SWITCHER1,DREQ,result);
		return true;
	}
	return false;
}

static void dreq_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER1,DREQ);
	const char *p = get_enum_tag(s_sfreq_enum, ARRAY_COUNT(s_sfreq_enum),r);
	board_printf( "%skHz (%d)\r\n", p, r );
}

static bool hreg_d_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;
	MAX3510X_WRITE_BITFIELD(p_max3510x,SWITCHER1,HREG_D,r ? MAX3510X_REG_SWITCHER1_HREG_D_DISABLED : MAX3510X_REG_SWITCHER1_HREG_D_ENABLED );
	return true;
}

static void hreg_d_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER1,HREG_D);
	board_printf("%s\r\n", (r==MAX3510X_REG_SWITCHER1_HREG_D_DISABLED) ? "regulator disabled (1)" : "regulator enabled (0)" );
}

static const float_t s_vpr_target[] = {27.0f, 25.2f, 23.4f, 21.6f, 19.2f, 17.4f, 15.6f, 13.2f, 11.4f, 9.0f, 7.2f, 5.4f };
static const float_t s_vp_target[] = {30.6f, 28.8f, 27.0f, 25.2f, 22.8f, 21.0f, 19.2f, 16.8f, 15.0f, 12.6f, 10.8f, 9.0f };
static const uint16_t s_vs_value[] = 
{
	MAX3510X_REG_SWITCHER1_VS_27V0,
	MAX3510X_REG_SWITCHER1_VS_25V2,
	MAX3510X_REG_SWITCHER1_VS_23V4,
	MAX3510X_REG_SWITCHER1_VS_21V6,
	MAX3510X_REG_SWITCHER1_VS_19V2,
	MAX3510X_REG_SWITCHER1_VS_17V4,
	MAX3510X_REG_SWITCHER1_VS_15V6,
	MAX3510X_REG_SWITCHER1_VS_13V2,
	MAX3510X_REG_SWITCHER1_VS_11V4,
	MAX3510X_REG_SWITCHER1_VS_9V0,
	MAX3510X_REG_SWITCHER1_VS_7V2,
	MAX3510X_REG_SWITCHER1_VS_5V4_60
};

static bool vs_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint8_t i;
	uint16_t reg = 0;
	float_t v = strtof(p_arg,NULL);
	if( v < 5.4f || v > 27.0f  )
		return false;
	for(i=0;i<ARRAY_COUNT(s_vp_target);i++)
	{
		if( v >= s_vpr_target[i] )
		{
			reg = s_vs_value[i];
			board_printf("vp = %f, vpr = %f\r\n", s_vp_target[i], s_vpr_target[i] );
			break;
		}
	}
	MAX3510X_WRITE_BITFIELD(p_max3510x,SWITCHER1,VS,reg);
	return true;
}

static void vs_get( max3510x_t *p_max3510x )
{
	uint8_t i;
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER1,VS);
	for(i=0;i<ARRAY_COUNT(s_vs_value);i++)
	{
		if( s_vs_value[i] == r )
		{
			board_printf("%f\r\n", s_vp_target[i] );
			return;
		}
	}
	// not all values are represented by this interface
	board_printf("%.2fV (%d)\r\n", s_vp_target[i-1], r);
}

static const enum_t s_lt_n_enum[] =
{
	{ "loop", MAX3510X_REG_SWITCHER2_LT_N_LOOP },
	{ "200", MAX3510X_REG_SWITCHER2_LT_N_0V2 },
	{ "400", MAX3510X_REG_SWITCHER2_LT_N_0V4 },
	{ "800", MAX3510X_REG_SWITCHER2_LT_N_0V8 },
	{ "1600", MAX3510X_REG_SWITCHER2_LT_N_1V6 }
};

static bool lt_n_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_lt_n_enum, ARRAY_COUNT(s_lt_n_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,SWITCHER2,LT_N,result);
		return true;
	}
	return false;
}

static void lt_n_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER2,LT_N);
	const char *p = get_enum_tag( s_lt_n_enum, ARRAY_COUNT(s_lt_n_enum), r );
	if( r == MAX3510X_REG_SWITCHER2_LT_N_LOOP )
	{
		board_printf("%s (%d)\r\n", p, r);
	}
	else
	{
		board_printf("%smV (%d)\r\n", p, r );
	}

}

static const enum_t s_lt_s_enum[] = 
{
	{ "none", MAX3510X_REG_SWITCHER2_LT_S_NO_LIMIT },
	{ "200", MAX3510X_REG_SWITCHER2_LT_S_0V2 },
	{ "400", MAX3510X_REG_SWITCHER2_LT_S_0V4 },
	{ "800", MAX3510X_REG_SWITCHER2_LT_S_0V8 },
	{ "1600", MAX3510X_REG_SWITCHER2_LT_S_1V6 }
};


static bool lt_s_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_lt_s_enum, ARRAY_COUNT(s_lt_s_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,SWITCHER2,LT_S, result);
		return true;
	}
	return false;
}

static void lt_s_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER2,LT_S);
	const char *p = get_enum_tag( s_lt_s_enum, ARRAY_COUNT(s_lt_s_enum), r );
	if( r == MAX3510X_REG_SWITCHER2_LT_N_LOOP )
	{
		board_printf("%s (%d)\r\n", p, r);
	}
	else
	{
		board_printf("%smV (%d)\r\n", p, r );
	}
}

static bool st_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t us = atoi(p_arg);

	if(  us < MAX3510X_REG_SWITCHER2_ST_US_MIN )
		us = MAX3510X_REG_SWITCHER2_ST_US_MIN;
	else if( us > MAX3510X_REG_SWITCHER2_ST_US_MAX )
		us = MAX3510X_REG_SWITCHER2_ST_US_MAX;

	us += (MAX3510X_REG_SWITCHER2_ST_US_MIN-1);	// round up 

	uint16_t r = MAX3510X_REG_SWITCHER2_ST_US( us );
	MAX3510X_WRITE_BITFIELD(p_max3510x,SWITCHER2,ST, r );

	us = MAX3510X_REG_SWITCHER2_ST(r);
	board_printf("st = %dus\r\n", us );
	return true;
}

static void st_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER2,ST);
	uint16_t us = MAX3510X_REG_SWITCHER2_ST(r);
	board_printf("%dus (%d)\r\n", us, r );
}


static bool lt_50d_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, SWITCHER2, LT_50D, r ? MAX3510X_REG_SWITCHER2_LT_50D_UNTRIMMED : MAX3510X_REG_SWITCHER2_LT_50D_TRIMMED );
	return true;
}

static void lt_50d_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER2,LT_50D);
	board_printf("%s (%d)\r\n", r ? "untrimmed" : "trimmed", r );
}

static bool pecho_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, SWITCHER2, PECHO, r ? MAX3510X_REG_SWITCHER2_PECHO_ENABLED : MAX3510X_REG_SWITCHER2_PECHO_DISABLED );
	return true;
}

static void pecho_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,SWITCHER2,PECHO);
	board_printf("%s (%d)\r\n", r ? "echo mode" : "tof mode", r );
}

static bool afe_bp_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, AFE1, AFE_BP, r ? MAX3510X_REG_AFE1_AFE_BP_ENABLED : MAX3510X_REG_AFE1_AFE_BP_DISABLED );
	return true;
}

static void afe_bp_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,AFE1,AFE_BP);
	board_printf("%s (%d)\r\n", r ? "afe bypassed" : "afe enabled", r);
}

static bool sd_en_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, AFE1, SD_EN, r ? MAX3510X_REG_AFE1_SD_EN_ENABLED : MAX3510X_REG_AFE1_SD_EN_DISABLED );
	return true;
}

static void sd_en_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,AFE1,SD_EN);
	board_printf("%s (%d)\r\n", r ? "single ended drive" : "differential drive", r);
}

static const enum_t s_afeout_enum[] =
{
	{ "disabled", MAX3510X_REG_AFE1_AFEOUT_DISABLED },
	{ "bandpass", MAX3510X_REG_AFE1_AFEOUT_BANDPASS },
	{ "pga", MAX3510X_REG_AFE1_AFEOUT_PGA },
	{ "fga", MAX3510X_REG_AFE1_AFEOUT_FIXED }
};

static bool afeout_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_afeout_enum, ARRAY_COUNT(s_afeout_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,AFE1,AFEOUT, result);
		return true;
	}
	return false;
}

static void afeout_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,AFE1,AFEOUT);
	const char *p = get_enum_tag( s_afeout_enum, ARRAY_COUNT(s_afeout_enum), r );
	board_printf("%s (%d)\r\n", p, r );
}

static bool _4m_bp_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, AFE2, 4M_BP, r ? MAX3510X_REG_AFE2_4M_BP_ENABLED : MAX3510X_REG_AFE2_4M_BP_DISABLED );
	return true;
}

static void _4m_bp_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,AFE2,4M_BP);
	board_printf("%s (%d)\r\n", r ? "CMOS clock input" : "oscillator", r);
}

static bool f0_set( max3510x_t *p_max3510x, const char *p_arg  )
{
	uint16_t r = atoi(p_arg);
	if( r <= MAX3510X_REG_AFE2_F0_MAX )
	{
		MAX3510X_WRITE_BITFIELD( p_max3510x, AFE2, F0, r );
		return true;
	}
	return false;
}

static void f0_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,AFE2,F0);
	board_printf("%d\r\n", r );
}

static bool pga_set( max3510x_t *p_max3510x, const char *p_arg )
{
	float_t gain_db = strtof(p_arg,NULL);
	const float_t min_gain_db = MAX3510X_REG_AFE2_PGA((float_t)MAX3510X_REG_AFE2_PGA_DB_MIN);
	const float_t max_gain_db = MAX3510X_REG_AFE2_PGA((float_t)MAX3510X_REG_AFE2_PGA_DB_MAX);
	if( gain_db < min_gain_db || gain_db > max_gain_db )
	{
		return false;
	}
	uint16_t r = (uint16_t)MAX3510X_REG_AFE2_PGA_DB(gain_db);
	MAX3510X_WRITE_BITFIELD( p_max3510x, AFE2, PGA, r );
	gain_db = MAX3510X_REG_AFE2_PGA(r);
	board_printf("pga = %.2fdB (%d)\r\n", gain_db, r );
	return true;
}

static void pga_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD( p_max3510x, AFE2, PGA );
	board_printf("%.2fdB(%d)\r\n", MAX3510X_REG_AFE2_PGA((float_t)r), r );
}

static const enum_t s_lowq_enum[] =
{
	{ "4.2", MAX3510X_REG_AFE2_LOWQ_4_2KHZ },
	{ "5.3", MAX3510X_REG_AFE2_LOWQ_5_3KHZ },
	{ "7.4", MAX3510X_REG_AFE2_LOWQ_7_4KHZ },
	{ "12", MAX3510X_REG_AFE2_LOWQ_12KHZ }
};

static bool lowq_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_lowq_enum, ARRAY_COUNT(s_lowq_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,AFE2,LOWQ, result);
		return true;
	}
	return false;
}

static void lowq_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,AFE2,LOWQ);
	const char *p = get_enum_tag( s_lowq_enum, ARRAY_COUNT(s_lowq_enum), r );
	board_printf("%skHz/kHz (%d)\r\n", p, r );
}

static bool bp_bp_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, AFE2, BP_BYPASS, r ? MAX3510X_REG_AFE2_BP_BYPASS_ENABLED : MAX3510X_REG_AFE2_BP_BYPASS_DISABLED );
	return true;
}

static void bp_bp_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,AFE2,BP_BYPASS);
	board_printf("filter %s (%d)\r\n", r==MAX3510X_REG_AFE2_BP_BYPASS_ENABLED ? "bypassed" : "enabled", r);
}

#endif //  MAX35104

static bool pl_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r = atoi(p_arg);
	if( r > MAX3510X_REG_TOF1_PL_MAX  )
	{
		return false;
	}
	MAX3510X_WRITE_BITFIELD( p_max3510x, TOF1, PL, r );
	return true;
}

static void pl_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF1,PL);
	board_printf("%d pulses\r\n", r);
}

static bool dpl_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	int32_t freq = atoi(p_arg);
	
	uint8_t i;
	int32_t f;
	int32_t min_delta = 1000000;
	int32_t nearest = 0;
	
	static const int32_t max_freq = (MAX3510X_REG_TOF1_DPL(MAX3510X_CLOCK_FREQ,MAX3510X_REG_TOF1_DPL_MIN)/1000);
	static const int32_t min_freq = (MAX3510X_REG_TOF1_DPL(MAX3510X_CLOCK_FREQ,MAX3510X_REG_TOF1_DPL_MAX)/1000);
	
	if( freq < min_freq || freq > max_freq )
	{
		return false;
	}

	for(i=MAX3510X_REG_TOF1_DPL_1MHZ;i<=MAX3510X_REG_TOF1_DPL_125KHZ;i++)
	{
		f = MAX3510X_REG_TOF1_DPL(MAX3510X_CLOCK_FREQ/1000,i);
		if( abs(freq - f) < min_delta )
		{
			nearest = f;
			min_delta = abs(freq-f);
		}
	}
	r = MAX3510X_REG_TOF1_DPL_HZ(MAX3510X_CLOCK_FREQ/1000,nearest);
	MAX3510X_WRITE_BITFIELD( p_max3510x, TOF1, DPL, r );
	board_printf("dpl = %dkHz (%d)\r\n", nearest, r );
	return true;
}


static void dpl_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF1,DPL);
	if( r < MAX3510X_REG_TOF1_DPL_MIN|| r > MAX3510X_REG_TOF1_DPL_MAX )
	{
		board_printf("invalid (%d)\r\n", r );
	}
	else
	{
		board_printf("%dkHz (%d)\r\n", MAX3510X_REG_TOF1_DPL((MAX3510X_CLOCK_FREQ/1000),r), r);
	}
}

static bool stop_pol_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, TOF1, STOP_POL, r ? MAX3510X_REG_TOF1_STOP_POL_NEG_EDGE : MAX3510X_REG_TOF1_STOP_POL_POS_EDGE );
	return true;
}

static void stop_pol_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF1,STOP_POL);
	board_printf("%s (%d)\r\n", (r==MAX3510X_REG_TOF1_STOP_POL_NEG_EDGE) ? "negative" : "positive", r );
}

static bool stop_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r, hitcount = atoi(p_arg);
	if( hitcount > MAX3510X_REG_TOF2_STOP_MAX || hitcount < MAX3510X_REG_TOF2_STOP_MIN )
		return false;
	r = MAX3510X_REG_TOF2_STOP_C(hitcount);
	MAX3510X_WRITE_BITFIELD(p_max3510x,TOF2,STOP, r );
	return true;
}

static void stop_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF2,STOP);
	uint16_t hitcount = MAX3510X_REG_TOF2_STOP(r);
	board_printf("hitcount = %d (%d)\r\n", hitcount, r );
}

static bool t2wv_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r = atoi(p_arg);
	if( r > MAX3510X_REG_TOF2_TW2V_MAX || r < MAX3510X_REG_TOF2_TW2V_MIN )
		return false;
	MAX3510X_WRITE_BITFIELD(p_max3510x,TOF2,TW2V,r);
	return true;
}

static void t2wv_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF2,TW2V);
	board_printf("wave %d\r\n",r);
}

static const enum_t s_tof_cyc_enum[] =
{
	{ "0", MAX3510X_REG_TOF2_TOF_CYC_0US },
	{ "122", MAX3510X_REG_TOF2_TOF_CYC_122US },
	{ "244", MAX3510X_REG_TOF2_TOF_CYC_244US },
	{ "488", MAX3510X_REG_TOF2_TOF_CYC_488US },
	{ "732", MAX3510X_REG_TOF2_TOF_CYC_732US },
	{ "976", MAX3510X_REG_TOF2_TOF_CYC_976US },
	{ "16650", MAX3510X_REG_TOF2_TOF_CYC_16_65MS },
	{ "19970", MAX3510X_REG_TOF2_TOF_CYC_19_97MS }
};

static bool tof_cyc_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_tof_cyc_enum, ARRAY_COUNT(s_tof_cyc_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,TOF2,TOF_CYC, result);
		return true;
	}
	return false;
}

static void tof_cyc_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF2,TOF_CYC);
	const char *p = get_enum_tag( s_tof_cyc_enum, ARRAY_COUNT(s_tof_cyc_enum), r );
	board_printf("%sms (%d)\r\n", p, r );
}

static const enum_t s_timout_enum[] =
{
	{ "128", MAX3510X_REG_TOF2_TIMOUT_128US },
	{ "256", MAX3510X_REG_TOF2_TIMOUT_256US },
	{ "512", MAX3510X_REG_TOF2_TIMOUT_512US },
	{ "1024", MAX3510X_REG_TOF2_TIMOUT_1024US },
	{ "2048", MAX3510X_REG_TOF2_TIMOUT_2048US },
	{ "4096", MAX3510X_REG_TOF2_TIMOUT_4096US },
	{ "8192", MAX3510X_REG_TOF2_TIMOUT_8192US },
	{ "16384", MAX3510X_REG_TOF2_TIMOUT_16384US }
};

static bool timout_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_timout_enum, ARRAY_COUNT(s_timout_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,TOF2,TIMOUT, result);
		return true;
	}
	return false;
}

static void timout_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF2,TIMOUT);
	const char *p = get_enum_tag( s_timout_enum, ARRAY_COUNT(s_timout_enum), r );
	board_printf("%sms (%d)\r\n", p, r );
}

#if !defined(MAX35102)

static bool hitwv_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint8_t i;
	uint8_t hw[MAX3510X_MAX_HITCOUNT];
	const char *p = p_arg;

	uint16_t tw2v = MAX3510X_READ_BITFIELD(p_max3510x,TOF2,TW2V);

	for(i=0;i<MAX3510X_MAX_HITCOUNT;i++)
	{
		if( !(*p) )
			break;
		if( !isdigit(*p) )
			return false;
		hw[i] = atoi(p);
		if(  hw[i] < MAX3510X_HITWV_MIN || hw[i] > MAX3510X_HITWV_MAX )
			return false;
		if( !i && hw[i] <= tw2v )
		{
			board_printf("error:  the first hit wave number must be greater than than the t2 wave number\r\n");
		}
		if( i && (hw[i] <= hw[i - 1]) )
		{
			board_printf("error: each hit value must be greater than the previous\r\n");
			return false;
		}
		if(i==MAX3510X_MAX_HITCOUNT-1)
		{
			i++;
			break;
		}
		while(*p && *p != ',' )
			p++;
		p = skip_space( p );
		if( !(*p) )
		{
			i++;
			break;
		}
		if( *p != ',' )
			return false;
		p = skip_space( p + 1 );
	}
	// when given a partial list of hit values, assume the remaning hit values are sequential and adjacent.
	for(;i<MAX3510X_MAX_HITCOUNT;i++)
	{
		hw[i] = hw[i-1]+1;
	}
	max3510x_set_hitwaves(p_max3510x,&hw[0]);
	return true;
}

static void hitwv_get( max3510x_t *p_max3510x )
{
	uint8_t hw[MAX3510X_MAX_HITCOUNT];
	max3510x_get_hitwaves( p_max3510x, &hw[0] );
	board_printf("%d, %d, %d, %d, %d, %d\r\n", hw[0], hw[1], hw[2], hw[3], hw[4], hw[5] );
}

static bool c_offsetupr_set( max3510x_t *p_max3510x, const char *p_arg )
{
	int8_t r = atoi(p_arg);
	MAX3510X_WRITE_BITFIELD(p_max3510x,TOF6,C_OFFSETUPR,r);
	return true;
}


static void c_offsetupr_get( max3510x_t *p_max3510x )
{
	int8_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF6,C_OFFSETUPR);
	board_printf("%d\r\n", r);
}

#endif // #if !defined(MAX35102)

static bool c_offsetup_set( max3510x_t *p_max3510x, const char *p_arg )
{
	int8_t r = atoi(p_arg);
	MAX3510X_WRITE_BITFIELD(p_max3510x,TOF6,C_OFFSETUP,r);
	return true;
}

static void c_offsetup_get( max3510x_t *p_max3510x )
{
	int8_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF6,C_OFFSETUP);
	board_printf("%d\r\n", r);
}

#if !defined(MAX35102)

static bool c_offsetdnr_set( max3510x_t *p_max3510x, const char *p_arg )
{
	int8_t r = atoi(p_arg);
	MAX3510X_WRITE_BITFIELD(p_max3510x,TOF7,C_OFFSETDNR,r);
	return true;
}

static void c_offsetdnr_get( max3510x_t *p_max3510x )
{
	int8_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF7,C_OFFSETDNR);
	board_printf("%d\r\n", r);
}

#endif

static bool c_offsetdn_set( max3510x_t *p_max3510x, const char *p_arg )
{
	int8_t r = atoi(p_arg);
	MAX3510X_WRITE_BITFIELD(p_max3510x,TOF7,C_OFFSETDN,r);
	return true;
}

static void c_offsetdn_get( max3510x_t *p_max3510x )
{
	int8_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF7,C_OFFSETDN);
	board_printf("%d\r\n", r);
}

static void tdf_get( max3510x_t *p_max3510x )
{
	uint16_t tdf = MAX3510X_READ_BITFIELD(p_max3510x, EVENT_TIMING_1, TDF);
#if defined(MAX35103)
        board_printf("%.2fs (%d)\r\n", (float_t)MAX3510X_REG_EVENT_TIMING_1_TDF((float_t)tdf,0), tdf);
#else
	board_printf("%.2fs (%d)\r\n", (float_t)MAX3510X_REG_EVENT_TIMING_1_TDF((float_t)tdf), tdf);
#endif
}

static bool tdf_set( max3510x_t *p_max3510x, const char *p_arg )
{
	float_t period = strtof(p_arg,NULL);
	
#if defined(MAX35103)
	uint16_t tdf = roundf(MAX3510X_REG_EVENT_TIMING_1_TDF_S(period,0));
#else
        uint16_t tdf = roundf(MAX3510X_REG_EVENT_TIMING_1_TDF_S(period));
#endif
	if( tdf > MAX3510X_REG_EVENT_TIMING_1_TDF_MAX )
		return false;
	MAX3510X_WRITE_BITFIELD(p_max3510x, EVENT_TIMING_1, TDF, tdf );
	tdf_get(p_max3510x);
	return true;
}

static void tdm_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD( p_max3510x, EVENT_TIMING_1, TDM );
	board_printf("%d (%d)\r\n", MAX3510X_REG_EVENT_TIMING_1_TDM(r), r );
}


static bool tdm_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t count = atoi(p_arg);
	if(  count < MAX3510X_REG_EVENT_TIMING_1_TDM_MIN || count > MAX3510X_REG_EVENT_TIMING_1_TDM_MAX )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, EVENT_TIMING_1, TDM, MAX3510X_REG_EVENT_TIMING_1_TDM_C(count) );
	tdm_get(p_max3510x);
	return true;
}

static bool tmf_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint8_t i, min_ndx = 0;
	float_t p = 0.0f, delta;
	float_t delta_min = 1000.0f;
	float_t period = strtof(p_arg,NULL);
#ifdef MAX35103
	if( period < MAX3510X_REG_EVENT_TIMING_1_TMF_S(0,MAX3510X_REG_EVENT_TIMING_1_TMF_MIN) ||
		period > MAX3510X_REG_EVENT_TIMING_1_TMF_S(0,MAX3510X_REG_EVENT_TIMING_1_TMF_MAX) )
	{
		return false;
	}
#else
	if( period < MAX3510X_REG_EVENT_TIMING_1_TMF_S(MAX3510X_REG_EVENT_TIMING_1_TMF_MIN) ||
		period > MAX3510X_REG_EVENT_TIMING_1_TMF_S(MAX3510X_REG_EVENT_TIMING_1_TMF_MAX) )
	{
		return false;
	}
#endif
	for( i = MAX3510X_REG_EVENT_TIMING_1_TMF_MIN; i <= MAX3510X_REG_EVENT_TIMING_1_TMF_MAX; i++ )
	{
		p = i * 1.0f;
		delta = fabsf(period - p);

		
		if( delta < delta_min )
		{
			delta_min = delta;
			min_ndx = i;
		}
	}
	MAX3510X_WRITE_BITFIELD(p_max3510x, EVENT_TIMING_1, TMF, min_ndx );

#ifdef MAX35103
	board_printf("tmf = %.2fs (%d)\r\n", MAX3510X_REG_EVENT_TIMING_1_TMF(0,min_ndx), min_ndx);
#else
    board_printf("tmf = %.2fs (%d)\r\n", MAX3510X_REG_EVENT_TIMING_1_TMF(min_ndx), min_ndx);
#endif
	return true;
}

static void tmf_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x, EVENT_TIMING_1, TMF);

#ifdef MAX35103
	board_printf("%ds (%d)\r\n", MAX3510X_REG_EVENT_TIMING_1_TMF(0,r), r );
#else
	board_printf("%ds (%d)\r\n", MAX3510X_REG_EVENT_TIMING_1_TMF(r), r );
#endif
}


static bool tmm_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t count = atoi(p_arg);
	if(  count < MAX3510X_REG_EVENT_TIMING_2_TMM_MIN || count > MAX3510X_REG_EVENT_TIMING_2_TMM_MAX )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, EVENT_TIMING_2, TMM, MAX3510X_REG_EVENT_TIMING_2_TMM_C(count) );
	return true;
}
static void tmm_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD( p_max3510x, EVENT_TIMING_2, TMM );
	board_printf("%d (%d)\r\n", MAX3510X_REG_EVENT_TIMING_2_TMM(r), r );
}

static bool cal_use_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;
	MAX3510X_WRITE_BITFIELD(p_max3510x,EVENT_TIMING_2,CAL_USE,r ? MAX3510X_REG_EVENT_TIMING_2_CAL_USE_ENABLED : MAX3510X_REG_EVENT_TIMING_2_CAL_USE_DISABLED );
	return true;
}

static void cal_use_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,EVENT_TIMING_2,CAL_USE);
	board_printf("%s\r\n", r ? "use calibration data (1)" : "no calibration (0)");
}

static const enum_t s_cal_cfg_enum[] =
{
	{ "disabled", MAX3510X_REG_EVENT_TIMING_2_CAL_CFG_DISABLED },
	{ "cc", MAX3510X_REG_EVENT_TIMING_2_CAL_CFG_CYCLE_CYCLE },
	{ "cs", MAX3510X_REG_EVENT_TIMING_2_CAL_CFG_CYCLE_SEQ },
	{ "sc", MAX3510X_REG_EVENT_TIMING_2_CAL_CFG_SEQ_CYCLE },
	{ "ss", MAX3510X_REG_EVENT_TIMING_2_CAL_CFG_SEQ_SEQ },
};

static bool cal_cfg_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_cal_cfg_enum, ARRAY_COUNT(s_cal_cfg_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,EVENT_TIMING_2,CAL_CFG, result);
		return true;
	}
	return false;
}

static void cal_cfg_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,EVENT_TIMING_2,CAL_CFG);
	const char *p = get_enum_tag( s_cal_cfg_enum, ARRAY_COUNT(s_cal_cfg_enum), r );
	board_printf("%s (%d)\r\n", p, r );
}


static bool precyc_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r = atoi(p_arg);
	if( r > MAX3510X_REG_EVENT_TIMING_2_PRECYC_MAX )
		return false;
	MAX3510X_WRITE_BITFIELD(p_max3510x,EVENT_TIMING_2,PRECYC,r);
	return true;
}

static void precyc_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,EVENT_TIMING_2,PRECYC);
	board_printf("%d cycles\r\n",r);
}

static const enum_t s_portcyc_enum[] = 
{
	{ "128", MAX3510X_REG_TEMPERATURE_PORTCYC_128US },
	{ "256", MAX3510X_REG_TEMPERATURE_PORTCYC_256US },
	{ "284", MAX3510X_REG_TEMPERATURE_PORTCYC_384US },
	{ "512", MAX3510X_REG_TEMPERATURE_PORTCYC_512US }
};

static bool portcyc_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_portcyc_enum, ARRAY_COUNT(s_portcyc_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,EVENT_TIMING_2,PORTCYC, result);
		return true;
	}
	return false;
}

static void portcyc_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,EVENT_TIMING_2,PORTCYC);
	const char *p = get_enum_tag( s_portcyc_enum, ARRAY_COUNT(s_portcyc_enum), r );
	board_printf("%sus (%d)\r\n", p, r );
}


static bool dly_set( max3510x_t *p_max3510x, const char *p_arg )
{
	float_t dly = strtof(p_arg,NULL);
	int16_t r = MAX3510X_REG_TOF_MEASUREMENT_DELAY_DLY_US(dly);

	if( r < MAX3510X_REG_TOF_MEASUREMENT_DELAY_DLY_MIN )
		return false;

	MAX3510X_WRITE_BITFIELD(p_max3510x,TOF_MEASUREMENT_DELAY,DLY,r);

	board_printf("%.2fus (%d)\r\n", (float_t)MAX3510X_REG_TOF_MEASUREMENT_DELAY_DLY(r), r );

	return true;
}

static void dly_get( max3510x_t *p_max3510x )
{
	int16_t r = MAX3510X_READ_BITFIELD(p_max3510x,TOF_MEASUREMENT_DELAY,DLY);
	board_printf("%.2fus (%d)\r\n", (float_t)MAX3510X_REG_TOF_MEASUREMENT_DELAY_DLY(r), r );
}

static bool cmp_en_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, CALIBRATION_CONTROL, CMP_EN, r ? MAX3510X_REG_CALIBRATION_CONTROL_CMP_EN_ENABLED : MAX3510X_REG_CALIBRATION_CONTROL_CMP_EN_DISABLED );
	return true;
}

static void cmp_en_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,CALIBRATION_CONTROL,CMP_EN);
	board_printf("%s\r\n", r ? "enable CMPOUT/UP_DN pin (1)" : "disable CMPOUT/UP_DN pin (0)");
}

static bool cmp_sel_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, CALIBRATION_CONTROL, CMP_SEL, r ? MAX3510X_REG_CALIBRATION_CONTROL_CMP_SEL_CMP_EN : MAX3510X_REG_CALIBRATION_CONTROL_CMP_SEL_UP_DN );
	return true;
}

static void cmp_sel_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,CALIBRATION_CONTROL,CMP_SEL);
	board_printf("%s (%d)\r\n", r==MAX3510X_REG_CALIBRATION_CONTROL_CMP_EN_ENABLED ? "CMPOUT" : "UP_DN", r);
}

static bool et_cont_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, CALIBRATION_CONTROL, ET_CONT, r ? MAX3510X_REG_CALIBRATION_CONTROL_ET_CONT_ENABLED : MAX3510X_REG_CALIBRATION_CONTROL_ET_CONT_DISABLED );
	return true;
}

static void et_cont_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,CALIBRATION_CONTROL,ET_CONT);
	board_printf("%s (%d)\r\n", r==MAX3510X_REG_CALIBRATION_CONTROL_ET_CONT_ENABLED ? "continuous" : "one-shot", r );
}

static bool cont_int_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, CALIBRATION_CONTROL, CONT_INT, r ? MAX3510X_REG_CALIBRATION_CONTROL_ET_CONT_ENABLED : MAX3510X_REG_CALIBRATION_CONTROL_CONT_INT_DISABLED );
	return true;
}

static void cont_int_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,CALIBRATION_CONTROL,CONT_INT);
	board_printf("%s (%d)\r\n", r==MAX3510X_REG_CALIBRATION_CONTROL_ET_CONT_ENABLED ? "continuous" : "one-shot", r );
}

static const enum_t s_clk_s_enum[] = 
{
	{ "488", MAX3510X_REG_CALIBRATION_CONTROL_CLK_S_488US },
	{ "1460", MAX3510X_REG_CALIBRATION_CONTROL_CLK_S_1046US },
	{ "2930", MAX3510X_REG_CALIBRATION_CONTROL_CLK_S_2930US },
	{ "3900", MAX3510X_REG_CALIBRATION_CONTROL_CLK_S_3900US },
	{ "5130", MAX3510X_REG_CALIBRATION_CONTROL_CLK_S_5130US },
	{ "continuous", MAX3510X_REG_CALIBRATION_CONTROL_CLK_S_CONTINUOUS }
};

static bool clk_s_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_clk_s_enum, ARRAY_COUNT(s_clk_s_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,CALIBRATION_CONTROL,CLK_S, result);
		return true;
	}
	return false;
}

static void clk_s_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,CALIBRATION_CONTROL,CLK_S);
	const char *p = get_enum_tag( s_clk_s_enum, ARRAY_COUNT(s_clk_s_enum), r );
	board_printf("%s (%d)\r\n", p, r );
}

static bool cal_period_set( max3510x_t *p_max3510x, const char *p_arg )
{
	float_t period = strtof(p_arg,NULL);
	uint16_t r = (uint16_t)MAX3510X_REG_CALIBRATION_CONTROL_CAL_PERIOD_US(period);
	if( r > MAX3510X_REG_CALIBRATION_CONTROL_CAL_PERIOD_MAX )
	{
		return false;
	}

	MAX3510X_WRITE_BITFIELD(p_max3510x, CALIBRATION_CONTROL, CAL_PERIOD, r );

	period = MAX3510X_REG_CALIBRATION_CONTROL_CAL_PERIOD(r);
	board_printf( "%.2fus (%d)\r\n", (float_t)period, r );
	return true;
}

static void cal_period_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x, CALIBRATION_CONTROL, CAL_PERIOD);

	board_printf("%.2fus (%d)\r\n", (float_t)MAX3510X_REG_CALIBRATION_CONTROL_CAL_PERIOD(r), r );
}

static bool _32k_bp_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, RTC, 32K_BP, r ? MAX3510X_REG_RTC_32K_BP_ENABLED : MAX3510X_REG_RTC_32K_BP_DISABLED );
	return true;
}

static void _32k_bp_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,RTC,32K_BP);
	board_printf("%s (%d)\r\n", r==MAX3510X_REG_RTC_32K_BP_ENABLED ? "CMOS clock input" : "oscillator", r);
}

static bool _32k_en_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;
#ifdef max35103evkit2
    r = 0;  // can't enable 32K signal on this board due to 32KIN over-voltage.  3.3V signal on a 1.8V input
#endif

	MAX3510X_WRITE_BITFIELD( p_max3510x, RTC, 32K_EN, r ? MAX3510X_REG_RTC_32K_EN_ENABLED : MAX3510X_REG_RTC_32K_EN_DISABLED );
	return true;
}

static void _32k_en_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,RTC,32K_EN);
	board_printf("%s (%d)\r\n", r==MAX3510X_REG_RTC_32K_EN_ENABLED ? "enabled" : "disabled", r);
}

static bool eosc_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, RTC, EOSC, r ? MAX3510X_REG_RTC_EOSC_ENABLED : MAX3510X_REG_RTC_EOSC_DISABLED );
	return true;
}

static void eosc_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,RTC,EOSC);
	board_printf("%s (%d)\r\n", r==MAX3510X_REG_RTC_EOSC_ENABLED ? "enabled" : "disabled", r);
}

static const enum_t s_am_enum[] = 
{
	{ "none", MAX3510X_REG_RTC_AM_NONE },
	{ "minutes", MAX3510X_REG_RTC_AM_MINUTES },
	{ "hours", MAX3510X_REG_RTC_AM_HOURS },
	{ "both", MAX3510X_REG_RTC_AM_HOURS_MINUTES },
};

static bool am_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_am_enum, ARRAY_COUNT(s_am_enum), &result ) )
	{
		MAX3510X_WRITE_BITFIELD(p_max3510x,RTC,AM, result);
		return true;
	}
	return false;
}

static void am_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,RTC,AM);
	const char *p = get_enum_tag( s_am_enum, ARRAY_COUNT(s_am_enum), r );
	board_printf("%s (%d)\r\n", p, r );
}

static bool wf_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, RTC, WF, r ? MAX3510X_REG_RTC_WF_SET : MAX3510X_REG_RTC_WF_CLEAR );
	return true;
}

static void wf_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,RTC,WF);
	board_printf("%s (%d)\r\n", r==MAX3510X_REG_RTC_WF_SET ? "set" : "clear", r );
}

static bool wd_en_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t r;
	if( !binary( p_arg, &r ) )
		return false;

	MAX3510X_WRITE_BITFIELD( p_max3510x, RTC, WD_EN, r ? MAX3510X_REG_RTC_WD_EN_ENABLED : MAX3510X_REG_RTC_WD_EN_DISABLED );
	return true;
}

static void wd_en_get( max3510x_t *p_max3510x )
{
	uint16_t r = MAX3510X_READ_BITFIELD(p_max3510x,RTC,WD_EN);
	board_printf("%s (%d)\r\n", r==MAX3510X_REG_RTC_WD_EN_ENABLED ? "enabled" : "disabled", r );
}

static const enum_t s_mode[] =
{
	{ "idle", flow_sampling_mode_idle },
	{ "event", flow_sampling_mode_event },
	{ "host", flow_sampling_mode_host },
	{ "max", flow_sampling_mode_max }
};

static void mode_get( max3510x_t *p_max3510x )
{
	flow_sampling_mode_t mode = flow_get_sampling_mode();
	const char *p = get_enum_tag( s_mode, ARRAY_COUNT(s_mode), mode );
	board_printf("%s\r\n", p );
}


static bool mode_set( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t result;
	if( get_enum_value(p_arg, s_mode, ARRAY_COUNT(s_mode), &result ) )
	{
		s_time = 0;
		flow_set_sampling_mode( (flow_sampling_mode_t)result );
		return true;
	}
	return false;
}

static void tof_temp_get( max3510x_t *p_max3510x )
{
	int16_t tof_temp = flow_get_tof_temp();
	board_printf("%d\r\n", tof_temp );
}

static bool tof_temp_set( max3510x_t *p_max3510x, const char *p_arg )
{
	int16_t tof_temp = atoi( p_arg );
	flow_set_tof_temp( tof_temp );
	tof_temp_get(p_max3510x);
	return true;
}

static void sampling_get( max3510x_t *p_max3510x )
{
	float_t freq = flow_get_sampling_frequency();
	board_printf("%.2f\r\n", freq );
}


static bool sampling_set( max3510x_t *p_max3510x, const char *p_arg )
{
	float_t freq = strtof( p_arg,NULL );
	if( !freq )
	{
		return false;
	}
	flow_set_sampling_frequency( freq );
	sampling_get(p_max3510x);
	return true;
}

static bool save_config( max3510x_t *p_max3510x, const char *p_arg )
{
	max3510x_registers_t *p_config = config_get_max3510x_regs();
	if( p_config )
	{
		max3510x_read_config( NULL, p_config );
		config_save();
		return true;
	}
	return false;
}


static bool start_event_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	static const enum_t s_start_event_enum[] = 
	{
		{ "tof", max3510x_event_timing_mode_tof },
		{ "temp", max3510x_event_timing_mode_temp },
		{ "both", max3510x_event_timing_mode_tof_temp },
	};

	uint16_t result;
	if( get_enum_value(p_arg, s_start_event_enum, ARRAY_COUNT(s_start_event_enum), &result ) )
	{
		switch( result )
		{
			case max3510x_event_timing_mode_tof:
				s_last_tdc_cmd = tdc_cmd_event_tof;
				break;
			case max3510x_event_timing_mode_temp:
				s_last_tdc_cmd = tdc_cmd_event_temp;
				break;
			case max3510x_event_timing_mode_tof_temp:
				s_last_tdc_cmd = tdc_cmd_event_tof_temp;
				break;
		}
		s_first_event = true;
		max3510x_event_timing(p_max3510x, (max3510x_event_timing_mode_t)result );
		return true;
	}
	return false;
}

static bool tof_up_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	max3510x_tof_up(p_max3510x);
	s_last_tdc_cmd = tdc_cmd_tof_up;
	return true;
}

static bool tof_down_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	max3510x_tof_down(p_max3510x);
	s_last_tdc_cmd = tdc_cmd_tof_down;
	return true;
}

static bool tof_diff_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	max3510x_tof_diff(p_max3510x);
	s_last_tdc_cmd = tdc_cmd_tof_diff;
	return true;
}

static bool temp_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	max3510x_temperature(p_max3510x);
	s_last_tdc_cmd = tdc_cmd_temp;
	return true;
}

static bool reset_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	board_reset();
	return true;
}

static bool init_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	max3510x_initialize(p_max3510x);
	return true;
}

#ifdef MAX35104
static bool bpcal_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	max3510x_bandpass_calibrate(p_max3510x);
	board_wait_ms(3);
	return true;
}
#endif

static bool halt_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	max3510x_halt(p_max3510x);
	return true;
}

static bool cal_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	s_last_tdc_cmd = tdc_cmd_cal;
	max3510x_calibrate(p_max3510x);
	return true;
}

static bool default_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	config_default();
	flow_init();
	return true;
}

static bool spi_test_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	uint16_t write = ~0;
	uint16_t read;
	uint16_t original = max3510x_read_register(p_max3510x,MAX3510X_REG_TOF_MEASUREMENT_DELAY);
	while( write )
	{
		max3510x_write_register(p_max3510x,MAX3510X_REG_TOF_MEASUREMENT_DELAY,write);
		read = max3510x_read_register(p_max3510x,MAX3510X_REG_TOF_MEASUREMENT_DELAY);
		if(  read != write )
		{
			board_printf("test failed:  write=%4.4X, read=%4.4X\r\n", write, read);
			return true;
		}
		write--;
	}
	max3510x_write_register(p_max3510x,MAX3510X_REG_TOF_MEASUREMENT_DELAY,original);
	board_printf("test passed\r\n");
	return true;
}

static bool results_report_cmd(  max3510x_t *p_max3510x, const char *p_arg )
{
	s_time = 0;
	s_results_report = true;
	return true;
}

static bool help_cmd( max3510x_t *p_max3510x, const char *p_arg );
static bool dc_cmd( max3510x_t *p_max3510x, const char *p_arg );

static const cmd_t s_cmd[] =
{
#if defined(MAX35104)
	{ "sfreq", "switcher frequency in kHz:  100, 125, 166, or 200", sfreq_set, sfreq_get },
	{ "hreg_d", "high voltage regulator disable:  1=disable regulator, 0=enable regulator", hreg_d_set, hreg_d_get },
	{ "dreq", "doubler freqency in kHz: 100, 125, 166, or 200", dreq_set, dreq_get },
	{ "vs", "voltage select regulator target: 5.4V-27V", vs_set, vs_get },
	{ "lt_n", "limit trim normal operation (mV): loop, 200, 400, 800, 1600", lt_n_set, lt_n_get },
	{ "lt_s", "limit trim startup (mV): none, 200, 400, 800, 1600", lt_s_set, lt_s_get },
	{ "st", "switcher stabilzation time:  64us - 16.4ms", st_set, st_get },
	{ "lt_50d", "limit trim 50% disable:  1=disable trim, 0=enable trim", lt_50d_set, lt_50d_get },
	{ "pecho", "pulse echo: 1=pulse echo mode, 0=time-of-flight mode", pecho_set, pecho_get },
	{ "afe_bp", "AFE bypass: 1=bypass, 0=normal", afe_bp_set, afe_bp_get },
	{ "sd_en", "single ended drive enable:  1=single ended drive, 0=differential drive", sd_en_set, sd_en_get },
	{ "afeout", "AFE output select: disabled, bandpass, pga, or fga", afeout_set, afeout_get },
	{ "4m_bp", "4MHz bypass:  1=using external CMOS clock signal, 0=using crystal oscillator", _4m_bp_set, _4m_bp_get },
	{ "f0", "bandpass center frequency adjustment", f0_set, f0_get },
	{ "pga", "pga gain:  10.00dB - 29.95dB", pga_set, pga_get },
	{ "lowq", "bandpass Q (Hz/Hz):  4.2, 5.3, 7.4, or 12", lowq_set, lowq_get },
	{ "bp_bp", "bandpass filter bypass:  1=filter bypassed, 0=filter active", bp_bp_set, bp_bp_get },
 #endif
	{ "pl", "pulse launch size:  0-127 pulses", pl_set, pl_get },
	{ "dpl", "pulse launch frequency (kHz): 125 to 1000", dpl_set, dpl_get },
	{ "stop_pol", "comparator stop polarity: 0=positive, 1=negative", stop_pol_set, stop_pol_get },
	{ "stop", "number of stop hits: 1-6", stop_set, stop_get },
	{ "t2wv", "t2 wave select: 2-63", t2wv_set, t2wv_get },
	{ "tof_cyc", "start-to-start time for TOF_DIFF measurements (us): 0, 122, 488, 732, 976, 16650, or 19970", tof_cyc_set, tof_cyc_get },
	{ "timout", "measurement timeout (us): 128, 256, 512, 1024, 2048, 4096, 8192, or 16384", timout_set, timout_get },
	{ "hitwv", "hit wave selection:  comma delimited list of wave numbers", hitwv_set, hitwv_get },
	{ "c_offsetupr", "comparator offset upstream (mV when VCC=3.3V): -137.5 to 136.4", c_offsetupr_set, c_offsetupr_get },
	{ "c_offsetup", "comparator return upstream (mV when VCC=3.3V): 0 to 136.4", c_offsetup_set, c_offsetup_get },
	{ "c_offsetdnr", "comparator offset downstream (mV when VCC=3.3V): -137.5 to 136.4", c_offsetdnr_set, c_offsetdnr_get },
	{ "c_offsetdn", "comparator return downstream (mV when VCC=3.3V): 0 to 136.4", c_offsetdn_set, c_offsetdn_get },
	{ "tdf", "TOF difference measurement period (s):  0.5 to 8.0", tdf_set, tdf_get },
	{ "tdm", "Number of TOF difference measurements to perform: 1 to 32", tdm_set, tdm_get },
	{ "tmf", "temperature measurement period (s):  1 to 64", tmf_set, tmf_get },
	{ "tmm", "Number of temperature measurements to perform: 1 to 32", tmm_set, tmm_get },
	{ "cal_use", "calibration usage:  1=enable, 0=disable", cal_use_set, cal_use_get },
	{ "cal_cfg", "calibration configuration: cc, cs, sc, or ss", cal_cfg_set, cal_cfg_get },
	{ "precyc", "preamble temperature cycle: 0-7", precyc_set, precyc_get },
	{ "portcyc", "port cycle time (us):  128, 256, 384, or 512", portcyc_set, portcyc_get },
	{ "dly", "measurement delay (us):  25 to 16383.75", dly_set, dly_get },
	{ "cmp_en", "comparator or up/down pin enable: 1=enable, 0=disable", cmp_en_set, cmp_en_get },
	{ "cmp_sel", "comparator or up/down select:  1=comparator, 0=up/down", cmp_sel_set, cmp_sel_get },
	{ "et_cont", "event timing continuous operation: 1=continuous, 0=one-shot", et_cont_set, et_cont_get },
	{ "cont_int", "continuous interrupt:  1=continuous, 0=one-shot", cont_int_set, cont_int_get },
	{ "clk_s", "clock settling time (us): 488, 1460, 2930, 3900, 5130, or continuous", clk_s_set, clk_s_get },
	{ "cal_period", "4MHz clock calibration period (us):  30.5 to 488.0", cal_period_set, cal_period_get },
	{ "32k_bp", "32kHz bypass: 1=cmos clock input, 0=crystal input", _32k_bp_set, _32k_bp_get },
	{ "32k_en", "enable 32KOUT pin:  1=enable, 0=disable", _32k_en_set, _32k_en_get },
	{ "eosc", "enable RTC oscillator: 0=enable, 1= disable", eosc_set, eosc_get },
	{ "am", "alarm control:  none, minutes, hours, or both", am_set, am_get },
	{ "wf", "watchdog flag:  0=reset", wf_set, wf_get },
	{ "wd_en", "watchdog enable:  1=enabled, 0=disabled", wd_en_set, wd_en_get },
	
	// load/save
	
	{ "save", "save configuration to flash", save_config, NULL },

	// chip commands

	{ "event", "start event timing mode: tof, temp, or both", start_event_cmd, NULL },
	{ "tof_up", "TOF_UP command", tof_up_cmd, NULL },
	{ "tof_down", "TOF_DOWN command", tof_down_cmd, NULL },
	{ "tof_diff", "TOF_DIFF command", tof_diff_cmd, NULL },
	{ "temp", "temperature command", temp_cmd, NULL },
	{ "reset", "reset command", reset_cmd, NULL },
	{ "init", "initialize command", init_cmd, NULL },
#ifdef MAX35104
	{ "bpcal", "bandpass calibration command", bpcal_cmd, NULL },
#endif
	{ "halt", "halt command", halt_cmd, NULL },
	{ "cal", "calibrate command", cal_cmd, NULL },
	{ "dc", "dumps all configuration registers", dc_cmd, NULL },

	// host commands

	{ "spi_test", "perform's a write/read verification test on the max3510x", spi_test_cmd, NULL },
	{ "tof_temp", "number of tof measurements for each temperature measurement", tof_temp_set, tof_temp_get },
	{ "default", "restore configuration defaults", default_cmd, NULL },
	{ "mode", "select sampling mode: event, host, max, idle", mode_set, mode_get },
	{ "sampling", "host mode sampling frequency", sampling_set, sampling_get },
	{ "report", "turn on sample reports until a key is pressed", results_report_cmd, NULL },
	{ "help", "you're looking at it", help_cmd, NULL }
};

static bool dc_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	uint8_t i;
	uint8_t max_len = 0, len;

	for(i=0;i<ARRAY_COUNT(s_cmd);i++)
	{
		if(  s_cmd[i].p_get )
		{
			len = strlen(s_cmd[i].p_cmd);
			if(  len > max_len )
				max_len = len;
		}
	}
	for(i=0;i<ARRAY_COUNT(s_cmd);i++)
	{
		if(  s_cmd[i].p_get )
		{
			len = strlen(s_cmd[i].p_cmd);
			len = max_len - len;
			board_printf( s_cmd[i].p_cmd );
			while( len-- )
				board_printf( " " );
			board_printf(" = ");
			s_cmd[i].p_get(p_max3510x);
		}
	}
	return true;
}

static bool help_cmd( max3510x_t *p_max3510x, const char *p_arg )
{
	uint8_t i;
	uint8_t max_len = 0, len;

	board_printf("\r\n");
	for(i=0;i<ARRAY_COUNT(s_cmd);i++)
	{
		len = strlen(s_cmd[i].p_cmd);
		if(  len > max_len )
			max_len = len;
	}
	for(i=0;i<ARRAY_COUNT(s_cmd);i++)
	{
		len = strlen(s_cmd[i].p_cmd);
		board_printf( s_cmd[i].p_cmd );
		len = max_len - len;
		while( len-- )
			board_printf( " " );
		board_printf(" - %s\r\n", s_cmd[i].p_help );
	}
	board_printf("\r\n");
	return true;
}


static void record_command( void )
{
	strncpy( &s_command_history[s_command_ndx][0], s_rx_buf, sizeof(s_command_history[0])-1 );
	s_command_ndx++;
	if( s_command_ndx >= COMMAND_HISTORY_COUNT )
	{
		s_command_ndx = 0;
	}
	if( s_command_ndx  )
		s_command_read_ndx = s_command_ndx - 1;
	else
		s_command_read_ndx = COMMAND_HISTORY_COUNT-1;
}

static void retrieve_command( void )
{
	if( s_command_history[s_command_read_ndx][0] )
	{
		strncpy( &s_rx_buf[0], &s_command_history[s_command_read_ndx][0], sizeof(s_rx_buf)-1 );
		s_rx_ndx = strlen(s_rx_buf);
		
		board_printf("\33[2K\r> %s", s_rx_buf );
		if( !s_command_read_ndx )
		{
			s_command_read_ndx = COMMAND_HISTORY_COUNT-1;
		}
		else
		{
			s_command_read_ndx--;
		}
	}
}


static bool escape( char c )
{
	static uint8_t s_escape;
	// handles cursour up CSI
	bool ret = false;
	if( c == 0x1B )
	{
		s_escape = 1;
		ret = true;
	}
	else if( s_escape == 1 )
	{
		if( c == '[' )
		{
			s_escape = 2;
		}
		ret = true;
	}
	else if(  s_escape == 2 )
	{
		if( c == 'A' )
		{
			retrieve_command();
		}
		s_escape = 0;
		ret = true;
	}
	return ret;
}

static void command( void )
{
	uint8_t c;
	if( board_uart_read(&c, 1) )
	{
		
		s_output = false;
		if( escape(c) )
			return;
		board_uart_write( &c, 1 );
		if( c == '\r' )
			board_printf("\n");
		
		if( s_results_report )
		{
			s_results_report = false;
			board_printf("\33[2K\r> %c", c);
		}
		if( c != '\r' )
		{
			if( isprint(c) )
			{
				s_rx_buf[s_rx_ndx++] = c;
			}
			else if( c == 0x7F )
			{
				s_rx_buf[s_rx_ndx] = 0;
				if( s_rx_ndx )
					s_rx_ndx--;
				return;
			}
		}
		else if( c == '\r' || (s_rx_ndx == sizeof(s_rx_buf)-1) )
		{
			uint8_t i, max_ndx = 0;
			int len, max_len  =0;
			bool b = false;

			strcpy( &s_rx_buf[0], skip_space(&s_rx_buf[0]) );

			for(i=0;i<ARRAY_COUNT(s_cmd);i++)
			{
				len = strlen(s_cmd[i].p_cmd );
				if( !strncmp( s_cmd[i].p_cmd, &s_rx_buf[0], len ) )
				{
					if( len > max_len )
					{
						max_len = len;
						max_ndx = i;
					}
				}
			}
			if( max_len )
			{
				len = max_len;
				i = max_ndx;
				const char *p_cmd_type = &s_rx_buf[len];
				if( *p_cmd_type == '?' )
				{
					if( s_cmd[i].p_get )
					{
						s_cmd[i].p_get( NULL );
						b = true;
						record_command();
						board_printf("> ");
					}
					else
					{
						board_printf("read not supported\r\n> ");
					} 
				} 
				else 
				{
					if( *p_cmd_type == ' ' )
						p_cmd_type = skip_space(p_cmd_type);
					else
					{
						p_cmd_type = skip_space(p_cmd_type);
					}
					if(  *p_cmd_type == '=' )
						p_cmd_type = skip_space(p_cmd_type+1);
				
					if( s_cmd[i].p_set )
					{
						if( !s_cmd[i].p_set( NULL, p_cmd_type ) )
							board_printf("argument error:  %s\r\n> ", s_cmd[i].p_help);
						else
						{
							record_command();
							if( !s_results_report )
								board_printf( "> " );
						}
						b = true;
					}
					else
					{
						board_printf("assignment not supported\r\n> ");
					}
				}
			}
			if( b == false )
			{
					if( s_rx_buf[0] )
						board_printf("unknown command.  type 'help' for a command list.\r\n");
					board_printf("> ");
			}
			memset( &s_rx_buf[0], 0, sizeof(s_rx_buf) );
			s_rx_ndx = 0;
			s_output = true;
		}
	}
}

void uui_init(void)
{
	s_output = true;
	board_printf("> ");
}


void uui_event( uint32_t event )
{
	if( event & BOARD_EVENT_SYSTICK )
	{
		if( s_output && s_display_count && s_accumulation_time )
		{
			if( !--s_display_count )
			{
				s_display_count = DISPLAY_COUNT;
				float_t volume_liters = (s_volume_last - s_volume_first) * 1000.0f * 60.0f; // cubic meters to liters
				float_t volume_rate = volume_liters / s_accumulation_time; // lpm
			//	board_printf( "\33[2K\r%.3f LPM, %.3fL\r\n> ", volume_rate, s_volume_last * 1000.0f * 60.0f );
				s_first = true;
			}
		}
	}
	command();
}

void uui_cmd_response( const char *p_format, ... )
{
	if( s_output )
	{
		va_list args;
		va_start(args, p_format);
		board_printf("\33[2K\r");
		board_printf( p_format, args );
		board_printf("\r\n> ");
		va_end(args);
	}
}

void uui_cal_complete( void )
{
	if( s_output )
	{
		max3510x_fixed_t fixed;
		max3510x_read_fixed(NULL,MAX3510X_REG_CALIBRATIONINT,&fixed);
		uint32_t _4mhz_xtal_freq = max3510x_input_frequency( &fixed );
		float_t factor = max3510x_calibration_factor( _4mhz_xtal_freq );
		uui_cmd_response( "4MX = %d, factor = %e", (uint32_t)_4mhz_xtal_freq, factor);
	}
}

void uui_update( float_t volume )
{
	static uint32_t timestamp;
	if( s_first )
	{
		timestamp = board_timestamp();
		s_volume_first = volume;
		s_volume_last = volume;
		s_accumulation_time = 0;
		s_first = false;
	}
	else
	{
		s_volume_last = volume;
		board_elapsed_time( timestamp, &s_accumulation_time );
	}

}

void uui_report_results( float_t *p_up, float_t *p_down, float_t time, uint8_t hitcount, uint8_t ndx )
{
	if( s_results_report )
	{
		s_time += time;

		// up hits [6], down hits [6], time
		uint8_t i;
		if( ndx )
		{
			board_printf( "x" );
		}
		else
		{
			board_printf( "y" );
		}
		for(i=0;i<hitcount;i++)
		{
			board_printf( ",%e", p_up[i] );
		}
		for(i=0;i<hitcount;i++)
		{
			board_printf( ",%e", p_down[i] );
		}
		board_printf(",%e\r\n", s_time);
#ifdef PGA_SWITCH		
		if( pga_switch )
			pga_17_98();
		else
			pga_13_99();
		pga_switch = !pga_switch;
#endif		
	}
}

void uui_report_temp( float_t temp_K )
{
	if( s_output )
	{
		board_printf("\33[2K\r%.1fC\r\n> ", temp_K  - 273.15f );
	}
}

static void dump_tof( const max3510x_float_measurement_t *p_dir, uint8_t hitwvs[6], uint16_t hitcount )
{
	board_printf("t2/ideal = %.6f\r\n", p_dir->t2_ideal );
	board_printf("t1/t2 = %.6f\r\n", p_dir->t1_t2 );
	uint8_t i;
	for(i=0;i<hitcount;i++)
	{
		board_printf("hit%d = %e\r\n", i+1, p_dir->hit[i] );
	}
	board_printf("mean = %e\r\n", p_dir->average );
	if( hitcount > 1 )
	{
		float_t period = 0;
		for(i=1;i<hitcount;i++)
		{
			period += (p_dir->hit[i] - p_dir->hit[i-1]) / (float_t)(hitwvs[i]-hitwvs[i-1]) ;
		}
		period /= (float_t)(hitcount-1);
		board_printf("average rx frequency = %.0f\r\n", 1.0f/period );
	}
}

static void dump_tof_diff( const max3510x_float_tof_results_t *p_results, uint8_t hitwvs[6], uint16_t hitcount )
{
	dump_tof( &p_results->up, hitwvs, hitcount );
	board_printf("\r\n");
	dump_tof( &p_results->down, hitwvs, hitcount );
	board_printf("diff = %e\r\n", p_results->tof_diff);
}

static void dump_period(void)
{
	if( s_first_event )
	{
		s_first_event = false;
	}
	else
	{
		float_t delta;
		board_elapsed_time( s_last_report_time, &delta );
		board_printf("period = %.2f\r\n", delta );
	}
	s_last_report_time = board_timestamp();
}

static void dump_temp( float_t r1, float_t r2 )
{
	board_printf("r1 = %e, r2 = %e, ratio = %.3f\r\n", r1, r2, r1/r2  );
}

static void dump_temp_event( const max3510x_temp_results_t *p_fixed, const max3510x_float_temp_results_t *p_results )
{
	dump_temp( p_results->ave_temp[0], p_results->ave_temp[2] );
	board_printf("count = %d\r\n", p_fixed->temp_cycle_count );
}

static void dump_tof_event( const max3510x_tof_results_t *p_fixed, const max3510x_float_tof_results_t *p_results )
{
	dump_period();
	board_printf("tof diff = %e\r\n", p_results->tof_diff_ave );
	board_printf("range = %d\r\n", p_fixed->tof_range );
	board_printf("count = %d\r\n", p_fixed->tof_cycle_count );
}


void uui_report_tof_temp( uint16_t status )
{

	max3510x_float_tof_results_t tof_results;
	max3510x_float_temp_results_t temp_results;
	max3510x_temp_results_t temp_fixed;
	uint16_t hitcount;
	max3510x_tof_results_t tof_fixed;
	uint8_t hw[MAX3510X_MAX_HITCOUNT];

	if( !s_output )
		return;

	board_printf("\33[2K\r");

	if( s_last_tdc_cmd == tdc_cmd_tof_up || s_last_tdc_cmd == tdc_cmd_tof_down || s_last_tdc_cmd == tdc_cmd_tof_diff ||
		s_last_tdc_cmd == tdc_cmd_event_tof || 
		( (s_last_tdc_cmd == tdc_cmd_event_tof_temp) && (status & MAX3510X_REG_INTERRUPT_STATUS_TOF_EVTMG) ) )
	{
		hitcount = MAX3510X_REG_TOF2_STOP( MAX3510X_READ_BITFIELD( NULL, TOF2, STOP ) );
		max3510x_get_hitwaves( NULL, &hw[0] );
		max3510x_read_tof_results( NULL, &tof_fixed );
		max3510x_convert_tof_results(&tof_results, &tof_fixed);
	}
	if( s_last_tdc_cmd == tdc_cmd_event_temp || s_last_tdc_cmd == tdc_cmd_temp ||
		( (s_last_tdc_cmd == tdc_cmd_event_tof_temp) && (status & MAX3510X_REG_INTERRUPT_STATUS_TEMP_EVTMG) ) )
	{
		max3510x_read_temp_results( NULL, &temp_fixed );
		max3510x_convert_temp_results(&temp_results, &temp_fixed);
	}
	switch( s_last_tdc_cmd )
	{
		case tdc_cmd_tof_up:
		{
			dump_tof( &tof_results.up, hw, hitcount );
			break;
		}
		case tdc_cmd_tof_down:
		{
			dump_tof( &tof_results.down, hw, hitcount );
			break;
		}
		case tdc_cmd_tof_diff:
		{
			dump_tof_diff( &tof_results, hw, hitcount );
			break;
		}
		case tdc_cmd_temp:
		{
			dump_temp( temp_results.temp[0], temp_results.temp[2] );
			break;
		}
		case tdc_cmd_event_tof:
		{
			if( status & MAX3510X_REG_INTERRUPT_STATUS_TOF_EVTMG )
				dump_tof_event( &tof_fixed, &tof_results );
			break;
		}
		case tdc_cmd_event_temp:
		{
			if( status & MAX3510X_REG_INTERRUPT_STATUS_TEMP_EVTMG )
				dump_temp_event( &temp_fixed, &temp_results );
			break;
		}
		case tdc_cmd_event_tof_temp:
		{
			if( status & MAX3510X_REG_INTERRUPT_STATUS_TOF_EVTMG )
				dump_tof_event( &tof_fixed, &tof_results );
			if( status & MAX3510X_REG_INTERRUPT_STATUS_TEMP_EVTMG )
				dump_temp_event( &temp_fixed, &temp_results );
			break;
		}
		case tdc_cmd_cal:
		{
			max3510x_fixed_t fixed;
			max3510x_read_fixed(NULL,MAX3510X_REG_CALIBRATIONINT,&fixed);
			uint32_t _4mhz_xtal_freq = max3510x_input_frequency( &fixed );
			float_t factor = max3510x_calibration_factor( _4mhz_xtal_freq );
			board_printf( "4MX = %d, factor = %e", (uint32_t)_4mhz_xtal_freq, factor);
			break;
		}
	}
	board_printf("\r\n> ");
}
