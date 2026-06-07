#pragma once

/////////////////////////////////////////////////////////////////////////////
// MenuAnimation.h - Animated tab-button rendering for the mod menu       //
// Handles per-button scale, glow, click-wave, and staggered slide-in.   //
// Depends on: Global.h (EaseOutBack, buttonAnimTimes, clickWaveTime…)   //
/////////////////////////////////////////////////////////////////////////////

#include "../Global.h"
#include "../ImGui/Icon_FA7.h"

/////////////////////////////////////////////////////////////////////////////
// Menu animation state                                                    //
/////////////////////////////////////////////////////////////////////////////
inline float   menuAnimationProgress = 0.0f;
inline bool    menuOpening           = false;
inline bool    menuClosing           = false;
inline ImVec2  hideIconPos           = ImVec2(0, 0);
inline ImVec2  menuPosition          = ImVec2(0, 0);
inline ImVec2  menuOffset            = ImVec2(70, 0);
inline bool    isDraggingMenu        = false;
inline bool    shouldUpdateIconPosition = false;
inline ImVec2  newIconPosition;

/* Speed at which the menu opens/closes (units per second) */
inline const float ANIMATION_SPEED = 2.4f;


/////////////////////////////////////////////////////////////////////////////
// Tab button animation state                                              //
/////////////////////////////////////////////////////////////////////////////
inline float buttonAnimTimes[10] = {0.0f};
inline float tabSwitchAnim       = 1.0f;
inline int   lastActiveTab       = -1;
inline float clickWaveTime[10]   = {0.0f};
inline bool  isButtonClicked[10] = {false};

/////////////////////////////////////////////////////////////////////////////
// Button descriptor - label shown on tab + its tab index                 //
/////////////////////////////////////////////////////////////////////////////
struct ButtonData {
    const char* label;
    int         tabIndex;
};

/////////////////////////////////////////////////////////////////////////////
// DrawAnimatedButton                                                      //
// Renders a single tab button with:                                      //
//   * scale animation (active tab grows slightly)                        //
//   * glow effect behind the active button                               //
//   * click-wave flash on press                                          //
//   * vertical lift when active                                          //
// Returns true if the button was clicked this frame.                     //
/////////////////////////////////////////////////////////////////////////////
inline bool DrawAnimatedButton(
    const char* label,
    int         tabIndex,
    float       buttonScale,
    ImVec2      baseButtonSize,
    float       deltaTime,
    float       easedProgress)
{
    bool isActive = (TabMenu == tabIndex);

    /* Animate in/out the active-state highlight */
    if (isActive) {
        buttonAnimTimes[tabIndex] = std::min(buttonAnimTimes[tabIndex] + deltaTime * 5.0f, 1.0f);
    } else {
        buttonAnimTimes[tabIndex] = std::max(buttonAnimTimes[tabIndex] - deltaTime * 3.0f, 0.0f);
    }

    /* Decay click-wave after press */
    if (clickWaveTime[tabIndex] > 0.0f) {
        clickWaveTime[tabIndex] = std::max(clickWaveTime[tabIndex] - deltaTime * 3.0f, 0.0f);
    }

    float animProgress  = EaseOutBack(buttonAnimTimes[tabIndex]);
    float activeScale   = 1.0f + (animProgress * 0.08f);
    float clickScale    = 1.0f + (clickWaveTime[tabIndex] * 0.15f);
    float finalScale    = buttonScale * activeScale * clickScale;

    ImVec2 animatedButtonSize(baseButtonSize.x * finalScale, baseButtonSize.y * finalScale);

    /* Resolve button colours from current ImGui style */
    ImVec4 normalColor  = ImGui::GetStyle().Colors[ImGuiCol_Button];
    ImVec4 hoveredColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
    ImVec4 activeColor  = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
    ImVec4 currentColor;

    if (isActive) {
        float t    = animProgress;
        currentColor = ImVec4(
            hoveredColor.x + (activeColor.x - hoveredColor.x) * t,
            hoveredColor.y + (activeColor.y - hoveredColor.y) * t,
            hoveredColor.z + (activeColor.z - hoveredColor.z) * t,
            hoveredColor.w + (activeColor.w - hoveredColor.w) * t);
    } else {
        currentColor = normalColor;
    }

    /* Draw glow + click-wave decorations behind the button */
    if (animProgress > 0.01f) {
        ImDrawList* dl       = ImGui::GetWindowDrawList();
        ImVec2      buttonPos = ImGui::GetCursorScreenPos();
        float       glowSize  = animProgress * 6.0f;

        ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(activeColor.x, activeColor.y, activeColor.z, animProgress * 0.2f));
        dl->AddRectFilled(
            ImVec2(buttonPos.x - glowSize,                         buttonPos.y - glowSize),
            ImVec2(buttonPos.x + animatedButtonSize.x + glowSize,  buttonPos.y + animatedButtonSize.y + glowSize),
            glowColor, 8.0f);

        if (clickWaveTime[tabIndex] > 0.0f) {
            ImU32 waveColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(1.0f, 1.0f, 1.0f, clickWaveTime[tabIndex] * 0.3f));
            dl->AddRectFilled(
                ImVec2(buttonPos.x + 2,                        buttonPos.y + 2),
                ImVec2(buttonPos.x + animatedButtonSize.x - 2, buttonPos.y + animatedButtonSize.y - 2),
                waveColor, 6.0f);
        }
    }

    /* Lift active button slightly upward */
    float yOffset = isActive ? -animProgress * 2.0f : 0.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);

    ImGui::PushStyleColor(ImGuiCol_Button,        currentColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  activeColor);

    bool clicked = ImGui::Button(label, animatedButtonSize);
    if (clicked) {
        clickWaveTime[tabIndex] = 1.0f;
        isButtonClicked[tabIndex] = true;
    }

    ImGui::PopStyleColor(3);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - yOffset);
    return clicked;
}


/////////////////////////////////////////////////////////////////////////////
// RenderTabBar                                                            //
// Draws the full row of tab buttons with staggered slide-in animation.   //
// Updates TabMenu when a button is clicked.                              //
// buttons[]  - array of {label, tabIndex} descriptors                   //
// numButtons - length of buttons[] (derived from the array, not manual) //
// easedProgress, deltaTime - from the caller's animation state          //
/////////////////////////////////////////////////////////////////////////////
inline void RenderTabBar(
    const ButtonData* buttons,
    int               numButtons,
    float             easedProgress,
    float             deltaTime,
    float             availableWidth)
{
    /* Update tab-switch animation */
    if (lastActiveTab != TabMenu && lastActiveTab != -1) tabSwitchAnim = 0.0f;
    lastActiveTab = TabMenu;
    if (tabSwitchAnim < 1.0f) tabSwitchAnim = std::min(tabSwitchAnim + deltaTime * 4.0f, 1.0f);

    const float spacing          = 3.0f;
    const float buttonHeightActual = 45.0f;
    float       buttonWidth      = (availableWidth - (spacing * (numButtons - 1))) / numButtons;
    float       buttonScale      = 0.95f + (easedProgress * 0.05f);
    float       buttonAlpha      = 0.3f  + (easedProgress * 0.7f);
    ImVec2      baseButtonSize(buttonWidth, buttonHeightActual);

    /* Scale font down if buttons are narrow */
    float textScaleFactor = 1.0f;
    if (buttonWidth < 160.0f)
        textScaleFactor = std::max(0.65f, buttonWidth / 160.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,        buttonAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 8));
    ImGui::SetWindowFontScale(textScaleFactor);

    for (int i = 0; i < numButtons; i++) {
        /* Stagger each button's slide-in by 50 ms */
        float staggerDelay      = i * 0.05f;
        float adjustedProgress  = std::max(0.0f, easedProgress - staggerDelay) / (1.0f - staggerDelay);
        adjustedProgress        = std::min(adjustedProgress, 1.0f);
        float slideOffset       = (1.0f - EaseOutBack(adjustedProgress)) * 30.0f;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + slideOffset);
        if (DrawAnimatedButton(buttons[i].label, buttons[i].tabIndex,
                               buttonScale, baseButtonSize, deltaTime, easedProgress)) {
            TabMenu = buttons[i].tabIndex;
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - slideOffset);

        if (i < numButtons - 1) ImGui::SameLine(0, spacing);
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar(2);
}
