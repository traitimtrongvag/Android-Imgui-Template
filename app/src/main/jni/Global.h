#pragma once

#include <cmath>
#include <imgui.h>

/* Mod author name shown in the menu header and watermark overlay */
#define MOD_AUTHOR "Modded by Your Name"

/////////////////////////////////////////////////////////////////////////////
// Menu / UI flags                                                         //
/////////////////////////////////////////////////////////////////////////////
static bool HideIcon  = false;
static bool Volume    = true;   /* true = volume-key shortcut is active */
bool        setup     = false;
bool        ShowMenu  = false;
int         TabMenu   = 1;


/////////////////////////////////////////////////////////////////////////////
// Interpolation helpers                                                   //
/////////////////////////////////////////////////////////////////////////////

/* EaseOutBack - overshoots slightly then settles; good for button pop-in */
inline auto EaseOutBack = [](float t) -> float {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
};

/* EaseInOutCubic - smooth acceleration and deceleration */
inline auto EaseInOutCubic = [](float t) -> float {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
};

/* EaseOutElastic - spring-like bounce at the end */
inline auto EaseOutElastic = [](float t) -> float {
    const float c4 = (2.0f * 3.14159f) / 3.0f;
    return t == 0.0f ? 0.0f
         : t == 1.0f ? 1.0f
         : powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
};


/////////////////////////////////////////////////////////////////////////////
// Architecture-specific patch bytes                                       //
/////////////////////////////////////////////////////////////////////////////

#if defined(__aarch64__)

    const char* libEGL = "lib64EGL.so";
    #define LIB_INPUT_PATH "/system/lib64/libinput.so"
    const char* aarch  = "arm64-v8a - 64Bit";

    struct patches {
        /* Boolean */
        const char* BoolTrue  = OBFUSCATE("20 00 80 D2 C0 03 5F D6");
        const char* BoolFalse = OBFUSCATE("00 00 80 D2 C0 03 5F D6");

        /* Int */
        const char* Int0        = BoolFalse;
        const char* Int10       = OBFUSCATE("40 01 80 52 C0 03 5F D6");
        const char* Int100      = OBFUSCATE("80 0C 80 52 C0 03 5F D6");
        const char* Int1000     = OBFUSCATE("00 7D 80 52 C0 03 5F D6");
        const char* Int10000    = OBFUSCATE("00 E2 84 52 C0 03 5F D6");
        const char* IntInfinite = OBFUSCATE("00 E0 AF D2 C0 03 5F D6");

        /* Float */
        const char* Float0    = OBFUSCATE("E0 03 27 1E C0 03 5F D6");
        const char* Float1    = OBFUSCATE("00 10 2E 1E C0 03 5F D6");
        const char* Float10   = OBFUSCATE("00 90 24 1E C0 03 5F D6");
        const char* Float100  = OBFUSCATE("00 59 A8 52 00 00 27 1E C0 03 5F D6");
        const char* Float1000 = OBFUSCATE("40 8F A8 52 00 00 27 1E C0 03 5F D6");
        const char* Float2000 = OBFUSCATE("FA 04 04 E3 1E FF 2F E1");

        /* Double - credit: Billonz Offset Tester */
        const char* Double0        = OBFUSCATE("E003679EC0035FD6");
        const char* Double1        = OBFUSCATE("00106E1EC0035FD6");
        const char* Double10       = OBFUSCATE("0090641EC0035FD6");
        const char* Double100      = OBFUSCATE("800C8052C0035FD6");
        const char* Double1000     = OBFUSCATE("000080F20000A0F20000C8F2E011E8F20000679EC0035FD6");
        const char* Double10000    = OBFUSCATE("000080F20000A0F20000D1F20019E8F20000679EC0035FD6");
        const char* DoubleInfinite = OBFUSCATE("000080F20000A0F20040CDF2001FE8F20000679EC0035FD6");
    } HEXPATCH;

#else /* armeabi-v7a - 32-bit */

    const char* libEGL = "libEGL.so";
    #define LIB_INPUT_PATH "/system/lib/libinput.so"
    const char* aarch  = "armeabi-v7a - 32Bit";

    struct patches {
        /* Boolean */
        const char* BoolTrue  = OBFUSCATE("01 00 A0 E3 1E FF 2F E1");
        const char* BoolFalse = OBFUSCATE("00 00 A0 E3 1E FF 2F E1");

        /* Int */
        const char* Int0        = BoolFalse;
        const char* Int1        = BoolTrue;
        const char* Int10       = OBFUSCATE("0A 00 A0 E3 1E FF 2F E1");
        const char* Int100      = OBFUSCATE("64 00 00 E3 1E FF 2F E1");
        const char* Int1000     = OBFUSCATE("01 0A A0 E3 1E FF 2F E1");
        const char* Int10000    = OBFUSCATE("10 07 02 E3 1E FF 2F E1");
        const char* IntInfinite = OBFUSCATE("02 01 E0 E3 1E FF 2F E1");

        /* Float */
        const char* Float0    = OBFUSCATE("00 00 40 E3 1E FF 2F E1");
        const char* Float1    = OBFUSCATE("80 0F 43 E3 1E FF 2F E1");
        const char* Float10   = OBFUSCATE("20 01 04 E3 1E FF 2F E1");
        const char* Float100  = OBFUSCATE("C8 02 44 E3 1E FF 2F E1");
        const char* Float1000 = OBFUSCATE("7A 04 44 E3 1E FF 2F E1");
        const char* Float2000 = OBFUSCATE("FA 04 04 E3 1E FF 2F E1");

        /* Double - credit: Billonz Offset Tester */
        const char* Double0        = OBFUSCATE("000000E3001040E3100B41EC1EFF2FE1");
        const char* Double1        = OBFUSCATE("0000A0E3001000E3F01F43E3100B41EC1EFF2FE1");
        const char* Double10       = OBFUSCATE("0000A0E3001000E3241044E3100B41EC1EFF2FE1");
        const char* Double100      = OBFUSCATE("0000A0E3001000E3591044E3100B41EC1EFF2FE1");
        const char* Double1000     = OBFUSCATE("0000A0E3001004E38F1044E3100B41EC1EFF2FE1");
        const char* Double10000    = OBFUSCATE("0000A0E3001808E3C31044E3100B41EC1EFF2FE1");
        const char* DoubleInfinite = OBFUSCATE("000000E3C00F4FE3FF1F0FE3DF1144E3100B41EC1EFF2FE1");
    } HEXPATCH;

#endif
