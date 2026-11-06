#include "OverlayInternal.h"
#include "OverlayLiveEditor.h"
#include "LuaOverlay.h"
#include "../core/RuntimeConfig.h"
#include "../cppmods/CppModImGui.h"

void DrawWorldExplorerInspectorTab()
{
    MaybeAutoRefreshWorldExplorer();

    if (!g_State.bInitialized)
    {
        DrawMutedText("Engine discovery is still running. Open this tab again once initialization is ready.");
        return;
    }

    RebuildWorldExplorerVisibleIndices();
    DrawWorldExplorerControls(gWorldExplorer.LastActorCount);

    const ImGuiTableFlags layoutFlags =
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("WorldExplorerLayout", 2, layoutFlags))
    {
        ImGui::TableSetupColumn("Explorer", ImGuiTableColumnFlags_WidthStretch, 0.95f);
        ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 1.25f);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawWorldExplorerListPanel();

        ImGui::TableSetColumnIndex(1);
        DrawWorldInspectorPanel();

        ImGui::EndTable();
    }
}

namespace menu {
namespace {
bool gRestoreOpenAfterTransition = false;
UINT gRestoreDelayFrames = 0;
}

bool isOpen = false;

void PrepareContext()
{
    ConfigureUiFontsForCurrentContext();
}

void RememberStateForTransition()
{
    if (isOpen)
        gRestoreOpenAfterTransition = true;
}

void SetRestoreDelayFrames(UINT frames)
{
    gRestoreDelayFrames = frames;
}

void RestoreStateAfterTransition()
{
    if (!gRestoreOpenAfterTransition)
        return;

    if (isOpen)
    {
        gRestoreOpenAfterTransition = false;
        gRestoreDelayFrames = 0;
        return;
    }

    if (gRestoreDelayFrames > 0)
    {
        --gRestoreDelayFrames;
        return;
    }

    isOpen = true;
    gRestoreOpenAfterTransition = false;
    DebugLog("[menu] Restored menu visibility after overlay transition.\n");
}

void OnFrame()
{
    bool wantsCursor = isOpen || LuaEngine::Overlay::LuaOverlayManager::Get().HasVisibleCustomWindows() ||
                       Avalonium::CppMods::ImGuiRender::AnyInputActive() ||
                       (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse);
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

void RenderOverlay()
{

    LuaEngine::Overlay::LuaOverlayManager::Get().Render();
    Avalonium::CppMods::ImGuiRender::RenderAll();
}

void Init()
{
    if (!RuntimeConfig::Get().EnableInGameGui)
    {
        return;
    }

    EnsureUiFonts();

    static ImGuiContext* styledContext = nullptr;
    ImGuiContext* currentContext = ImGui::GetCurrentContext();
    if (styledContext != currentContext)
    {
        ImGui::StyleColorsDark();

        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = 1.0f;
        io.FontAllowUserScaling = false;

        styledContext = currentContext;
    }

    ImGui::SetNextWindowSize(ImVec2(980.0f, 640.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f, 580.0f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Avalonium", &isOpen, flags))
    {
        ImGui::End();
        return;
    }

    DrawCompactTopBar();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    switch (gActiveMainView)
    {
    case RuntimeMainView_LiveEditor:
        LiveEditor::DrawLiveEditorTab();
        break;

    case RuntimeMainView_GlobalSearch:
        GlobalSearch::DrawGlobalSearchTab();
        break;

    case RuntimeMainView_WorldExplorer:
        DrawWorldExplorerInspectorTab();
        break;

    case RuntimeMainView_Globals:
    default:
        DrawGlobalsTab();
        break;
    }

    ImGui::End();
}
}
