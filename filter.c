#include "global.h"
#include "filter.h"
#include <arm_math.h>

void filter_init( filter_t *p_filter )
{
	static const float_t s_coef[10] = 
	{
		0.024886971908111608, 0.049773943816223215, 0.024886971908111608, 1.493723940028769, -0.562101135566579,
		0.015625, 0.03125, 0.015625, 1.572224881546609, -0.6632163726145267
	};
	memset( p_filter, 0, sizeof( filter_t) );
	arm_biquad_cascade_df1_init_f32( &p_filter->filter, 2, (float_t*)s_coef, p_filter->state );
}

float_t filter_sample( filter_t *p_filter, float_t sample )
{
	return sample;
	/*
	float_t output;
	arm_biquad_cascade_df1_f32( &p_filter->filter, &sample, &output, 1 );
	return output;
	*/
}

