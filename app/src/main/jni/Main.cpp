#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

#include "AnAn/Call_Me.h"
#include "Headers.h"

#include "SaveLoadMenu.h"
#include "Global.h"
#include "Logo.h"
#include "Login.h"

#define TARGET_LIB oxorany("libil2cpp.so")

/////////////////////////////////////////////////////////////////////////////
// Login / session state (file-scope)                                      //
/////////////////////////////////////////////////////////////////////////////
static bool  isLogin              = true;
static bool  showLoginSuccess     = false;
static float progress             = 1.0f;
static auto  startTime            = std::chrono::steady_clock::now();

static bool  attemptedAutoLogin   = false; 

void ShowLoginSuccess() {
    static float animProgress = 0.0f;
    static bool  closing      = false;
    static bool  wasShowing   = false;

    ImGuiIO& io = ImGui::GetIO();

    if (showLoginSuccess && !wasShowing) {
        animProgress = 0.0f;
        closing      = false;
    }
    wasShowing = showLoginSuccess;

    const float openSpeed  = 2.8f;
    const float closeSpeed = 3.5f;

    if (!closing) {
        animProgress += io.DeltaTime * openSpeed;
        if (animProgress > 1.0f) animProgress = 1.0f;
    } else {
        animProgress -= io.DeltaTime * closeSpeed;
        if (animProgress <= 0.0f) {
            animProgress     = 0.0f;
            closing          = false;
            wasShowing       = false;
            showLoginSuccess = false;
            return;
        }
    }

    if (animProgress <= 0.0f) return;

    float t = animProgress * animProgress * (3.0f - 2.0f * animProgress);

    const float fullW = 400.0f;
    const float fullH = 180.0f;
    const float kBW   = 100.0f;
    const float kBH   = 40.0f;

    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 wPos(center.x - fullW * 0.5f, center.y - fullH * 0.5f);
    ImVec2 wEnd(wPos.x + fullW, wPos.y + fullH);

    float halfVisible = (fullW * 0.5f) * t;
    float clipX0 = center.x - halfVisible;
    float clipX1 = center.x + halfVisible;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->PushClipRect(ImVec2(clipX0, wPos.y), ImVec2(clipX1, wEnd.y), false);

    dl->AddRectFilled(wPos, wEnd,
        ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.15f, 0.95f)), 10.0f);
    dl->AddRect(wPos, wEnd,
        ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.5f, 1.0f)), 10.0f, 0, 2.0f);

    if (t > 0.4f) {
        float alpha = (t - 0.4f) / 0.6f;
        if (alpha > 1.0f) alpha = 1.0f;

        const char* msg = OBFUSCATE("Login successful!");
        ImVec2 mSz = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(wPos.x + (fullW - mSz.x) * 0.5f, wPos.y + 28.0f),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)), msg);

        const char* title = OBFUSCATE("Welcom to Menu Mod");
        ImVec2 tSz = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(wPos.x + (fullW - tSz.x) * 0.5f, wPos.y + 76.0f),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)), title);

        ImVec2 btnMin(center.x - kBW * 0.5f, wPos.y + 128.0f);
        ImVec2 btnMax(btnMin.x + kBW, btnMin.y + kBH);

        bool hovered = ImGui::IsMouseHoveringRect(btnMin, btnMax, false);
        ImVec4 btnCol = hovered
            ? ImVec4(0.35f, 0.35f, 0.6f, alpha)
            : ImVec4(0.25f, 0.25f, 0.45f, alpha);
        dl->AddRectFilled(btnMin, btnMax, ImGui::GetColorU32(btnCol), 6.0f);
        dl->AddRect(btnMin, btnMax,
            ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.8f, alpha)), 6.0f, 0, 1.5f);

        const char* okLabel = OBFUSCATE("OK");
        ImVec2 okSz = ImGui::CalcTextSize(okLabel);
        dl->AddText(
            ImVec2(btnMin.x + (kBW - okSz.x) * 0.5f, btnMin.y + (kBH - okSz.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)), okLabel);
    }

    dl->PopClipRect();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin(OBFUSCATE("##LoginSuccessHitArea"), nullptr,
        ImGuiWindowFlags_NoTitleBar      | ImGuiWindowFlags_NoResize       |
        ImGuiWindowFlags_NoMove          | ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoScrollbar     | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoInputs * (t <= 0.4f)); 
    if (t > 0.4f) {
        ImVec2 btnMin(center.x - kBW * 0.5f, wPos.y + 128.0f);
        ImGui::SetCursorScreenPos(btnMin);
        if (ImGui::InvisibleButton(OBFUSCATE("##OKBtn"), ImVec2(kBW, kBH)))
            closing = true;
    }

    ImGui::End();
}

EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

EGLBoolean _eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {

    /* Update drawable dimensions each frame */
    eglQuerySurface(dpy, surface, EGL_WIDTH,  &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);
    if (glWidth > 0 && glHeight > 0) {
        if (Width  == 0) Width  = glWidth;
        if (Height == 0) Height = glHeight;
    }

    /////////////////////////////////////////////////////////////////////////
    // One-time ImGui setup                                                //
    /////////////////////////////////////////////////////////////////////////
    if (!setup) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        DrawImGuiStyle();

        /* Primary font - Vietnamese glyph range */
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<std::uint8_t*>(Custom), sizeof(Custom),
            25.f, NULL, io.Fonts->GetGlyphRangesVietnamese());

        /* Font Awesome 5 icon font (merged) */
        static const ImWchar fa5_ranges[] = {0xe000, 0xf8ff, 0};
        ImFontConfig fa5_config;
        fa5_config.MergeMode   = true;
        fa5_config.PixelSnapH  = true;
        fa5_config.OversampleH = 2.3f;
        fa5_config.OversampleV = 2.3f;
        io.Fonts->AddFontFromMemoryCompressedTTF(
            font_awesome_data, font_awesome_size, 25.f, &fa5_config, fa5_ranges);

        /* Font Awesome 7 solid icon font (merged) */
        static const ImWchar fa7_ranges[] = {ICON_MIN_FA7, ICON_MAX_FA7, 0};
        ImFontConfig fa7_config;
        fa7_config.MergeMode   = true;
        fa7_config.PixelSnapH  = true;
        fa7_config.OversampleH = 2.3f;
        fa7_config.OversampleV = 2.3f;
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned char*>(fa7_solid_data), fa7_solid_data_size,
            25.f, &fa7_config, fa7_ranges);

        /* Android key mappings for ImGui text-input */
        io.KeyMap[ImGuiKey_UpArrow]    = 19;
        io.KeyMap[ImGuiKey_DownArrow]  = 20;
        io.KeyMap[ImGuiKey_LeftArrow]  = 21;
        io.KeyMap[ImGuiKey_RightArrow] = 22;
        io.KeyMap[ImGuiKey_Enter]      = 66;
        io.KeyMap[ImGuiKey_Backspace]  = 67;
        io.KeyMap[ImGuiKey_Escape]     = 111;
        io.KeyMap[ImGuiKey_Delete]     = 112;
        io.KeyMap[ImGuiKey_Home]       = 122;
        io.KeyMap[ImGuiKey_End]        = 123;

        ImGui_ImplOpenGL3_Init(OBFUSCATE("#version 300 es"));
        ImGui::GetStyle().ScaleAllSizes(3.0f);
        LoadSaveLoadMenu();
        setup = true;
    }

    /////////////////////////////////////////////////////////////////////////
    // Begin new ImGui frame                                               //
    /////////////////////////////////////////////////////////////////////////
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(glWidth, glHeight);
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    /////////////////////////////////////////////////////////////////////////
    // Volume-key shortcut - poll every 200 ms                            //
    /////////////////////////////////////////////////////////////////////////
    static float volumeCheckTimer   = 0.0f;
    static int   g_lastVolume       = -1;
    static bool  g_volumeCheckerInit = false;
    volumeCheckTimer += io.DeltaTime;
    if (volumeCheckTimer >= 0.2f) {
        volumeCheckTimer = 0.0f;
        int currentVolume = getCurrentVolume();
        if (currentVolume >= 0) {
            if (!g_volumeCheckerInit) {
                g_lastVolume      = currentVolume;
                g_volumeCheckerInit = true;
            } else {
                if (currentVolume > g_lastVolume && Volume) {
                    /* Volume UP → open menu */
                    if (!ShowMenu) {
                        ShowMenu              = true;
                        menuOpening           = true;
                        menuClosing           = false;
                        menuAnimationProgress = 0.0f;
                        menuPosition          = ImVec2(hideIconPos.x + menuOffset.x,
                                                       hideIconPos.y + menuOffset.y);
                    }
                } else if (currentVolume < g_lastVolume && Volume) {
                    /* Volume DOWN → close menu */
                    if (ShowMenu) {
                        ShowMenu    = false;
                        menuClosing = true;
                        menuOpening = false;
                    }
                }
                g_lastVolume = currentVolume;
            }
        }
    }

    /* Show soft keyboard when ImGui requests text input */
    static bool WantTextInputLast = false;
    if (io.WantTextInput && !WantTextInputLast) ShowSoftKeyboardInput();
    WantTextInputLast = io.WantTextInput;

    /////////////////////////////////////////////////////////////////////////
    // Key loading - runs once per session                                //
    /////////////////////////////////////////////////////////////////////////
    static bool sessionInitialized = false;
    if (!sessionInitialized) {
        loadKey();
        sessionInitialized = true;
    }

    if (showLoginSuccess) ShowLoginSuccess();

    /////////////////////////////////////////////////////////////////////////
    // Login flow                                                          //
    /////////////////////////////////////////////////////////////////////////
    if (!isLogin) {
        static bool         isLoggingIn = false;
        static std::thread  loginThread;
        static std::string  err;

        /* Auto-login with saved key on first frame */
        if (!attemptedAutoLogin) {
            attemptedAutoLogin = true;
            ImGui::OpenPopup(OBFUSCATE("##LoginPage"));

            if (g_s[0] != '\0' && !isLoggingIn) {
                isLoggingIn = true;
                if (loginThread.joinable()) loginThread.join();
                loginThread = std::thread([&]() {
                    err = Login(g_s);
                    if (err == std::string(OBFUSCATE("OK"))) {
                        isLogin          = bValid;
                        startTime        = std::chrono::steady_clock::now();
                        showLoginSuccess = true;
                    }
                    isLoggingIn = false;
                });
                loginThread.detach();
            }
        }

        /* Login modal */
        if (ImGui::BeginPopupModal(OBFUSCATE("##LoginPage"), NULL,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {

            ImGui::SetWindowSize(ImVec2(600, 500), ImGuiCond_Always);

            /* Centred title */
            const char* titleText  = OBFUSCATE("Please enter key");
            float       titleWidth = ImGui::CalcTextSize(titleText).x;
            float       winWidth   = ImGui::GetWindowContentRegionWidth();
            ImGui::SetCursorPosX((winWidth - titleWidth) * 0.5f + ImGui::GetStyle().WindowPadding.x);
            ImGui::Text(OBFUSCATE("%s"), titleText);

            /* Key input + Paste button */
            float buttonWidth = ImGui::CalcTextSize(OBFUSCATE("Paste")).x
                              + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::PushItemWidth(winWidth - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
            ImGui::BeginDisabled(isLoggingIn);
            ImGui::InputText(OBFUSCATE("##key"), g_s, sizeof(g_s));
            ImGui::EndDisabled();
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button(OBFUSCATE("Paste"), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
                auto key = getClipboard();
                strncpy(g_s, key.c_str(), sizeof(g_s) - 1);
                g_s[sizeof(g_s) - 1] = '\0';
            }

            /* Login / Loading / Retry button label */
            static float loginTimer = 0.0f;
            std::string  buttonText;
            if (isLoggingIn) {
                loginTimer += io.DeltaTime;
                static float loadingTimer = 0.0f;
                loadingTimer += io.DeltaTime;
                int dots = (int)(loadingTimer * 2.0f) % 4;
                buttonText = static_cast<const char*>(OBFUSCATE("Loading"));
                for (int i = 0; i < dots; i++) buttonText += ".";
                for (int i = dots; i < 3;    i++) buttonText += " ";
                if (loginTimer >= 60.0f) {
                    isLoggingIn = false;
                    err         = std::string(OBFUSCATE("timeout"));
                }
            } else {
                buttonText = (loginTimer >= 60.0f && err == std::string(OBFUSCATE("timeout")))
                    ? static_cast<const char*>(OBFUSCATE("Retry"))
                    : static_cast<const char*>(OBFUSCATE("Login"));
            }

            if (ImGui::Button(buttonText.c_str(), ImVec2(winWidth, 0)) && !isLoggingIn) {
                isLoggingIn = true;
                loginTimer  = 0.0f;
                err.clear();
                if (loginThread.joinable()) loginThread.join();
                loginThread = std::thread([&]() {
                    err = Login(g_s);
                    if (err == std::string(OBFUSCATE("OK"))) {
                        isLogin          = bValid;
                        saveKey();
                        startTime        = std::chrono::steady_clock::now();
                        showLoginSuccess = true;
                        ImGui::CloseCurrentPopup();
                    }
                    isLoggingIn = false;
                });
                loginThread.detach();
            }

            /* Status messages */
            if (isLoggingIn && loginTimer >= 30.0f && loginTimer < 60.0f) {
                ImGui::Spacing();
                ImGui::TextWrapped(OBFUSCATE("This process may take a while."));
            } else if (loginTimer >= 60.0f && err == std::string(OBFUSCATE("timeout"))) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), OBFUSCATE("Please try again."));
            } else if (!err.empty() && err != std::string(OBFUSCATE("OK"))) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), OBFUSCATE("Error: %s"), err.c_str());
            }

            /* Contact / link */
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextWrapped(OBFUSCATE("Contact"));
            ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
            ImVec4 linkColor(0.2f, 0.6f, 1.0f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, linkColor);
            const char* url   = OBFUSCATE("https://www.facebook.com/an.an.526810"); // Url Example
            const char* label = OBFUSCATE("fb");
            ImGui::Text(OBFUSCATE("%s"), label);
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsMouseClicked(0)) OpenUrl(url);
            }
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(min.x, max.y), ImVec2(max.x, max.y),
                ImGui::GetColorU32(linkColor), 1.0f);
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
            ImGui::Text(OBFUSCATE("to get free key"));

            ImGui::EndPopup();
        }

    } else {

        /////////////////////////////////////////////////////////////////////
        // Main overlay (post-login)                                       //
        /////////////////////////////////////////////////////////////////////
        initPublicIP();
        Render(ImGui::GetBackgroundDrawList());

        /////////////////////////////////////////////////////////////////////
        // Menu open/close animation driver                                //
        /////////////////////////////////////////////////////////////////////
        if (ShowMenu && !menuOpening && menuAnimationProgress == 0.0f) {
            menuOpening = true;
            menuClosing = false;
        }
        if (menuOpening) {
            menuAnimationProgress += io.DeltaTime * ANIMATION_SPEED;
            if (menuAnimationProgress >= 1.0f) {
                menuAnimationProgress = 1.0f;
                menuOpening = false;
            }
        }
        if (menuClosing) {
            menuAnimationProgress -= io.DeltaTime * ANIMATION_SPEED;
            if (menuAnimationProgress <= 0.0f) {
                menuAnimationProgress = 0.0f;
                menuClosing = false;
            }
        }

        /////////////////////////////////////////////////////////////////////
        // Mod menu window                                                 //
        /////////////////////////////////////////////////////////////////////
        if (ShowMenu || menuAnimationProgress > 0.0f) {
            ImGui::OpenPopup(OBFUSCATE("##MenuMod"));
			
            ///////////////////////////////////////////////////////////
            // Tab bar                                               //
            ///////////////////////////////////////////////////////////
            /* Tab definitions - add/remove entries here; numButtons
               is derived automatically so no manual counter needed. */
            static const ButtonData buttons[] = {
                {OBFUSCATE(ICON_FA7_WRENCH      " Option 1"), 1},
                {OBFUSCATE(ICON_FA7_FISH        " Option 2"), 2},
                {OBFUSCATE(ICON_FA7_HAMMER      " Option 3"), 3},
                {OBFUSCATE(ICON_FA7_BUG         " Option 4"), 4},
                {OBFUSCATE(ICON_FA7_LEAF        " Option 5"), 5},
                {OBFUSCATE(ICON_FA7_GEARS       " Setting"),  6},
                {OBFUSCATE(ICON_FA7_CIRCLE_USER " About"),    7},
            };			
            /* Compute clamped menu size relative to screen */
            ImVec2 displaySize       = io.DisplaySize;
            const float padding      = 20.0f;
            float desiredMenuHeight  = std::clamp(displaySize.y * 0.8f, 400.0f, displaySize.y * 0.9f);
            float maxMenuHeight      = displaySize.y * 0.9f;

            /* Tab bar width: ~157 px per tab */
            static constexpr int numButtons = (int)(sizeof(buttons) / sizeof(buttons[0]));
            ImVec2 menuSize = ImVec2(numButtons * 157.0f, desiredMenuHeight);
            ImVec2 maxMenuSize(displaySize.x * 0.9f, maxMenuHeight);

            /* Clamp menu so it doesn't overflow the screen edges */
            if (menuPosition.x + menuSize.x > displaySize.x - 10.0f)
                menuSize.x = displaySize.x - menuPosition.x - 10.0f;
            if (menuPosition.y + menuSize.y > displaySize.y - 10.0f)
                menuSize.y = displaySize.y - menuPosition.y - 10.0f;
            if (menuPosition.x < 10.0f) {
                menuSize.x   -= (10.0f - menuPosition.x);
                menuPosition.x = 10.0f;
            }
            if (menuPosition.y < 10.0f) {
                menuSize.y   -= (10.0f - menuPosition.y);
                menuPosition.y = 10.0f;
            }
            menuSize.x = std::clamp(menuSize.x, 200.0f, maxMenuSize.x);
            menuSize.y = std::clamp(menuSize.y, 150.0f, maxMenuSize.y);

            /* Smooth-step progress for the open/close animation */
            float easedProgress   = menuAnimationProgress * menuAnimationProgress
                                  * (3.0f - 2.0f * menuAnimationProgress);
            float animatedWidth   = menuSize.x * easedProgress;
            float animatedHeight  = menuSize.y * easedProgress;
            ImVec2 animatedPos(
                hideIconPos.x + (menuPosition.x - hideIconPos.x) * easedProgress,
                hideIconPos.y + (menuPosition.y - hideIconPos.y) * easedProgress);

            ImGui::SetNextWindowSize(ImVec2(animatedWidth, animatedHeight), ImGuiCond_Always);
            ImGui::SetNextWindowPos(animatedPos, ImGuiCond_Always);

            if (ImGui::BeginPopupModal(OBFUSCATE("##MenuMod"), NULL,
                    ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoScrollbar  | ImGuiWindowFlags_NoScrollWithMouse)) {

                /* Only render content once the window is partially open */
                if (menuAnimationProgress > 0.2f) {
                    ImGuiWindow* window    = ImGui::GetCurrentWindow();
                    ImDrawList*  drawList  = window->DrawList;
                    ImVec2       windowPos = window->Pos;
                    ImVec2       windowSize= window->Size;

                    ///////////////////////////////////////////////////////////
                    // Drag header + close button                            //
                    ///////////////////////////////////////////////////////////
                    const float closeBtnSize    = 24.0f;
                    const float closeBtnPadding = 20.0f;
                    ImVec2 headerMin = windowPos;
                    ImVec2 headerMax = ImVec2(windowPos.x + windowSize.x, windowPos.y + 50);
                    ImVec2 dragMin   = ImVec2(windowPos.x + closeBtnPadding + closeBtnSize + 10, windowPos.y);
                    ImVec2 dragMax   = headerMax;

                    ImGui::SetCursorScreenPos(dragMin);
                    ImGui::InvisibleButton(OBFUSCATE("##HeaderDrag"),
                        ImVec2(dragMax.x - dragMin.x, dragMax.y - dragMin.y));

                    bool mouseInHeader = ImGui::IsItemHovered();
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                        ImVec2 drag_delta = io.MouseDelta;
                        menuPosition.x   += drag_delta.x;
                        menuPosition.y   += drag_delta.y;
                        isDraggingMenu    = true;
                        hideIconPos       = ImVec2(menuPosition.x - menuOffset.x,
                                                   menuPosition.y - menuOffset.y);
                        shouldUpdateIconPosition = true;
                        newIconPosition          = hideIconPos;
                    } else {
                        isDraggingMenu = false;
                    }
                    if (mouseInHeader) {
                        drawList->AddRectFilled(headerMin, headerMax, IM_COL32(255, 255, 255, 10));
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    /* Centred title text */
                    ImVec2 textSize = ImGui::CalcTextSize(OBFUSCATE(MOD_AUTHOR));
                    ImVec2 textPos(
                        windowPos.x + (windowSize.x - textSize.x) * 0.5f,
                        windowPos.y + 24.0f);
                    drawList->AddText(textPos, IM_COL32_WHITE, OBFUSCATE(MOD_AUTHOR));

                    /* Red circular close button */
                    ImGui::SetCursorPos(ImVec2(40, 20));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(6.f, 6.f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 300.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
                    if (ImGui::Button(OBFUSCATE("##CloseMenu"), ImVec2(27, 27))) {
                        ShowMenu       = false;
                        menuClosing    = true;
                        menuOpening    = false;
                        isDraggingMenu = false;
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::PopStyleVar(2);
                    ImGui::Spacing();
                    ImGui::Separator();
    
                    float deltaTime     = io.DeltaTime;
                    float availableWidth = windowSize.x - 40.0f;
                    RenderTabBar(buttons, numButtons, easedProgress, deltaTime, availableWidth);

                    ///////////////////////////////////////////////////////////
                    // Tab content area                                      //
                    ///////////////////////////////////////////////////////////
                    float currentY        = ImGui::GetCursorPosY();
                    float tabContentHeight = std::max(windowSize.y - currentY - padding, 100.0f);
                    ImGui::BeginChild(OBFUSCATE("##TabContent"),
                        ImVec2(0, tabContentHeight), false, ImGuiWindowFlags_None);

                    if (TabMenu == 1) {
                        static bool example1 = false, example2 = false;
                        if (ImGui::BeginTable(OBFUSCATE("##split_table1"), 2, ImGuiTableFlags_BordersInnerV)) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 1"), &example1);
                            }
                            ImGui::TableSetColumnIndex(1);{
                                ImGui::Checkbox(OBFUSCATE("Example 2"), &example2);
                            }
                            ImGui::EndTable();
                        }
                    }
                    if (TabMenu == 2) {
                        static bool example3 = false, example4 = false;
                        if (ImGui::BeginTable(OBFUSCATE("##split_table2"), 2, ImGuiTableFlags_BordersInnerV)) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 3"), &example3);
                            }
                            ImGui::TableSetColumnIndex(1);
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 4"), &example4);
                            }
                            ImGui::EndTable();
                        }
                    }
                    if (TabMenu == 3) {
                        static bool example5 = false, example6 = false;
                        if (ImGui::BeginTable(OBFUSCATE("##split_table3"), 2, ImGuiTableFlags_BordersInnerV)) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); 
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 5"), &example5);
                            }
                            ImGui::TableSetColumnIndex(1);
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 6"), &example6);
                            }
                            ImGui::EndTable();
                        }
                    }
                    if (TabMenu == 4) {
                        static bool example7 = false, example8 = false;
                        if (ImGui::BeginTable(OBFUSCATE("##split_table4"), 2, ImGuiTableFlags_BordersInnerV)) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); 
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 7"), &example7);
                            }
                            ImGui::TableSetColumnIndex(1);
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 8"), &example8);
                            }
                            ImGui::EndTable();
                        }
                    }
                    if (TabMenu == 5) {
                        static bool example9 = false, example10 = false;
                        if (ImGui::BeginTable(OBFUSCATE("##split_table5"), 2, ImGuiTableFlags_BordersInnerV)) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 9"),  &example9);
                            }
                            ImGui::TableSetColumnIndex(1);
                            {
                                ImGui::Checkbox(OBFUSCATE("Example 10"), &example10);
                            }
                            ImGui::EndTable();
                        }
                    }
                    if (TabMenu == 6) {
                        SaveLoad_GUI();
                        ImGui::Checkbox(OBFUSCATE("Increase/decrease volume to open menu"), &Volume);
                        ImGui::Checkbox(OBFUSCATE("Hide Menu Icon"), &HideIcon);
                    }
                    if (TabMenu == 7) {
                        static std::string package  = "";
                        static std::string version  = "";
                        static bool        infoLoaded = false;
                        if (!infoLoaded) {
                            JNIEnv* env;
                            jvm->AttachCurrentThread(&env, nullptr);
                            jobject context = getApplicationContext();
                            package    = getPackageName(env, context);
                            version    = getVersion(env, context);
                            jvm->DetachCurrentThread();
                            infoLoaded = true;
                        }
                        ImGui::Text(OBFUSCATE("Package: %s"),      package.c_str());
                        ImGui::Text(OBFUSCATE("Version: %s"),      version.c_str());
                        ImGui::Text(OBFUSCATE("Last updated: %s"), BuildTime().c_str());
                        /*
                        std::tm tmVN = getCurrentVnTime();
						VnDateTime vn = getCurrentVnDateTime();
						
						char bufDate[64];
						snprintf(bufDate, sizeof(bufDate), OBFUSCATE("Ngày %d Tháng %d Năm %d"), vn.day, vn.month, vn.year);
						
						char bufTime[32];
						snprintf(bufTime, sizeof(bufTime), OBFUSCATE("%02d:%02d:%02d"), vn.hour, vn.minute, vn.second);
						
						ImGui::Text(OBFUSCATE("%s"), bufDate);
						ImGui::Text(OBFUSCATE("Time: %s"), bufTime);
						
						ImGui::Text(OBFUSCATE("IP: %s"), g_publicIP.c_str());
						
						if (bValid && !g_ExpireDate.empty()) {
						    TimeRemaining remaining = calculateTimeRemaining(g_ExpireDate);
						    
						    if (remaining.expired) {
						        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), OBFUSCATE("KEY HẾT HẠN!"));
						    } else {
						        ImGui::Text(OBFUSCATE("Thời hạn key còn: %d ngày %d giờ %d phút %d giây"), remaining.days, remaining.hours, remaining.minutes, remaining.seconds);
						    }

						ImVec4 linkColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
						ImGui::PushStyleColor(ImGuiCol_Text, linkColor);
						
						const char* url   = OBFUSCATE("https://www.facebook.com/an.an.526810");
						const char* label = OBFUSCATE("link fb");
						ImGui::Text("%s", ICON_FA7_FACEBOOK);
						ImGui::SameLine();
						
						ImGui::Text("%s", label);
						
						if (ImGui::IsItemHovered()) {
							ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
							if (ImGui::IsMouseClicked(0)) {
							    OpenUrl(url);
							}
						}
						
						// Underline the link
						ImVec2 min = ImGui::GetItemRectMin();
						ImVec2 max = ImGui::GetItemRectMax();
						ImGui::GetWindowDrawList()->AddLine(
							ImVec2(min.x, max.y),
							ImVec2(max.x, max.y),
							ImGui::GetColorU32(linkColor),
							1.0f
						);
                        */
                    }

                    ImGui::EndChild();
                }
                ImGui::EndPopup();
            }

        } else {
            /* Menu is fully closed - run close-animation tail and show icon */
            if (menuClosing) {
                menuAnimationProgress -= io.DeltaTime * ANIMATION_SPEED * 1.5f;
                if (menuAnimationProgress <= 0.0f) {
                    menuAnimationProgress = 0.0f;
                    menuClosing    = false;
                    isDraggingMenu = false;
                }
            }

            ///////////////////////////////////////////////////////////////////
            // Floating icon window                                          //
            ///////////////////////////////////////////////////////////////////
            static TextureInfo logoTex = createTexturePNGFromMem(logo, sizeof(logo));

            ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
            if (shouldUpdateIconPosition) {
                ImGui::SetNextWindowPos(newIconPosition, ImGuiCond_Always);
                shouldUpdateIconPosition = false;
            }
            ImGui::Begin(OBFUSCATE("##IconWindow"), nullptr,
                ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoBackground  | ImGuiWindowFlags_NoScrollbar      |
                ImGuiWindowFlags_NoCollapse);

            if (!HideIcon) {
                const ImVec2 button_size(84, 84);
                ImVec2       pos = ImGui::GetCursorScreenPos();
                hideIconPos = pos;

                ImGui::InvisibleButton(OBFUSCATE("logo_btn"), button_size);
                ImVec2 delta = io.MouseDelta;

                /* Tap (no drag) → open menu */
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0)
                    && fabsf(delta.x) < 2.0f && fabsf(delta.y) < 2.0f) {
                    ShowMenu              = true;
                    menuOpening           = true;
                    menuClosing           = false;
                    menuAnimationProgress = 0.0f;
                    menuPosition          = ImVec2(hideIconPos.x + menuOffset.x,
                                                   hideIconPos.y + menuOffset.y);
                }

                /* Drag → reposition icon */
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                    ImVec2 drag_delta = io.MouseDelta;
                    ImVec2 windowPos  = ImGui::GetWindowPos();
                    ImGui::SetWindowPos(ImVec2(windowPos.x + drag_delta.x,
                                               windowPos.y + drag_delta.y));
                    menuPosition = ImVec2(hideIconPos.x + menuOffset.x,
                                          hideIconPos.y + menuOffset.y);
                }

                /* Draw logo image */
                float   iconAlpha = ImGui::IsItemHovered() ? 1.0f : 0.8f;
                ImU32   iconColor = IM_COL32(255, 255, 255, (int)(iconAlpha * 255));
                ImVec2  icon_size(60, 60);
                ImVec2  icon_pos(
                    pos.x + (button_size.x - icon_size.x) * 0.5f,
                    pos.y + (button_size.y - icon_size.y) * 0.5f);
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)logoTex.textureId,
                    icon_pos,
                    ImVec2(icon_pos.x + icon_size.x, icon_pos.y + icon_size.y),
                    ImVec2(0, 0), ImVec2(1, 1), iconColor);
            }
            ImGui::End();
        }
    }

    /////////////////////////////////////////////////////////////////////////
    // Finalise and present frame                                          //
    /////////////////////////////////////////////////////////////////////////
    ImGui::Render();
    ImGui::EndFrame();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,  &glWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &glHeight);

    /* Clear key-down flags set by the touch handler */
    io.KeysDown[io.KeyMap[ImGuiKey_UpArrow]]    = false;
    io.KeysDown[io.KeyMap[ImGuiKey_DownArrow]]  = false;
    io.KeysDown[io.KeyMap[ImGuiKey_LeftArrow]]  = false;
    io.KeysDown[io.KeyMap[ImGuiKey_RightArrow]] = false;
    io.KeysDown[io.KeyMap[ImGuiKey_Enter]]      = false;
    io.KeysDown[io.KeyMap[ImGuiKey_Backspace]]  = false;
    io.KeysDown[io.KeyMap[ImGuiKey_Delete]]     = false;
    io.KeysDown[io.KeyMap[ImGuiKey_Escape]]     = false;
    io.KeysDown[io.KeyMap[ImGuiKey_Home]]       = false;
    io.KeysDown[io.KeyMap[ImGuiKey_End]]        = false;

    return orig_eglSwapBuffers(dpy, surface);
}


/////////////////////////////////////////////////////////////////////////////
// Init_Thread                                                             //
// Waits for libil2cpp.so to be mapped, then runs game hooks.             //
// Retries up to 10 times with 2-second delays before giving up.         //
/////////////////////////////////////////////////////////////////////////////
using KittyScanner::RegisterNativeFn;
ProcMap il2cppMap;

void* Init_Thread(void*) {
    int       retryCount = 0;
    const int maxRetries = 10;

    while (!il2cppMap.isValid() && retryCount < maxRetries) {
        il2cppMap = KittyMemory::getLibraryBaseMap(TARGET_LIB);
        if (!il2cppMap.isValid()) {
            usleep(2000 * 1000);
            retryCount++;
        }
    }
    if (!il2cppMap.isValid()) return nullptr;

    Attach();
    /* Place game hooks here, e.g.:
       HOOK(0x1234567, CW::Common::GetTouch, CW::Common::orig_GetTouch); */

    return nullptr;
}

/* This is the Input Hook part used for UI testing.
 * You can follow the latest changes at:
 * https://github.com/traitimtrongvag/Android-Imgui-Template/tree/Test-UI
 */

 // /////////////////////////////////////////////////////////////////////////////
 // // nativeOnTouch - JNI bridge for motion events from Java                 //
 // // Forwards ACTION_DOWN / ACTION_UP / ACTION_MOVE to ImGui's IO.         //
 // // Returns JNI_TRUE if ImGui wants to consume the event.                 //
 // /////////////////////////////////////////////////////////////////////////////
 // extern "C" {
 //
 // JNIEXPORT jboolean JNICALL
 // Java_com_unity3d_player_UnityPlayerActivity_nativeOnTouch(
 //   JNIEnv* env, jclass clazz, jobject motionEvent)
 // {
 //   if (!setup || motionEvent == nullptr) return JNI_FALSE;
 //
 //   jclass    motionEventClass = env->GetObjectClass(motionEvent);
 //   if (!motionEventClass) return JNI_FALSE;
 //
 //   jmethodID getX            = env->GetMethodID(motionEventClass, "getX",            "()F");
 //   jmethodID getY            = env->GetMethodID(motionEventClass, "getY",            "()F");
 //   jmethodID getAction       = env->GetMethodID(motionEventClass, "getAction",       "()I");
 //   jmethodID getPointerCount = env->GetMethodID(motionEventClass, "getPointerCount", "()I");
 //   if (!getX || !getY || !getAction) return JNI_FALSE;
 //
 //   float x      = env->CallFloatMethod(motionEvent, getX);
 //   float y      = env->CallFloatMethod(motionEvent, getY);
 //   int   action = env->CallIntMethod(motionEvent, getAction);
 //
 //   ImGuiIO& io = ImGui::GetIO();
 //   io.AddMousePosEvent(x, y);
 //
 //   if      (action == 0) io.AddMouseButtonEvent(0, true);   /* ACTION_DOWN */
 //   else if (action == 1) io.AddMouseButtonEvent(0, false);  /* ACTION_UP   */
 //   /* ACTION_MOVE (2) - position already updated above */
 //
 //   return io.WantCaptureMouse ? JNI_TRUE : JNI_FALSE;
 // }
 //
 // } /* extern "C" */


/////////////////////////////////////////////////////////////////////////////
// JNI_OnLoad                                                             //
// Installs all native hooks and launches the il2cpp init thread.         //
/////////////////////////////////////////////////////////////////////////////
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    jvm = vm;
    srand(time(nullptr));

    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;

    /* Hook ANativeWindow size queries to track surface dimensions */
    Tools::Hook(
        DobbySymbolResolver(oxorany("/system/lib64/libandroid.so"), "ANativeWindow_getWidth"),
        (void*)_ANativeWindow_getWidth, (void**)&orig_ANativeWindow_getWidth);
    Tools::Hook(
        DobbySymbolResolver(oxorany("/system/lib64/libandroid.so"), "ANativeWindow_getHeight"),
        (void*)_ANativeWindow_getHeight, (void**)&orig_ANativeWindow_getHeight);
    Tools::Hook(
        DobbySymbolResolver("/system/lib64/libEGL.so", "eglSwapBuffers"),
        (void*)_eglSwapBuffers, (void**)&orig_eglSwapBuffers);

    usleep(5000 * 1000);
    pthread_t initThread;
    if (pthread_create(&initThread, nullptr, Init_Thread, nullptr) == 0)
        pthread_detach(initThread);

    return JNI_VERSION_1_6;
}
