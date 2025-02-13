#include "CkOverlapBody_DebugWindow.h"

#include "CkOverlapBody/Marker/CkMarker_Utils.h"

#include "CogImguiHelper.h"
#include "CogWindowWidgets.h"

// --------------------------------------------------------------------------------------------------------------------

template class FCk_OverlapBody_DebugWindow<FCk_Handle_Marker, UCk_Utils_Marker_UE>;
template class FCk_OverlapBody_DebugWindow<FCk_Handle_Sensor, UCk_Utils_Sensor_UE>;

//--------------------------------------------------------------------------------------------------------------------------

template <typename T_HandleType, typename T_UtilsType>
auto
    FCk_OverlapBody_DebugWindow<T_HandleType, T_UtilsType>::
    Initialize()
    -> void
{
    Super::Initialize();

    bHasMenu = true;
    bNoPadding = true;

    _Config = GetConfig<UCk_OverlapBody_DebugWindowConfig>();
}

//--------------------------------------------------------------------------------------------------------------------------

template <typename T_HandleType, typename T_UtilsType>
auto
    FCk_OverlapBody_DebugWindow<T_HandleType, T_UtilsType>::
    RenderHelp()
    -> void
{
    ImGui::Text("This window displays Overlap info on the selected actor");
}

//--------------------------------------------------------------------------------------------------------------------------

template <typename T_HandleType, typename T_UtilsType>
auto
    FCk_OverlapBody_DebugWindow<T_HandleType, T_UtilsType>::
    RenderContent()
    -> void
{
    Super::RenderContent();

    RenderMenu();

    if (const auto& SelectionEntity = Get_SelectionEntity(); ck::IsValid(SelectionEntity))
    {
        if (T_UtilsType::Has_Any(SelectionEntity))
        {
            RenderTable(SelectionEntity);
        }
        else
        {
            ImGui::Text(ck::Format_ANSI(TEXT("Selection Actor does NOT have any {}"), Get_Title()).c_str());
        }
    }
    else
    {
        ImGui::Text("Selection Actor is NOT Ecs Ready");
    }
}

template <typename T_HandleType, typename T_UtilsType>
auto
    FCk_OverlapBody_DebugWindow<T_HandleType, T_UtilsType>::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Separator();

            ImGui::Checkbox("Sort by Name", &_Config->SortByName);
            ImGui::Checkbox("Show Active", &_Config->ShowActive);
            ImGui::Checkbox("Show Inactive", &_Config->ShowInactive);

            ImGui::Separator();

            ImGui::ColorEdit4("Active Color", reinterpret_cast<float*>(&_Config->ActiveColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Inactive Color", reinterpret_cast<float*>(&_Config->InactiveColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);

            ImGui::Separator();

            if (ImGui::MenuItem("Reset"))
            {
                ResetConfig();
            }

            ImGui::EndMenu();
        }

        FCogWindowWidgets::SearchBar(_Filter);

        ImGui::EndMenuBar();
    }
}

template <typename T_HandleType, typename T_UtilsType>
auto
    FCk_OverlapBody_DebugWindow<T_HandleType, T_UtilsType>::
    RenderTable(
        const FCk_Handle& InOwner) -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_OverlapBody_DebugWindow_RenderTable)
    if (ImGui::BeginTable(ck::Format_ANSI(TEXT("{}"), Get_Title()).c_str(), 6,
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_NoBordersInBodyUntilResize |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("M");
        ImGui::TableSetupColumn("W");
        ImGui::TableSetupColumn("Replication");
        ImGui::TableSetupColumn("Shape");
        ImGui::TableSetupColumn("Transform");
        ImGui::TableHeadersRow();

        const auto AddToTable = [&](const T_HandleType& InOverlap)
        {
            const auto& Name = T_UtilsType::Get_Name(InOverlap);

            if (NOT _Filter.PassFilter(ck::Format_ANSI(TEXT("{}"), Name).c_str()))
            { return; }

            switch(T_UtilsType::Get_EnableDisable(InOverlap))
            {
                case ECk_EnableDisable::Enable: if (NOT _Config->ShowActive) { return; } break;
                case ECk_EnableDisable::Disable: if (NOT _Config->ShowInactive) { return; } break;
                default: break;
            }

            const auto& Color = FCogImguiHelper::ToImVec4(_Config->Get_Color<T_UtilsType>(InOverlap));
            ImGui::PushStyleColor(ImGuiCol_Text, Color);

            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), Name).c_str());

            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), Get_NumOverlaps(InOverlap)).c_str());

            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), Get_NumWorldOverlaps(InOverlap)).c_str());

            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), T_UtilsType::Get_ReplicationType(InOverlap)).c_str());

            const auto Get_ShapeInfo = [](const T_HandleType& Overlap)
            {
                const auto& ShapeInfo = T_UtilsType::Get_ShapeInfo(Overlap).Get_ShapeDimensions();
                switch(ShapeInfo.Get_ShapeType())
                {
                    case ECk_ShapeType::Box:
                    { return ck::Format(TEXT("Box:{}"), ShapeInfo.Get_BoxExtents().Get_Extents()); }
                    case ECk_ShapeType::Sphere:
                    { return ck::Format(TEXT("Sphere:{}"), ShapeInfo.Get_SphereRadius().Get_Radius()); }
                    case ECk_ShapeType::Capsule:
                    { return ck::Format(TEXT("Capsule:{}|{}"), ShapeInfo.Get_CapsuleSize().Get_Radius(), ShapeInfo.Get_CapsuleSize().Get_HalfHeight()); }
                    default: break;
                }

                return std::wstring{};
            };

            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), Get_ShapeInfo(InOverlap)).c_str());

            ImGui::TableNextColumn();
            if (auto ShapeComponent = T_UtilsType::Get_ShapeComponent(InOverlap); ck::IsValid(ShapeComponent))
            {
                ImGui::Text(ck::Format_ANSI(TEXT("{}\n{}\n{}\n"),
                    ShapeComponent->GetRelativeLocation(),
                    ShapeComponent->GetRelativeRotation(),
                    ShapeComponent->GetRelativeScale3D()).c_str());
            }
            else
            {
                ImGui::Text(ck::Format_ANSI(TEXT("NO VALID SHAPE COMPONENT")).c_str());
            }
        };

        auto AllOverlaps = T_UtilsType::Get_All(InOwner);

        if (_Config->SortByName)
        {
            AllOverlaps.Sort([&](const T_HandleType& A, const T_HandleType& B)
            {
                const auto& NameA = T_UtilsType::Get_Name(A);
                const auto& NameB = T_UtilsType::Get_Name(B);

                return NameA < NameB;
            });
        }

        for (const auto& Overlap : AllOverlaps)
        {
            AddToTable(Overlap);
        }

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Marker_DebugWindow::
    Get_Title()
        -> FName
{
    return TEXT("Marker");
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Sensor_DebugWindow::
    Get_Title()
    -> FName
{
    return TEXT("Sensor");
}

auto
    FCk_Sensor_DebugWindow::
    Get_NumOverlaps(
        const FCk_Handle_Sensor& InSensor)
    -> FName
{
    return *ck::Format_UE(TEXT("{}"), UCk_Utils_Sensor_UE::Get_AllMarkerOverlaps(InSensor).Get_Overlaps().Num());
}

FName
    FCk_Sensor_DebugWindow::
    Get_NumWorldOverlaps(
        const FCk_Handle_Sensor& InSensor)
{
    return *ck::Format_UE(TEXT("{}"), UCk_Utils_Sensor_UE::Get_AllNonMarkerOverlaps(InSensor).Get_Overlaps().Num());
}

// --------------------------------------------------------------------------------------------------------------------

