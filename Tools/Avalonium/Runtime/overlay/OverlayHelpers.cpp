#include "OverlayInternal.h"

ImFont* gUiFont = nullptr;
ImFont* gUiFontBold = nullptr;
bool gFontsConfigured = false;
ImGuiContext* gFontContext = nullptr;

WorldExplorerState gWorldExplorer;
RuntimeMainView gActiveMainView = RuntimeMainView_Globals;

void ConfigureUiFontsForCurrentContext()
{
    ImGuiContext* currentContext = ImGui::GetCurrentContext();
    if (!currentContext)
        return;

    gFontContext = currentContext;
    gUiFont = nullptr;
    gUiFontBold = nullptr;
    gFontsConfigured = false;
}

void EnsureUiFonts()
{
    ImGuiContext* currentContext = ImGui::GetCurrentContext();
    if (!currentContext)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (gFontContext != currentContext)
    {
        gFontContext = currentContext;
        gUiFont = nullptr;
        gUiFontBold = nullptr;
        gFontsConfigured = false;
    }

    if (gFontsConfigured)
        return;

    gUiFont = io.FontDefault;
    if (!gUiFont && io.Fonts && !io.Fonts->Fonts.empty())
        gUiFont = io.Fonts->Fonts[0];

    if (io.Fonts && io.Fonts->Fonts.size() > 1)
        gUiFontBold = io.Fonts->Fonts[1];
    else
        gUiFontBold = gUiFont;

    gFontsConfigured = (gUiFont != nullptr);
}

std::string FormatPointer(void* value)
{
    if (!value)
        return "<not found>";

    char buffer[32] = {};
    snprintf(buffer, sizeof(buffer), "0x%llX", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(value)));
    return buffer;
}

std::string FormatOffset(int32_t value)
{
    if (value < 0)
        return "<n/a>";

    char buffer[32] = {};
    snprintf(buffer, sizeof(buffer), "0x%X", static_cast<unsigned int>(value));
    return buffer;
}

std::string FormatCountValue(int value)
{
    std::string digits = std::to_string(std::max(value, 0));
    for (int insertAt = static_cast<int>(digits.size()) - 3; insertAt > 0; insertAt -= 3)
        digits.insert(static_cast<size_t>(insertAt), ",");
    return digits;
}

std::string SanitizeUiText(const std::string& value, const char* fallback)
{
    if (value.empty())
        return fallback;

    std::string sanitized;
    sanitized.reserve(value.size());

    bool lastWasSpace = false;
    for (unsigned char ch : value)
    {
        if (ch == 0)
            break;

        if (ch == '\r' || ch == '\n' || ch == '\t')
        {
            if (!lastWasSpace)
            {
                sanitized.push_back(' ');
                lastWasSpace = true;
            }
            continue;
        }

        if (ch < 32)
            continue;

        sanitized.push_back(static_cast<char>(ch));
        lastWasSpace = (ch == ' ');
    }

    while (!sanitized.empty() && sanitized.front() == ' ')
        sanitized.erase(sanitized.begin());
    while (!sanitized.empty() && sanitized.back() == ' ')
        sanitized.pop_back();

    return sanitized.empty() ? std::string(fallback) : sanitized;
}

std::string ToLowerCopy(const std::string& value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for (unsigned char ch : value)
        lowered.push_back(static_cast<char>(std::tolower(ch)));
    return lowered;
}

bool ContainsCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (!needle || needle[0] == '\0')
        return true;

    const std::string loweredNeedle = ToLowerCopy(needle);
    return ToLowerCopy(haystack).find(loweredNeedle) != std::string::npos;
}

bool BeginSectionPanel(const char* id, const ImVec2& size)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.12f, 0.98f));
    return ImGui::BeginChild(id, size, true);
}

void EndSectionPanel()
{
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void DrawMutedText(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.58f, 0.63f, 0.72f, 1.00f));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void DrawPanelHeading(const char* title, const char* subtitle)
{
    if (gUiFontBold)
        ImGui::PushFont(gUiFontBold);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.96f, 1.00f, 1.00f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    if (gUiFontBold)
        ImGui::PopFont();
    DrawMutedText(subtitle);
    ImGui::Dummy(ImVec2(0.0f, 12.0f));
}

void DrawHotkeyPill(const char* text, const ImVec4& color)
{
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.97f, 0.98f, 1.00f, 1.00f));
    ImGui::SmallButton(text);
    ImGui::PopStyleColor(4);
}

void DrawMetricBlock(const char* label, const std::string& value)
{
    ImGui::PushID(label && label[0] ? label : "metric_block");
    DrawMutedText(label ? label : "");
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.97f, 1.00f, 1.00f));
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopStyleColor();
    ImGui::PopID();
}

void DrawInspectorInfoRow(const char* label, const std::string& value)
{
    ImGui::PushID(label && label[0] ? label : "info_row");
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.73f, 0.77f, 0.85f, 1.00f));
    ImGui::TextUnformatted(label ? label : "");
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.97f, 1.00f, 1.00f));
    ImGui::TextWrapped("%s", value.c_str());
    ImGui::PopStyleColor();
    ImGui::PopID();
}

std::string GetDetectedVersionLabel()
{
    if (g_State.UEMajor <= 0)
        return "<pending>";

    return GetDetectedVersion();
}

std::string GetRendererBackendLabel()
{
    switch (globals::activeBackend)
    {
    case globals::Backend::DX11:   return "DirectX 11";
    case globals::Backend::DX12:   return "DirectX 12";
    case globals::Backend::None:
    default:                       return "<pending>";
    }
}

std::string GetProcessEventLabel()
{
    if (g_State.ProcessEvent_VTableIdx < 0)
        return "<pending>";

    char buffer[64] = {};
    snprintf(buffer, sizeof(buffer), "ProcessEvent[%d]", g_State.ProcessEvent_VTableIdx);
    return buffer;
}

bool DrawMainViewTabButton(const char* label, RuntimeMainView view, const ImVec2& size)
{
    const bool isActive = (gActiveMainView == view);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, isActive ? ImVec4(0.16f, 0.23f, 0.35f, 0.98f) : ImVec4(0.10f, 0.12f, 0.17f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isActive ? ImVec4(0.18f, 0.26f, 0.39f, 1.00f) : ImVec4(0.15f, 0.19f, 0.27f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.26f, 0.39f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, isActive ? ImVec4(0.97f, 0.98f, 1.00f, 1.00f) : ImVec4(0.77f, 0.82f, 0.90f, 1.00f));

    const bool pressed = ImGui::Button(label, size);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (pressed)
        gActiveMainView = view;

    return pressed;
}

void DrawCompactTopBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.12f, 0.98f));
    if (ImGui::BeginChild("CompactTopBar", ImVec2(0.0f, 52.0f), true, ImGuiWindowFlags_NoScrollbar))
    {
        const float rowHeight = ImGui::GetFrameHeight();
        const float centeredRowY = std::max((ImGui::GetWindowHeight() - rowHeight) * 0.5f, 0.0f);
        ImGui::SetCursorPosY(centeredRowY);

        if (ImGui::BeginTable("CompactTopBarLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
        {
            ImGui::TableSetupColumn("PrimaryTabs", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Hotkeys", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
            DrawMainViewTabButton("Globals", RuntimeMainView_Globals, ImVec2(96.0f, 0.0f));
            ImGui::SameLine();
            DrawMainViewTabButton("Explorer / Inspector", RuntimeMainView_WorldExplorer, ImVec2(180.0f, 0.0f));
            ImGui::SameLine();
            DrawMainViewTabButton("Live Editor", RuntimeMainView_LiveEditor, ImVec2(120.0f, 0.0f));
            ImGui::SameLine();
            DrawMainViewTabButton("Universal Search", RuntimeMainView_GlobalSearch, ImVec2(150.0f, 0.0f));
            ImGui::PopStyleVar();

            ImGui::TableSetColumnIndex(1);
            const float actionStart = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 206.0f;
            if (actionStart > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(actionStart);

            ImGui::AlignTextToFramePadding();
            DrawMutedText("F5");
            ImGui::SameLine();
            DrawHotkeyPill("Toggle", ImVec4(0.22f, 0.33f, 0.58f, 0.96f));
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            DrawMutedText("DEL");
            ImGui::SameLine();
            DrawHotkeyPill("Detach", ImVec4(0.43f, 0.22f, 0.25f, 0.96f));

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
