#include "RGBDelay.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>

// -----------------------------------------------------------------------------
// Constants & Helpers
// -----------------------------------------------------------------------------

template <typename T>
static inline T Clamp(T val, T minVal, T maxVal) {
    return std::max(minVal, std::min(val, maxVal));
}

// -----------------------------------------------------------------------------
// Pixel Traits
// -----------------------------------------------------------------------------

template <typename PixelT>
struct PixelTraits;

template <>
struct PixelTraits<PF_Pixel> {
    using ChannelType = A_u_char;
    static constexpr float MAX_VAL = 255.0f;
    static inline float ToFloat(ChannelType v) { return static_cast<float>(v); }
    static inline ChannelType FromFloat(float v) { return static_cast<ChannelType>(Clamp(v, 0.0f, MAX_VAL) + 0.5f); }
};

template <>
struct PixelTraits<PF_Pixel16> {
    using ChannelType = A_u_short;
    static constexpr float MAX_VAL = 32768.0f;
    static inline float ToFloat(ChannelType v) { return static_cast<float>(v); }
    static inline ChannelType FromFloat(float v) { return static_cast<ChannelType>(Clamp(v, 0.0f, MAX_VAL) + 0.5f); }
};

template <>
struct PixelTraits<PF_PixelFloat> {
    using ChannelType = PF_FpShort;
    static inline float ToFloat(ChannelType v) { return static_cast<float>(v); }
    static inline ChannelType FromFloat(float v) { return static_cast<ChannelType>(v); }
};

// -----------------------------------------------------------------------------
// Sampling
// -----------------------------------------------------------------------------

template <typename Pixel>
static inline float SampleChannelBilinear(const A_u_char *base_ptr,
                                          A_long rowbytes,
                                          float xf,
                                          float yf,
                                          int width,
                                          int height,
                                          int channel_idx) // 0=A, 1=R, 2=G, 3=B (PF_Pixel structure is ARGB usually? No, usually ARGB or BGRA depending on platform)
                                          // PF_Pixel is struct { alpha, red, green, blue }.
{
    // Clamp coordinates
    xf = Clamp(xf, 0.0f, static_cast<float>(width - 1));
    yf = Clamp(yf, 0.0f, static_cast<float>(height - 1));

    const int x0 = static_cast<int>(xf);
    const int y0 = static_cast<int>(yf);
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);

    const float tx = xf - static_cast<float>(x0);
    const float ty = yf - static_cast<float>(y0);

    const Pixel *row0 = reinterpret_cast<const Pixel *>(base_ptr + static_cast<A_long>(y0) * rowbytes);
    const Pixel *row1 = reinterpret_cast<const Pixel *>(base_ptr + static_cast<A_long>(y1) * rowbytes);

    const Pixel &p00 = row0[x0];
    const Pixel &p10 = row0[x1];
    const Pixel &p01 = row1[x0];
    const Pixel &p11 = row1[x1];

    auto get_val = [&](const Pixel& p) -> float {
        if (channel_idx == 0) return PixelTraits<Pixel>::ToFloat(p.alpha);
        if (channel_idx == 1) return PixelTraits<Pixel>::ToFloat(p.red);
        if (channel_idx == 2) return PixelTraits<Pixel>::ToFloat(p.green);
        if (channel_idx == 3) return PixelTraits<Pixel>::ToFloat(p.blue);
        return 0.0f;
    };

    float v00 = get_val(p00);
    float v10 = get_val(p10);
    float v01 = get_val(p01);
    float v11 = get_val(p11);

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    return lerp(lerp(v00, v10, tx), lerp(v01, v11, tx), ty);
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------

template <typename Pixel>
static PF_Err RenderGeneric(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output) {
    PF_EffectWorld* input = &params[RGBDELAY_INPUT]->u.ld;
    
    const int width = output->width;
    const int height = output->height;
    
    if (width <= 0 || height <= 0) return PF_Err_NONE;

    const A_u_char* input_base = reinterpret_cast<const A_u_char*>(input->data);
    A_u_char* output_base = reinterpret_cast<A_u_char*>(output->data);
    const A_long input_rowbytes = input->rowbytes;
    const A_long output_rowbytes = output->rowbytes;

    // Parameters
    float red_delay = static_cast<float>(params[RGBDELAY_RED_DELAY]->u.fs_d.value);
    float green_delay = static_cast<float>(params[RGBDELAY_GREEN_DELAY]->u.fs_d.value);
    float blue_delay = static_cast<float>(params[RGBDELAY_BLUE_DELAY]->u.fs_d.value);

    // Multi-threading
    int num_threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;

    auto process_rows = [&](int start_y, int end_y) {
        for (int y = start_y; y < end_y; ++y) {
            Pixel* out_row = reinterpret_cast<Pixel*>(output_base + y * output_rowbytes);
            
            for (int x = 0; x < width; ++x) {
                float fx = static_cast<float>(x);
                float fy = static_cast<float>(y);

                // Sample each channel independently
                // Assuming "Delay" means shift to the right? Or left?
                // Usually delay means it arrives later, so it's shifted left?
                // Or if we view it as "offset", positive offset shifts source lookup.
                // out(x) = in(x - delay)
                
                float r = SampleChannelBilinear<Pixel>(input_base, input_rowbytes, fx - red_delay, fy, width, height, 1);
                float g = SampleChannelBilinear<Pixel>(input_base, input_rowbytes, fx - green_delay, fy, width, height, 2);
                float b = SampleChannelBilinear<Pixel>(input_base, input_rowbytes, fx - blue_delay, fy, width, height, 3);
                float a = SampleChannelBilinear<Pixel>(input_base, input_rowbytes, fx, fy, width, height, 0); // Alpha not shifted? Or max of shifts?
                // Usually alpha stays or follows one channel.
                // Let's keep alpha unshifted for "RGB" delay, or maybe max of shifted alphas?
                // If we shift RGB, we might shift alpha too?
                // But we have 3 delays.
                // Let's keep alpha at 0 delay (original position) to maintain silhouette, 
                // or maybe sample alpha at each delay and take max?
                // "RGB Split" usually keeps alpha from original or union.
                // Let's use original alpha for now.
                
                out_row[x].red = PixelTraits<Pixel>::FromFloat(r);
                out_row[x].green = PixelTraits<Pixel>::FromFloat(g);
                out_row[x].blue = PixelTraits<Pixel>::FromFloat(b);
                out_row[x].alpha = PixelTraits<Pixel>::FromFloat(a);
            }
        }
    };

    int rows_per_thread = (height + num_threads - 1) / num_threads;
    for (int i = 0; i < num_threads; ++i) {
        int start = i * rows_per_thread;
        int end = std::min(start + rows_per_thread, height);
        if (start < end) threads.emplace_back(process_rows, start, end);
    }
    for (auto& t : threads) t.join();

    return PF_Err_NONE;
}

static PF_Err Render(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output) {
    int bpp = (output->width > 0) ? (output->rowbytes / output->width) : 0;
    if (bpp == sizeof(PF_PixelFloat)) {
        return RenderGeneric<PF_PixelFloat>(in_data, out_data, params, output);
    } else if (bpp == sizeof(PF_Pixel16)) {
        return RenderGeneric<PF_Pixel16>(in_data, out_data, params, output);
    } else {
        return RenderGeneric<PF_Pixel>(in_data, out_data, params, output);
    }
}

static PF_Err
About(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
        "%s v%d.%d\r%s",
        STR(StrID_Name),
        MAJOR_VERSION,
        MINOR_VERSION,
        STR(StrID_Description));
    return PF_Err_NONE;
}

static PF_Err
GlobalSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PIX_INDEPENDENT;
    out_data->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return PF_Err_NONE;
}

static void UnionLRect(const PF_LRect* src, PF_LRect* dst) {
    if (src->left < dst->left) dst->left = src->left;
    if (src->top < dst->top) dst->top = src->top;
    if (src->right > dst->right) dst->right = src->right;
    if (src->bottom > dst->bottom) dst->bottom = src->bottom;
}

static PF_Err
PreRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    void* extraP)
{
    PF_Err err = PF_Err_NONE;
    PF_PreRenderExtra_Local* extra = (PF_PreRenderExtra_Local*)extraP;
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult in_result;

    // Checkout input
    ERR(extra->cb->checkout_layer(in_data->effect_ref,
        RGBDELAY_INPUT,
        RGBDELAY_INPUT,
        &req,
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        &in_result));

    // Set result rects
    if (!err) {
        UnionLRect(&in_result.result_rect, &extra->output->result_rect);
        UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
    }

    return err;
}

static PF_Err
SmartRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    void* extraP)
{
    PF_Err err = PF_Err_NONE;
    PF_SmartRenderExtra_Local* extra = (PF_SmartRenderExtra_Local*)extraP;
    PF_EffectWorld* input_world = NULL;
    PF_EffectWorld* output_world = NULL;
    
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    PF_WorldSuite2* wsP = NULL;
    ERR(suites.SPBasicSuite()->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, (const void**)&wsP));

    if (!err) {
        // Checkout input/output
        ERR((extra->cb->checkout_layer_pixels(in_data->effect_ref, RGBDELAY_INPUT, &input_world)));
        ERR(extra->cb->checkout_output(in_data->effect_ref, &output_world));
    }

    if (!err && input_world && output_world) {
        // Checkout parameters
        PF_ParamDef p[RGBDELAY_NUM_PARAMS];
        PF_ParamDef* pp[RGBDELAY_NUM_PARAMS];
        
        // Input
        AEFX_CLR_STRUCT(p[RGBDELAY_INPUT]);
        p[RGBDELAY_INPUT].u.ld = *input_world;
        pp[RGBDELAY_INPUT] = &p[RGBDELAY_INPUT];

        // Params
        for (int i = 1; i < RGBDELAY_NUM_PARAMS; ++i) {
            PF_Checkout_Value(in_data, out_data, i, in_data->current_time, in_data->time_step, in_data->time_scale, &p[i]);
            pp[i] = &p[i];
        }

        // Call Render
        int bpp = (output_world->width > 0) ? (output_world->rowbytes / output_world->width) : 0;
        if (bpp == sizeof(PF_PixelFloat)) {
            err = RenderGeneric<PF_PixelFloat>(in_data, out_data, pp, output_world);
        } else if (bpp == sizeof(PF_Pixel16)) {
            err = RenderGeneric<PF_Pixel16>(in_data, out_data, pp, output_world);
        } else {
            err = RenderGeneric<PF_Pixel>(in_data, out_data, pp, output_world);
        }
        
        // Checkin params
        for (int i = 1; i < RGBDELAY_NUM_PARAMS; ++i) {
            PF_Checkin_Param(in_data, out_data, i, &p[i]);
        }
    }
    
    if (wsP) suites.SPBasicSuite()->ReleaseSuite(kPFWorldSuite, kPFWorldSuiteVersion2);

    return err;
}

static PF_Err
ParamsSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;

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
        RGBDELAY_RED_DELAY);

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
        RGBDELAY_GREEN_DELAY);

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
        RGBDELAY_BLUE_DELAY);

    out_data->num_params = RGBDELAY_NUM_PARAMS;
    return PF_Err_NONE;
}

extern "C" DllExport
PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr,
    PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite * inSPBasicSuitePtr,
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

extern "C" DllExport
PF_Err EffectMain(PF_Cmd cmd,
    PF_InData * in_data,
    PF_OutData * out_data,
    PF_ParamDef * params[],
    PF_LayerDef * output,
    void* extra)
{
    PF_Err err = PF_Err_NONE;
    try {
        switch (cmd) {
        case PF_Cmd_ABOUT: err = About(in_data, out_data, params, output); break;
        case PF_Cmd_GLOBAL_SETUP: err = GlobalSetup(in_data, out_data, params, output); break;
        case PF_Cmd_PARAMS_SETUP: err = ParamsSetup(in_data, out_data, params, output); break;
        case PF_Cmd_RENDER: err = Render(in_data, out_data, params, output); break;
        case PF_Cmd_SMART_PRE_RENDER: err = PreRender(in_data, out_data, extra); break;
        case PF_Cmd_SMART_RENDER: err = SmartRender(in_data, out_data, extra); break;
        default: break;
        }
    }
    catch (...) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    return err;
}
