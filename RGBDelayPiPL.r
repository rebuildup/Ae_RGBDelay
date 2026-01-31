#include "AEConfig.h"
#include "AE_EffectVers.h"

/* Include AE_General.r for resource definitions on Mac */
#ifdef AE_OS_MAC
	#include <AE_General.r>
#endif

#if defined(__MACH__) && !defined(AE_OS_MAC)
	#define AE_OS_MAC 1
	#include <AE_General.r>
#endif
    
resource 'PiPL' (16000) {
    {    /* array properties: 12 elements */
        /* [1] */
        Kind {
            AEEffect
        },
        /* [2] */
        Name {
            "RGBDelay"
        },
        /* [3] */
        Category {
            "361do"
        },
#ifdef AE_OS_WIN
    #ifdef AE_PROC_INTELx64
        CodeWin64X86 {"EffectMain"},
    #endif
#else
    #ifdef AE_OS_MAC
        CodeMacIntel64 {"EffectMain"},
        CodeMacARM64 {"EffectMain"},
    #endif
#endif
        /* [6] */
        AE_PiPL_Version {
            2,
            0
        },
        /* [7] */
        AE_Effect_Spec_Version {
            PF_PLUG_IN_VERSION,
            PF_PLUG_IN_SUBVERS
        },
        /* [8] */
        AE_Effect_Version {
            524289    /* Calculated from version macros in RGBDelay.h:
                        MAJOR_VERSION=1, MINOR_VERSION=0, BUG_VERSION=0,
                        STAGE_VERSION=PF_Stage_DEVELOP, BUILD_VERSION=1 */
        },
        /* [9] */
        AE_Effect_Info_Flags {
            0
        },
        /* [10] */
        /* Must match GlobalSetup() in RGBDelay.cpp:
           PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PIX_INDEPENDENT | PF_OutFlag_WIDE_TIME_INPUT */
        AE_Effect_Global_OutFlags {
            0x02000402
        },
        /* Must match GlobalSetup() in RGBDelay.cpp:
           PF_OutFlag2_SUPPORTS_THREADED_RENDERING */
        AE_Effect_Global_OutFlags_2 {
            0x08000000
        },
        /* [11] */
        AE_Effect_Match_Name {
            "361do_RGBDelay"
        },
        /* [12] */
        AE_Reserved_Info {
            0
        },
        /* [13] */
        AE_Effect_Support_URL {
            "https://github.com/rebuildup/Ae_RGBDelay"
        }
    }
};
