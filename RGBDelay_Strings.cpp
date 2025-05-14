/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/*******************************************************************/

/*
    RGBDelay_Strings.cpp
*/

#include "RGBDelay.h"

typedef struct {
    unsigned long   index;
    const char* str;  // Changed from char* to const char*
} TableString;

TableString    g_strs[StrID_NUMTYPES] = {
    {StrID_NONE,                    ""},
    {StrID_Name,                    "RGBDelay"},
    {StrID_Description,             "Delays RGB channels separately.\rCopyright 2023 Adobe Inc."},
    {StrID_RedDelay_Param_Name,     "Red Delay"},
    {StrID_GreenDelay_Param_Name,   "Green Delay"},
    {StrID_BlueDelay_Param_Name,    "Blue Delay"},
    {StrID_DependString1,           "This effect requires"},
    {StrID_DependString2,           "framework data."},
    {StrID_Err_LoadSuite,           "Error loading suite."},
    {StrID_Err_FreeSuite,           "Error releasing suite."},
    {StrID_Exception,               "An exception occurred."}
};

char* STR(int strNum)
{
    // This cast is technically unsafe but needed for compatibility with SDK
    return const_cast<char*>(g_strs[strNum].str);
}