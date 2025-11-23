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

	PF_EffectWorld *input_worldP = &params[RGBDELAY_INPUT]->u.ld;
	PF_EffectWorld *output_worldP = output;

	// Get parameter values
	PF_FpLong red_delay = params[RGBDELAY_RED_DELAY]->u.fs_d.value;
	PF_FpLong green_delay = params[RGBDELAY_GREEN_DELAY]->u.fs_d.value;
	PF_FpLong blue_delay = params[RGBDELAY_BLUE_DELAY]->u.fs_d.value;

	// Use AEFX_ChannelDepthTpl.h to handle different bit depths
	// ERR(suites.Iterate8Suite1()->iterate(	in_data,
	// 										0,								
	// 										output->height,
	// 										input_worldP,
	// 										NULL,
	// 										(void*)params,
	// 										NULL,
	// 										output_worldP));
	
	// Fix: Use PF_COPY for now to avoid bad parameter error (NULL callback)
	// ERR(PF_COPY(input_worldP, output_worldP, NULL, NULL));
	
	// Implement simple RGB Delay
	PF_COPY(input_worldP, output_worldP, NULL, NULL);
	
	// We need to read from input and write to output with offsets
	// Since we already copied, we can just read from input (which is safe) and overwrite output
	
	A_long width = output->width;
	A_long height = output->height;
	
	// RGB Delay implementation
	int r_delay = (int)red_delay;
	int g_delay = (int)green_delay;
	int b_delay = (int)blue_delay;
	
	if (PF_WORLD_IS_DEEP(output)) {
		// 16-bit
		A_long rowbytes = output->rowbytes;
		for (int y = 0; y < height; y++) {
			PF_Pixel16 *in_row = (PF_Pixel16*)((char*)input_worldP->data + y * rowbytes);
			PF_Pixel16 *out_row = (PF_Pixel16*)((char*)output_worldP->data + y * rowbytes);
			
			for (int x = 0; x < width; x++) {
				int rx = std::min((int)width - 1, std::max(0, x - r_delay));
				int gx = std::min((int)width - 1, std::max(0, x - g_delay));
				int bx = std::min((int)width - 1, std::max(0, x - b_delay));
				
				out_row[x].alpha = in_row[x].alpha;
				out_row[x].red = in_row[rx].red;
				out_row[x].green = in_row[gx].green;
				out_row[x].blue = in_row[bx].blue;
			}
		}
	} else {
		// 8-bit
		A_long rowbytes = output->rowbytes;
		for (int y = 0; y < height; y++) {
			PF_Pixel8 *in_row = (PF_Pixel8*)((char*)input_worldP->data + y * rowbytes);
			PF_Pixel8 *out_row = (PF_Pixel8*)((char*)output_worldP->data + y * rowbytes);
			
			for (int x = 0; x < width; x++) {
				int rx = std::min((int)width - 1, std::max(0, x - r_delay));
				int gx = std::min((int)width - 1, std::max(0, x - g_delay));
				int bx = std::min((int)width - 1, std::max(0, x - b_delay));
				
				out_row[x].alpha = in_row[x].alpha;
				out_row[x].red = in_row[rx].red;
				out_row[x].green = in_row[gx].green;
				out_row[x].blue = in_row[bx].blue;
			}
		}
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
