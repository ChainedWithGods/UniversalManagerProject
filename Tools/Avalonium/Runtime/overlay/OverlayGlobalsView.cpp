#include "OverlayInternal.h"

void DrawGlobalsTab()
{
    const int objectCount = g_State.bInitialized ? EngineDiscovery::GetObjectCount(g_State) : 0;
    const char* stateLabel = g_State.bInitialized ? "Ready" : "Scanning";

    const std::string ptrGUObjectArray = FormatPointer(GetGUObjectArray());
    const std::string ptrGNames = FormatPointer(GetFNamePool());
    const std::string ptrGWorld = FormatPointer(GetLiveWorldPointer());
    const std::string ptrGMalloc = FormatPointer(GetGMalloc());
    const std::string ptrGNatives = FormatPointer(GetGNatives());
    const std::string ptrGUObjectHashTables = FormatPointer(GetGUObjectHashTables());

    auto isResolvedPointer = [](const std::string& value) -> bool
    {
        return value != "<not found>";
    };

    int resolvedPointers = 0;
    resolvedPointers += isResolvedPointer(ptrGUObjectArray) ? 1 : 0;
    resolvedPointers += isResolvedPointer(ptrGNames) ? 1 : 0;
    resolvedPointers += isResolvedPointer(ptrGWorld) ? 1 : 0;
    resolvedPointers += isResolvedPointer(ptrGMalloc) ? 1 : 0;
    resolvedPointers += isResolvedPointer(ptrGNatives) ? 1 : 0;
    resolvedPointers += isResolvedPointer(ptrGUObjectHashTables) ? 1 : 0;

    if (ImGui::BeginTable("GlobalsMetrics", 6, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawMetricBlock("UE Version", GetDetectedVersionLabel());

        ImGui::TableSetColumnIndex(1);
        DrawMetricBlock("Renderer", GetRendererBackendLabel());

        ImGui::TableSetColumnIndex(2);
        DrawMetricBlock("VTable", GetProcessEventLabel());

        ImGui::TableSetColumnIndex(3);
        DrawMetricBlock("Object Count", std::to_string(objectCount));

        ImGui::TableSetColumnIndex(4);
        DrawMetricBlock("Discovery", stateLabel);

        ImGui::TableSetColumnIndex(5);
        DrawMetricBlock("Pointers", std::to_string(resolvedPointers) + "/6 resolved");

        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (BeginSectionPanel("GlobalsPanel"))
    {
        DrawPanelHeading("Globals", "Runtime health, pointer map, and quick copy actions");

        if (ImGui::BeginTable("GlobalsDashboardLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("StatusColumn", ImGuiTableColumnFlags_WidthStretch, 0.95f);
            ImGui::TableSetupColumn("PointersColumn", ImGuiTableColumnFlags_WidthStretch, 1.35f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            {
                DrawMutedText("Runtime status");
                ImGui::Dummy(ImVec2(0.0f, 4.0f));

                ImGui::PushStyleColor(ImGuiCol_Text, g_State.bInitialized
                    ? ImVec4(0.56f, 0.88f, 0.66f, 1.00f)
                    : ImVec4(0.93f, 0.74f, 0.40f, 1.00f));
                ImGui::TextUnformatted(g_State.bInitialized ? "DISCOVERY READY" : "DISCOVERY IN PROGRESS");
                ImGui::PopStyleColor();

                DrawMutedText((std::string("Backend: ") + GetRendererBackendLabel()).c_str());
                DrawMutedText((std::string("UE Build: ") + GetDetectedVersionLabel()).c_str());
                DrawMutedText((std::string("ProcessEvent slot: ") + GetProcessEventLabel()).c_str());

                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                if (ImGui::Button("Copy all", ImVec2(160.0f, 0.0f)))
                {
                    std::string payload;
                    payload += "GUObjectArray=" + ptrGUObjectArray + "\n";
                    payload += "GNames=" + ptrGNames + "\n";
                    payload += "GWorld=" + ptrGWorld + "\n";
                    payload += "GMalloc=" + ptrGMalloc + "\n";
                    payload += "GNatives=" + ptrGNatives + "\n";
                    payload += "GUObjectHashTables=" + ptrGUObjectHashTables;
                    ImGui::SetClipboardText(payload.c_str());
                }
                ImGui::SameLine();
                DrawMutedText("Copy all to clipboard");
            }

            ImGui::TableSetColumnIndex(1);
            {
                const ImGuiTableFlags tableFlags =
                    ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_BordersInnerH |
                    ImGuiTableFlags_PadOuterX;

                if (ImGui::BeginTable("GlobalsTable", 3, tableFlags))
                {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 68.0f);

                    auto drawRow = [&](const char* label, const std::string& value)
                    {
                        const bool resolved = isResolvedPointer(value);

                        ImGui::PushID(label && label[0] ? label : "global_row");
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.73f, 0.77f, 0.85f, 1.00f));
                        ImGui::TextUnformatted(label);
                        ImGui::PopStyleColor();

                        ImGui::TableSetColumnIndex(1);
                        ImGui::PushStyleColor(ImGuiCol_Text, resolved
                            ? ImVec4(0.96f, 0.97f, 1.00f, 1.00f)
                            : ImVec4(0.67f, 0.72f, 0.80f, 1.00f));
                        ImGui::TextUnformatted(value.c_str());
                        ImGui::PopStyleColor();

                        ImGui::TableSetColumnIndex(2);
                        if (resolved)
                        {
                            if (ImGui::SmallButton("Copy"))
                                ImGui::SetClipboardText(value.c_str());
                        }
                        else
                        {
                            DrawMutedText("--");
                        }
                        ImGui::PopID();
                    };

                    drawRow("GUObjectArray", ptrGUObjectArray);
                    drawRow("GNames", ptrGNames);
                    drawRow("GWorld", ptrGWorld);
                    drawRow("GMalloc", ptrGMalloc);
                    drawRow("GNatives", ptrGNatives);
                    drawRow("GUObjectHashTables", ptrGUObjectHashTables);

                    ImGui::EndTable();
                }
            }

            ImGui::EndTable();
        }
    }
    EndSectionPanel();
}
