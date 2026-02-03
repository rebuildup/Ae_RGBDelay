#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "Param_Utils.h"
#include "AE_Macros.h"
#include "String_Utils.h"
#include "RGBDelay.h"
#include <climits>

// Zero pixel constants (alpha, red, green, blue order per PF_Pixel definition)
static const PF_Pixel kZeroPixel8 = {0, 0, 0, 0};
static const PF_Pixel16 kZeroPixel16 = {0, 0, 0, 0};

// Maximum number of cached layer sources (one per RGB channel)
// We only need at most 3 unique time samples since we only have R/G/B delays
constexpr int kMaxChannelSources = 3;

struct RGBDelayIterateRefcon {
    const char* r_base{};
    const char* g_base{};
    const char* b_base{};
    A_long r_rowbytes{};
    A_long g_rowbytes{};
    A_long b_rowbytes{};
    A_long r_width{};
    A_long r_height{};
    A_long g_width{};
    A_long g_height{};
    A_long b_width{};
    A_long b_height{};
    // Offset from output local coords -> source local coords.
    // src_x = out_x + src_off_x, src_y = out_y + src_off_y
    A_long r_off_x{};
    A_long r_off_y{};
    A_long g_off_x{};
    A_long g_off_y{};
    A_long b_off_x{};
    A_long b_off_y{};
    PF_Boolean r_fast{ FALSE };
    PF_Boolean g_fast{ FALSE };
    PF_Boolean b_fast{ FALSE };
};

// Template helper function for pixel access
template<typename PixelType>
static inline const PixelType* get_channel_ptr(
    const char* base, A_long rowbytes, A_long width, A_long height,
    A_long off_x, A_long off_y, PF_Boolean fast, A_long x, A_long y)
{
    if (fast) {
        if (y < height && x < width) {
            // Check for overflow in y * rowbytes calculation
            if (y > 0 && rowbytes > LONG_MAX / y) {
                return nullptr;  // Overflow would occur
            }
            return reinterpret_cast<const PixelType*>(base + y * rowbytes) + x;
        }
        return nullptr;
    } else {
        const A_long src_x = x + off_x;
        const A_long src_y = y + off_y;
        if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
            // Check for overflow in src_y * rowbytes calculation
            if (src_y > 0 && rowbytes > LONG_MAX / src_y) {
                return nullptr;  // Overflow would occur
            }
            return reinterpret_cast<const PixelType*>(base + src_y * rowbytes) + src_x;
        }
        return nullptr;
    }
}

// Template iterate function for both 8-bit and 16-bit processing
template<typename PixelType, const PixelType* ZeroPixel>
static PF_Err RGBDelayIterateT(void* refconP, A_long x, A_long y, PixelType* inP, PixelType* outP)
{
    (void)inP;
    const RGBDelayIterateRefcon* rc = reinterpret_cast<const RGBDelayIterateRefcon*>(refconP);

    const PixelType* r = get_channel_ptr<PixelType>(
        rc->r_base, rc->r_rowbytes, rc->r_width, rc->r_height,
        rc->r_off_x, rc->r_off_y, rc->r_fast, x, y);
    const PixelType* g = get_channel_ptr<PixelType>(
        rc->g_base, rc->g_rowbytes, rc->g_width, rc->g_height,
        rc->g_off_x, rc->g_off_y, rc->g_fast, x, y);
    const PixelType* b = get_channel_ptr<PixelType>(
        rc->b_base, rc->b_rowbytes, rc->b_width, rc->b_height,
        rc->b_off_x, rc->b_off_y, rc->b_fast, x, y);

    const PixelType r0 = r ? *r : *ZeroPixel;
    const PixelType g0 = g ? *g : *ZeroPixel;
    const PixelType b0 = b ? *b : *ZeroPixel;

    outP->red = r0.red;
    outP->green = g0.green;
    outP->blue = b0.blue;

    // Use max alpha for both 8-bit and 16-bit for consistency.
    // Taking the maximum preserves transparency correctly - if any channel source
    // is transparent at a pixel, the output should reflect that transparency.
    // Using sum could incorrectly make transparent areas opaque.
    outP->alpha = MAX(r0.alpha, MAX(g0.alpha, b0.alpha));

    return PF_Err_NONE;
}

// Thin wrapper for 8-bit iteration
static PF_Err RGBDelayIterate8(void* refconP, A_long x, A_long y, PF_Pixel* inP, PF_Pixel* outP)
{
    return RGBDelayIterateT<PF_Pixel, &kZeroPixel8>(refconP, x, y, inP, outP);
}

// Thin wrapper for 16-bit iteration
static PF_Err RGBDelayIterate16(void* refconP, A_long x, A_long y, PF_Pixel16* inP, PF_Pixel16* outP)
{
    return RGBDelayIterateT<PF_Pixel16, &kZeroPixel16>(refconP, x, y, inP, outP);
}

// Helper function for safe subtraction with overflow check
inline bool safe_sub(A_long a, A_long b, A_long* result) {
    // Check for overflow: if a and b have different signs, subtraction may overflow
    if ((b >= 0 && a < LONG_MIN + b) || (b < 0 && a > LONG_MAX + b)) {
        return false;  // Overflow would occur
    }
    *result = a - b;
    return true;
}

// Helper to compute fast path flag - true when source covers output completely
// off_x/off_y = output_origin - source_origin (can be positive or negative)
// For fast path, we need: source covers output area
inline PF_Boolean compute_fast_flag(A_long off_x, A_long off_y, A_long src_width, A_long src_height, A_long out_width, A_long out_height)
{
    // Fast path requires source to completely cover output region
    // When off_x <= 0: source starts at or before output (to the left/above)
    // When off_y <= 0: source starts at or before output (above)

    // Check basic conditions first
    if (off_x > 0 || off_y > 0) {
        return FALSE;
    }

    // Check for overflow in subtraction before comparison
    // If off_x is very negative, out_width - off_x could overflow
    A_long required_width;
    if (off_x < 0 && out_width > LONG_MAX + off_x) {
        // Would overflow: required width exceeds maximum
        return FALSE;
    }
    required_width = out_width - off_x;

    A_long required_height;
    if (off_y < 0 && out_height > LONG_MAX + off_y) {
        // Would overflow: required height exceeds maximum
        return FALSE;
    }
    required_height = out_height - off_y;

    // src_width >= out_width - off_x: source is wide enough to cover output
    // src_height >= out_height - off_y: source is tall enough to cover output
    return (src_width >= required_width && src_height >= required_height) ? TRUE : FALSE;
}

// Parameter setup

static PF_Err GlobalSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);
    // Tell the host that we sample frames at times other than the one being rendered.
    // Without PF_OutFlag_WIDE_TIME_INPUT, AE may reuse cached frames even after the
    // delay sliders change, leaving old imagery visible.
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE |
        PF_OutFlag_PIX_INDEPENDENT |
        PF_OutFlag_WIDE_TIME_INPUT;
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return err;
}

static PF_Err ParamsSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);

    PF_ADD_SLIDER("Red Delay (frames)", RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, -1, RED_DELAY_DISK_ID);
    PF_ADD_SLIDER("Green Delay (frames)", RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, -2, GREEN_DELAY_DISK_ID);
    PF_ADD_SLIDER("Blue Delay (frames)", RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, RGBDELAY_AMOUNT_MIN, RGBDELAY_AMOUNT_MAX, -3, BLUE_DELAY_DISK_ID);

    out_data->num_params = RGBDELAY_NUM_PARAMS;
    return err;
}

// Rendering function
static PF_Err Render(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* outputP)
{
    PF_Err err = PF_Err_NONE;

    // Get parameter values
    A_long red_delay = params[RGBDELAY_RED_DELAY]->u.sd.value;
    A_long green_delay = params[RGBDELAY_GREEN_DELAY]->u.sd.value;
    A_long blue_delay = params[RGBDELAY_BLUE_DELAY]->u.sd.value;

    // Validate time_step to prevent division by zero and invalid calculations
    if (in_data->time_step <= 0) {
        return in_data->current_time;  // Return current time as fallback
    }

    // Calculate source times safely (avoid overflow/underflow)
    auto safe_time_calc = [&](A_long delay) -> A_long {
        if (delay == 0) {
            return in_data->current_time;
        }

        // Check for potential overflow in multiplication
        // delay is in range [-100, 100], time_step could be large
        A_long abs_delay = (delay >= 0) ? delay : -delay;
        if (abs_delay > 0 && in_data->time_step > 0 &&
            abs_delay > LONG_MAX / in_data->time_step) {
            // Overflow would occur, clamp to maximum safe delay
            abs_delay = LONG_MAX / in_data->time_step;
        }

        if (delay >= 0) {
            // Positive delay: time goes backwards
            A_long step_time = abs_delay * in_data->time_step;
            if (step_time > in_data->current_time) {
                return 0;  // Would underflow
            }
            A_long result = in_data->current_time - step_time;
            return (result < 0) ? 0 : result;
        } else {
            // Negative delay: time goes forwards
            A_long step_time = abs_delay * in_data->time_step;
            A_long max_valid = (in_data->total_time > in_data->time_step)
                ? in_data->total_time - in_data->time_step
                : 0;
            if (max_valid - in_data->current_time < step_time) {
                return max_valid;  // Would overflow
            }
            A_long result = in_data->current_time + step_time;
            return (result > max_valid) ? max_valid : result;
        }
    };

    A_long red_time = safe_time_calc(red_delay);
    A_long green_time = safe_time_calc(green_delay);
    A_long blue_time = safe_time_calc(blue_delay);

    struct CheckedOutSource {
        A_long time{};
        PF_ParamDef param{};
        PF_Boolean checked_out{ FALSE };
    };

    CheckedOutSource sources[kMaxChannelSources] = {};
    int sources_count = 0;

    // Validate input parameter
    if (!params || !params[RGBDELAY_INPUT]) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    const PF_LayerDef* input_ld = &params[RGBDELAY_INPUT]->u.ld;
    const PF_LayerDef* red_ld = nullptr;
    const PF_LayerDef* green_ld = nullptr;
    const PF_LayerDef* blue_ld = nullptr;

    auto resolve_layer_at_time = [&](A_long t, const PF_LayerDef** out_ld) -> PF_Err {
        if (t == in_data->current_time) {
            *out_ld = input_ld;
            return PF_Err_NONE;
        }

        for (int i = 0; i < sources_count; i++) {
            if (sources[i].time == t) {
                *out_ld = &sources[i].param.u.ld;
                return PF_Err_NONE;
            }
        }

        if (sources_count >= kMaxChannelSources) {
            // Should never happen since we only have 3 channels (RGB)
            // This check is a safety guard for future modifications
            return PF_Err_INTERNAL_STRUCT_DAMAGED;
        }

        sources[sources_count].time = t;
        AEFX_CLR_STRUCT(sources[sources_count].param);
        ERR(PF_CHECKOUT_PARAM(in_data, RGBDELAY_INPUT, t, in_data->time_step, in_data->time_scale, &sources[sources_count].param));
        // Only mark as checked out and continue if checkout succeeded
        if (!err) {
            sources[sources_count].checked_out = TRUE;
            *out_ld = &sources[sources_count].param.u.ld;
            sources_count++;
        }

        return err;
    };

    // Resolve layer sources for each channel
    // Stop on first error to avoid unnecessary checkouts
    ERR(resolve_layer_at_time(red_time, &red_ld));
    if (!err) {
        ERR(resolve_layer_at_time(green_time, &green_ld));
    }
    if (!err) {
        ERR(resolve_layer_at_time(blue_time, &blue_ld));
    }

    // Only render if all checkouts succeeded
    // After this check, red_ld, green_ld, and blue_ld are guaranteed non-null
    if (!err && red_ld && green_ld && blue_ld) {
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        auto* iterate8 = suites.Iterate8Suite1();
        auto* iterate16 = suites.Iterate16Suite1();

        if (!iterate8 || !iterate16) {
            return PF_Err_OUT_OF_MEMORY;
        }

        PF_Rect area{0, 0, outputP->width, outputP->height};

        RGBDelayIterateRefcon rc{};
        // Layer pointers are guaranteed non-null after the check above
        rc.r_base = red_ld->data ? reinterpret_cast<const char*>(red_ld->data) : nullptr;
        rc.g_base = green_ld->data ? reinterpret_cast<const char*>(green_ld->data) : nullptr;
        rc.b_base = blue_ld->data ? reinterpret_cast<const char*>(blue_ld->data) : nullptr;
        rc.r_rowbytes = red_ld->rowbytes;
        rc.g_rowbytes = green_ld->rowbytes;
        rc.b_rowbytes = blue_ld->rowbytes;
        rc.r_width = red_ld->width;
        rc.r_height = red_ld->height;
        rc.g_width = green_ld->width;
        rc.g_height = green_ld->height;
        rc.b_width = blue_ld->width;
        rc.b_height = blue_ld->height;

        // Calculate offsets with overflow protection
        // If overflow occurs, skip rendering for that channel (treat as out of bounds)
        if (!safe_sub(outputP->origin_x, red_ld->origin_x, &rc.r_off_x) ||
            !safe_sub(outputP->origin_y, red_ld->origin_y, &rc.r_off_y) ||
            !safe_sub(outputP->origin_x, green_ld->origin_x, &rc.g_off_x) ||
            !safe_sub(outputP->origin_y, green_ld->origin_y, &rc.g_off_y) ||
            !safe_sub(outputP->origin_x, blue_ld->origin_x, &rc.b_off_x) ||
            !safe_sub(outputP->origin_y, blue_ld->origin_y, &rc.b_off_y)) {
            // Offset overflow occurred - this is extremely rare but possible with extreme coordinate values
            // Fall back to simple copy from current time layer
            ERR(PF_COPY(const_cast<PF_LayerDef*>(red_ld), outputP, nullptr, nullptr));
            goto cleanup;
        }

        rc.r_fast = compute_fast_flag(rc.r_off_x, rc.r_off_y, rc.r_width, rc.r_height, outputP->width, outputP->height);
        rc.g_fast = compute_fast_flag(rc.g_off_x, rc.g_off_y, rc.g_width, rc.g_height, outputP->width, outputP->height);
        rc.b_fast = compute_fast_flag(rc.b_off_x, rc.b_off_y, rc.b_width, rc.b_height, outputP->width, outputP->height);

        if (PF_WORLD_IS_DEEP(outputP)) {
            // Fast path for 16-bit: all channels sample same source with no offset AND matching size
            if (red_ld == green_ld && red_ld == blue_ld &&
                rc.r_off_x == 0 && rc.r_off_y == 0 &&
                rc.r_width == outputP->width && rc.r_height == outputP->height) {
                ERR(PF_COPY(const_cast<PF_LayerDef*>(red_ld), outputP, nullptr, nullptr));
            } else {
                err = iterate16->iterate(
                    in_data,
                    0,
                    outputP->height,
                    const_cast<PF_LayerDef*>(red_ld),
                    &area,
                    &rc,
                    RGBDelayIterate16,
                    outputP);
            }
        } else {
            // Fast path for 8-bit: all channels sample same source with no offset AND matching size
            if (red_ld == green_ld && red_ld == blue_ld &&
                rc.r_off_x == 0 && rc.r_off_y == 0 &&
                rc.r_width == outputP->width && rc.r_height == outputP->height) {
                ERR(PF_COPY(const_cast<PF_LayerDef*>(red_ld), outputP, nullptr, nullptr));
            } else {
                err = iterate8->iterate(
                    in_data,
                    0,
                    outputP->height,
                    const_cast<PF_LayerDef*>(red_ld),
                    &area,
                    &rc,
                    RGBDelayIterate8,
                    outputP);
            }
        }
    }  // End of render block

cleanup:
    // Checkin (only checked out sources)
    for (int i = sources_count - 1; i >= 0; i--) {
        if (sources[i].checked_out) {
            PF_CHECKIN_PARAM(in_data, &sources[i].param);
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
        RGBDELAY_NAME,          // Name
        RGBDELAY_MATCH_NAME,    // Match Name
        RGBDELAY_CATEGORY,      // Category
        AE_RESERVED_INFO,       // Reserved Info
        "EffectMain",           // Entry point
        RGBDELAY_SUPPORT_URL    // Support URL
    );

    return result;
}

extern "C" DllExport
PF_Err EffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
    PF_Err err = PF_Err_NONE;

    switch (cmd) {
    case PF_Cmd_ABOUT: {
        const char* info =
            "RGBDelay v1.0.0\n"
            "Copyright (C) 2024 Tsuyoshi Okumura/Hotkey ltd.\n"
            "All Rights Reserved.\n"
            "\n"
            "This software is provided \"as is\" without warranty of any kind.\n"
            "Use at your own risk.\n";
        PF_SPRINTF(out_data->return_msg, "%s", info);
        break;
    }
    case PF_Cmd_GLOBAL_SETUP:
        err = GlobalSetup(in_data, out_data, params, output);
        break;
    case PF_Cmd_PARAMS_SETUP:
        err = ParamsSetup(in_data, out_data, params, output);
        break;
    case PF_Cmd_RENDER:
        err = Render(in_data, out_data, params, output);
        break;
    }

    return err;
}
