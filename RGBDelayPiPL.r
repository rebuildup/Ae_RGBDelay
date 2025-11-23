#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
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
            "361do_plugins"
        },
#ifdef AE_OS_WIN
    #ifdef AE_PROC_INTELx64
        CodeWin64X86 {"EffectMain"},
    #endif
#else
            8
        },
        /* [13] */
        AE_Effect_Support_URL {
            "https://github.com/rebuildup/Ae_RGBDelay"
        }
    }
};
