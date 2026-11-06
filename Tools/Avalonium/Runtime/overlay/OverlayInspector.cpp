#include "OverlayInternal.h"
#include "OverlayLiveEditor.h"

struct ResolvedOuterInfo {
    void* Outer = nullptr;
    bool SlotReadable = false;
    bool TargetReadable = false;
    std::string Name = "<outer unavailable>";
    std::string Pointer = "<not found>";
};

ResolvedOuterInfo ResolveObjectOuterInfo(void* object)
{
    ResolvedOuterInfo info;

    if (!object || g_State.UObject_OuterOff < 0)
    {
        info.Name = "<outer unavailable>";
        info.Pointer = "<offset unavailable>";
        return info;
    }

    void* outer = nullptr;
    if (!SafeMem_Read(static_cast<uint8_t*>(object) + g_State.UObject_OuterOff, &outer, sizeof(outer)))
    {
        info.Name = "<outer unreadable>";
        info.Pointer = "<unreadable>";
        return info;
    }

    info.SlotReadable = true;
    info.Outer = outer;

    if (!outer)
    {
        info.Name = "<none>";
        info.Pointer = "0x0";
        return info;
    }

    info.Pointer = FormatPointer(outer);
    if (!IsReadableWorldExplorerObject(outer))
    {
        info.Name = "<stale outer>";
        return info;
    }

    info.TargetReadable = true;
    info.Name = ResolveObjectName(outer);
    return info;
}

void RefreshInspectorSnapshot()
{
    gWorldExplorer.Inspector = WorldInspectorSnapshot();

    if (!g_State.bInitialized || !gWorldExplorer.SelectedObject)
        return;

    void* selectedObject = gWorldExplorer.SelectedObject;
    gWorldExplorer.Inspector.Pointer = FormatPointer(selectedObject);

    if (!IsReadableWorldExplorerObject(selectedObject))
    {
        gWorldExplorer.Inspector.Name = "<stale object>";
        gWorldExplorer.Inspector.FullName = "<stale object>";
        gWorldExplorer.Inspector.ClassName = "<unresolved>";
        gWorldExplorer.Inspector.OuterName = "<unresolved>";
        gWorldExplorer.Inspector.Inheritance = "<unresolved>";
        return;
    }

    void* selectedClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(selectedObject));
    const ResolvedOuterInfo outerInfo = ResolveObjectOuterInfo(selectedObject);

    gWorldExplorer.Inspector.SelectedClass = selectedClass;
    gWorldExplorer.Inspector.SelectedOuter = outerInfo.TargetReadable ? outerInfo.Outer : nullptr;
    gWorldExplorer.Inspector.Name = ResolveObjectName(selectedObject);
    gWorldExplorer.Inspector.FullName = ResolveObjectFullName(selectedObject);
    gWorldExplorer.Inspector.ClassName = selectedClass ? ResolveCachedClassName(selectedClass) : std::string("<unknown>");
    gWorldExplorer.Inspector.ClassPointer = FormatPointer(selectedClass);
    gWorldExplorer.Inspector.OuterName = outerInfo.Name;
    gWorldExplorer.Inspector.OuterPointer = outerInfo.Pointer;
    gWorldExplorer.Inspector.Inheritance = ResolveInheritanceChain(selectedClass);
}

void InvalidateInspectorProperties()
{
    gWorldExplorer.Properties.clear();
    gWorldExplorer.PropertiesLoaded = false;
    gWorldExplorer.PropertiesLoadPending = (gWorldExplorer.SelectedObject != nullptr);
}

void RefreshInspectorProperties()
{
    InvalidateInspectorProperties();
    RefreshInspectorSnapshot();
    gWorldExplorer.PropertiesLoadPending = false;

    if (!g_State.bInitialized || !gWorldExplorer.SelectedObject)
        return;

    gWorldExplorer.PropertiesLoaded = true;

    void* selectedClass = gWorldExplorer.Inspector.SelectedClass;

    if (!selectedClass)
        return;

    std::vector<PropertyInfoEx> properties(static_cast<size_t>(kMaxInspectorProperties));
    int propertyCount = GetClassProperties(selectedClass, properties.data(), static_cast<int>(properties.size()));
    if (propertyCount <= 0)
        return;
    if (propertyCount > static_cast<int>(properties.size()))
        propertyCount = static_cast<int>(properties.size());

    properties.resize(static_cast<size_t>(propertyCount));
    std::sort(properties.begin(), properties.end(), [](const PropertyInfoEx& lhs, const PropertyInfoEx& rhs)
    {
        if (lhs.Offset != rhs.Offset)
            return lhs.Offset < rhs.Offset;
        return std::strcmp(lhs.Name, rhs.Name) < 0;
    });

    gWorldExplorer.Properties = std::move(properties);
}

void MaybeAutoLoadInspectorProperties()
{
    if (!gWorldExplorer.PropertiesLoadPending)
        return;

    if (!g_State.bInitialized || gWorldExplorer.RefreshPending)
        return;

    if (!gWorldExplorer.SelectedObject)
    {
        gWorldExplorer.PropertiesLoadPending = false;
        return;
    }

    RefreshInspectorProperties();
}

void DrawInspectorPropertiesTable()
{
    ImGui::SetNextItemWidth(-FLT_MIN);

    ImGui::InputTextWithHint("##PropertyFilter", "Filter properties", gWorldExplorer.PropertyFilter, IM_ARRAYSIZE(gWorldExplorer.PropertyFilter));
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_PadOuterX;

    if (ImGui::BeginTable("InspectorPropertiesTable", 4, tableFlags, ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Owner", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();

        int propertyIndex = 0;
        for (const PropertyInfoEx& property : gWorldExplorer.Properties)
        {
            const std::string propertyName = property.Name[0] ? property.Name : "<unnamed>";
            const std::string typeName = property.TypeName[0] ? property.TypeName : "<unknown>";
            const std::string ownerName = property.ClassName[0] ? property.ClassName : "<unknown>";

            if (!ContainsCaseInsensitive(propertyName, gWorldExplorer.PropertyFilter) &&
                !ContainsCaseInsensitive(typeName, gWorldExplorer.PropertyFilter) &&
                !ContainsCaseInsensitive(ownerName, gWorldExplorer.PropertyFilter))
            {
                continue;
            }

            ImGui::PushID(propertyIndex++);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(FormatOffset(property.Offset).c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(propertyName.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.79f, 0.92f, 1.00f));
            ImGui::TextUnformatted(typeName.c_str());
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(ownerName.c_str());

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void DrawInspectorOverviewTab(const WorldInspectorSnapshot& inspector)
{
    const ImGuiTableFlags infoFlags =
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_PadOuterX;

    if (ImGui::BeginTable("InspectorInfoTable", 2, infoFlags))
    {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        const std::string classDisplay = inspector.ClassName + " (" + inspector.ClassPointer + ")";
        const std::string outerDisplay = (inspector.OuterName == "<none>" && inspector.OuterPointer == "0x0")
            ? inspector.OuterName
            : inspector.OuterName + " (" + inspector.OuterPointer + ")";

        DrawInspectorInfoRow("Name", inspector.Name);
        DrawInspectorInfoRow("Pointer", inspector.Pointer);
        DrawInspectorInfoRow("Class", classDisplay);
        DrawInspectorInfoRow("Outer", outerDisplay);
        DrawInspectorInfoRow("Full Name", inspector.FullName);
        DrawInspectorInfoRow("Inheritance", inspector.Inheritance);
        DrawInspectorInfoRow("Property Count", std::to_string(gWorldExplorer.Properties.size()));

        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    DrawMutedText("Overview keeps identity and lineage handy while Properties gets the full working area.");
}

void DrawWorldInspectorPanel()
{
    if (BeginSectionPanel("WorldInspectorPanel"))
    {
        DrawPanelHeading("Inspector", "Basic details stay instant; reflected properties now auto-load for the current selection");
        MaybeAutoLoadInspectorProperties();

        if (!gWorldExplorer.SelectedObject)
        {
            DrawMutedText((!gWorldExplorer.HasLoadedOnce || gWorldExplorer.RefreshPending)
                ? "Refresh the list first, then pick something to inspect."
                : "Pick an actor or object on the left to inspect it.");
        }
        else
        {
            const WorldInspectorSnapshot& inspector = gWorldExplorer.Inspector;

            if (ImGui::Button("Copy Pointer"))
                ImGui::SetClipboardText(inspector.Pointer.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Copy Full Name"))
                ImGui::SetClipboardText(inspector.FullName.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Refresh Basic Info"))
            {
                RefreshInspectorSnapshot();
                InvalidateInspectorProperties();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload Properties"))
                RefreshInspectorProperties();

            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            LiveEditor::DrawNavigationBreadcrumbs();
            ImGui::Dummy(ImVec2(0.0f, 6.0f));

            if (ImGui::BeginTable("InspectorQuickMetrics", 3, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                DrawMetricBlock("Selected", inspector.Name);

                ImGui::TableSetColumnIndex(1);
                DrawMetricBlock("Class", inspector.ClassName);

                ImGui::TableSetColumnIndex(2);
                DrawMetricBlock("Properties", gWorldExplorer.PropertiesLoaded
                    ? std::to_string(gWorldExplorer.Properties.size())
                    : std::string(gWorldExplorer.PropertiesLoadPending ? "Loading..." : "Pending"));

                ImGui::EndTable();
            }

            if (gWorldExplorer.SelectedObject != gLiveEditor.CurrentObject)
            {
                LiveEditor::OnSelectObject(gWorldExplorer.SelectedObject);
            }

            if (ImGui::BeginTabBar("InspectorWorkspaceTabs", ImGuiTabBarFlags_FittingPolicyResizeDown))
            {
                if (ImGui::BeginTabItem("Overview"))
                {
                    DrawInspectorOverviewTab(inspector);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Live Properties"))
                {
                    LiveEditor::DrawLivePropertiesTab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Functions"))
                {
                    LiveEditor::DrawLiveFunctionCallerTab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Watches"))
                {
                    LiveEditor::DrawLiveWatchesTab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Components"))
                {
                    LiveEditor::DrawLiveComponentsTab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Raw Properties"))
                {
                    if (!gWorldExplorer.PropertiesLoaded)
                    {
                        DrawMutedText(gWorldExplorer.PropertiesLoadPending
                            ? "Loading reflected properties for the current selection..."
                            : "Properties are waiting for a valid selection refresh.");
                        DrawMutedText("Use Reload Properties if you want to force a fresh class scan.");
                        ImGui::Dummy(ImVec2(0.0f, 8.0f));
                    }
                    else if (gWorldExplorer.Properties.empty())
                    {
                        DrawMutedText("No reflected class properties were discovered for this selection. Some packages, root objects, transient objects, or stale entries can legitimately show 0 here.");
                        ImGui::Dummy(ImVec2(0.0f, 8.0f));
                    }
                    else
                    {
                        const std::string propertySummary = std::to_string(gWorldExplorer.Properties.size()) + " reflected properties for the current class.";
                        DrawMutedText(propertySummary.c_str());
                        ImGui::Dummy(ImVec2(0.0f, 8.0f));
                        DrawInspectorPropertiesTable();
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
    }
    EndSectionPanel();
}
