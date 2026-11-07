#include "stdafx.h"
#ifdef UNIVERSAL_MANAGER_BUILD
#include "overlay/LuaOverlay.h"
#endif

namespace menu {
    namespace {
        bool gRestoreOpenAfterTransition = false;
        UINT gRestoreDelayFrames = 0;
    }


    bool isOpen = false;
    float test = 0.0f;
    static bool noTitleBar = false;

    void RememberStateForTransition() {
        if (isOpen)
            gRestoreOpenAfterTransition = true;
    }

    void SetRestoreDelayFrames(UINT frames) {
        gRestoreDelayFrames = frames;
    }

    void RestoreStateAfterTransition() {
        if (!gRestoreOpenAfterTransition)
            return;

        if (isOpen) {
            gRestoreOpenAfterTransition = false;
            gRestoreDelayFrames = 0;
            return;
        }

        if (gRestoreDelayFrames > 0) {
            --gRestoreDelayFrames;
            return;
        }

        isOpen = true;
        gRestoreOpenAfterTransition = false;
        DebugLog("[menu] Restored menu visibility after overlay transition.\n");
    }



    void OnFrame() {
        bool wantsCursor = isOpen;
#ifdef UNIVERSAL_MANAGER_BUILD
        wantsCursor = wantsCursor || runtimeinput::ShouldReleaseMouseCapture() || LuaEngine::Overlay::LuaOverlayManager::Get().HasVisibleCustomWindows();
#endif

        if (ImGui::GetCurrentContext())
            ImGui::GetIO().MouseDrawCursor = wantsCursor;

        static bool sReleasedCaptureForCursor = false;
        if (wantsCursor) {
            ReleaseCapture();
            ClipCursor(nullptr);

            if (!sReleasedCaptureForCursor && globals::mainWindow && IsWindow(globals::mainWindow)) {
                PostMessage(globals::mainWindow, WM_MOUSEMOVE, 0, 0);
                PostMessage(globals::mainWindow, WM_SETCURSOR, reinterpret_cast<WPARAM>(globals::mainWindow), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
            }
            sReleasedCaptureForCursor = true;
        } else {
            sReleasedCaptureForCursor = false;
        }
    }

    void Init() {

        ImGuiIO& io = ImGui::GetIO();

        bool wantsCursor = isOpen;
#ifdef UNIVERSAL_MANAGER_BUILD
        wantsCursor = wantsCursor || runtimeinput::ShouldReleaseMouseCapture() || LuaEngine::Overlay::LuaOverlayManager::Get().HasVisibleCustomWindows();
#endif
        io.MouseDrawCursor = wantsCursor;

        if (!isOpen) {
            return;
        }

        // Style setup (one-time)
        static bool styled = false;
        if (!styled) {
            ImGui::StyleColorsDark();
            ImVec4* colors = ImGui::GetStyle().Colors;
            // Custom color palette
            colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0.8f);
            colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.3f, 0.8f);
            colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.4f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.0f);
            styled = true;
            DebugLog("[menu] Style applied.\n");
        }

        // Window flags
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(25, 25), ImGuiCond_FirstUseEver);

        ImGui::Begin("ImGui Menu", &isOpen, flags);

        if (ImGui::CollapsingHeader("MENU")) {
            if (ImGui::TreeNode("SUB MENU")) {
                ImGui::Text("Text Test");
                if (ImGui::Button("Button Test")) {
                    DebugLog("[menu] Button Test clicked.\n");
                }
                if (ImGui::Checkbox("No Title Bar", &noTitleBar)) {
                    DebugLog("[menu] Checkbox No Title Bar toggled. flags=0x%X\n", flags);
                }
                ImGui::SliderFloat("Slider Test", &test, 1.0f, 100.0f);
                ImGui::Text("Slider value=%.2f", test);
                ImGui::TreePop();
            }
        }

        ImGui::End();
    }
}
