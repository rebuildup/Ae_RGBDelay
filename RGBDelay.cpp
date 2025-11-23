#define NOMINMAX
#include "RGBDelay.h"

#include <algorithm>

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s",
											"RGBDelay", 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											"RGB channel delay effect");
	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);
	
	out_data->out_flags =  PF_OutFlag_DEEP_COLOR_AWARE;
	out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	
	return PF_Err_NONE;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		"Red Delay",
		RGBDELAY_AMOUNT_MIN,
		RGBDELAY_AMOUNT_MAX,
		RGBDELAY_AMOUNT_MIN,
		RGBDELAY_AMOUNT_MAX,
		RGBDELAY_AMOUNT_DFLT,
		PF_Precision_TENTHS,
		0,
		0,
		RED_DELAY_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		"Green Delay",
		RGBDELAY_AMOUNT_MIN,
		RGBDELAY_AMOUNT_MAX,
		RGBDELAY_AMOUNT_MIN,
		RGBDELAY_AMOUNT_MAX,
		RGBDELAY_AMOUNT_DFLT,
		PF_Precision_TENTHS,
		0,
		0,
		GREEN_DELAY_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		"Blue Delay",
		RGBDELAY_AMOUNT_MIN,
		RGBDELAY_AMOUNT_MAX,
		RGBDELAY_AMOUNT_MIN,
		RGBDELAY_AMOUNT_MAX,
		RGBDELAY_AMOUNT_DFLT,
		PF_Precision_TENTHS,
		0,
		0,
		BLUE_DELAY_DISK_ID);

	out_data->num_params = RGBDELAY_NUM_PARAMS;

	return err;
}

static PF_Err
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err				err		= PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	// Get parameter values (in frames)
	int red_delay_frames = (int)(params[RGBDELAY_RED_DELAY]->u.fs_d.value);
	int green_delay_frames = (int)(params[RGBDELAY_GREEN_DELAY]->u.fs_d.value);
	int blue_delay_frames = (int)(params[RGBDELAY_BLUE_DELAY]->u.fs_d.value);
	
	A_long current_time = in_data->current_time;
	A_long time_step = in_data->time_step;
	
	A_long width = output->width;
	A_long height = output->height;
	A_long rowbytes = output->rowbytes;
	
	// Checkout delayed frames for each channel
	PF_ParamDef red_layer, green_layer, blue_layer;
	AEFX_CLR_STRUCT(red_layer);
	AEFX_CLR_STRUCT(green_layer);
	AEFX_CLR_STRUCT(blue_layer);
	
	A_long red_time = current_time - (red_delay_frames * time_step);
	A_long green_time = current_time - (green_delay_frames * time_step);
	A_long blue_time = current_time - (blue_delay_frames * time_step);
	
	ERR(PF_CHECKOUT_PARAM(in_data, RGBDELAY_INPUT, red_time, time_step, time_step, &red_layer));
	if (!err) ERR(PF_CHECKOUT_PARAM(in_data, RGBDELAY_INPUT, green_time, time_step, time_step, &green_layer));
	if (!err) ERR(PF_CHECKOUT_PARAM(in_data, RGBDELAY_INPUT, blue_time, time_step, time_step, &blue_layer));
	
	if (!err) {
		PF_EffectWorld *red_world = &red_layer.u.ld;
		PF_EffectWorld *green_world = &green_layer.u.ld;
		PF_EffectWorld *blue_world = &blue_layer.u.ld;
		
		if (PF_WORLD_IS_DEEP(output)) {
			// 16-bit
			for (int y = 0; y < height; y++) {
				PF_Pixel16 *red_row = (PF_Pixel16*)((char*)red_world->data + y * red_world->rowbytes);
				PF_Pixel16 *green_row = (PF_Pixel16*)((char*)green_world->data + y * green_world->rowbytes);
				PF_Pixel16 *blue_row = (PF_Pixel16*)((char*)blue_world->data + y * blue_world->rowbytes);
				PF_Pixel16 *out_row = (PF_Pixel16*)((char*)output->data + y * rowbytes);
				
				for (int x = 0; x < width; x++) {
					out_row[x].alpha = red_row[x].alpha;
					out_row[x].red = red_row[x].red;
					out_row[x].green = green_row[x].green;
					out_row[x].blue = blue_row[x].blue;
				}
			}
		} else {
			// 8-bit
			for (int y = 0; y < height; y++) {
				PF_Pixel8 *red_row = (PF_Pixel8*)((char*)red_world->data + y * red_world->rowbytes);
				PF_Pixel8 *green_row = (PF_Pixel8*)((char*)green_world->data + y * green_world->rowbytes);
				PF_Pixel8 *blue_row = (PF_Pixel8*)((char*)blue_world->data + y * blue_world->rowbytes);
				PF_Pixel8 *out_row = (PF_Pixel8*)((char*)output->data + y * rowbytes);
				
				for (int x = 0; x < width; x++) {
					out_row[x].alpha = red_row[x].alpha;
					out_row[x].red = red_row[x].red;
					out_row[x].green = green_row[x].green;
					out_row[x].blue = blue_row[x].blue;
				}
			}
		}
		
		ERR2(PF_CHECKIN_PARAM(in_data, &blue_layer));
		ERR2(PF_CHECKIN_PARAM(in_data, &green_layer));
		ERR2(PF_CHECKIN_PARAM(in_data, &red_layer));
	}

	return err;
}


extern "C" DllExport
PF_Err PluginDataEntryFunction2(
	PF_PluginDataPtr inPtr,
	PF_PluginDataCB2 inPluginDataCallBackPtr,
	SPBasicSuite* inSPBasicSuitePtr,
	const char* inHostName,
	const char* inHostVersion)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;

	result = PF_REGISTER_EFFECT_EXT2(
		inPtr,
		inPluginDataCallBackPtr,
		"RGBDelay", // Name
		"361do RGBDelay", // Match Name
		"361do_plugins", // Category
		AE_RESERVED_INFO,
		"EffectMain",
		"https://github.com/rebuildup/Ae_RGBDelay");

	return result;
}


DllExport	
PF_Err 
EffectMain(
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:
				err = About(in_data,
							out_data,
							params,
							output);
				break;
				
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_RENDER:
				err = Render(	in_data,
								out_data,
								params,
								output);
				break;
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}
