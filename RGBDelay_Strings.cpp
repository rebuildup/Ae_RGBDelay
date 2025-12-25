#include "RGBDelay_Strings.h"

typedef struct {
    unsigned long    index;
    char            str[256];
} TableString;

TableString g_strs[StrID_NUMTYPES] = {
    StrID_NONE,             "",
    StrID_Name,             "RGBDelay",
    StrID_Description,      "RGBごとにディレイをかけるサンプルプラグイン。\rCopyright 2023 okmr",
    StrID_RedDelay_Param_Name,   "Red Delay",
    StrID_GreenDelay_Param_Name, "Green Delay",
    StrID_BlueDelay_Param_Name,  "Blue Delay",
    StrID_DependString1,    "All Dependencies requested.",
    StrID_DependString2,    "Missing Dependencies requested.",
    StrID_Err_LoadSuite,    "Error loading suite.",
    StrID_Err_FreeSuite,    "Error releasing suite.",
    StrID_Exception,        "Caught an exception! Uh-oh, missing suite..."
};

extern "C" char* GetStringPtr(int strNum)
{
    return g_strs[strNum].str;
}
