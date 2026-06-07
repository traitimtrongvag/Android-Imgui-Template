#pragma once
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <chrono>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "Includes/obfuscate.h"
#include "Includes/Logger.h"
#include "Global.h"
#include "Includes/Helper.h"

void Render(ImDrawList* draw) {
    if (!draw) return;
    
    char tmp_watermark[256];
    std::sprintf(tmp_watermark, OBFUSCATE(MOD_AUTHOR " - %.1f FPS"), ImGui::GetIO().Framerate);
    
    float t = ImGui::GetTime();
    int r = int((std::sin(t * 0.25f + 0.0f) * 0.5f + 0.5f) * 255);
    int g = int((std::sin(t * 0.25f + 2.0f) * 0.5f + 0.5f) * 255);
    int b = int((std::sin(t * 0.25f + 4.0f) * 0.5f + 0.5f) * 255);
    ImU32 rainbow_color = IM_COL32(r, g, b, 255);
    
    draw->AddText(ImVec2(40.0f, ImGui::GetIO().DisplaySize.y - 30.0f), rainbow_color, tmp_watermark);
}
