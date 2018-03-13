#ifndef __FILTER__
#define __FILTER__

#define __FPU_PRESENT 1

#include <arm_math.h>

typedef struct _filter_t
{
	arm_biquad_casd_df1_inst_f32 filter;
	float_t 	state[8];
	float_t 	output;
}
filter_t;

void filter_init( filter_t *p_filter );
float_t filter_sample( filter_t *p_filter, float_t sample );

#endif
