#include "CkEntityCollection_DebugWindow.h"

#include "CkEntityCollection/CkEntityCollection_Utils.h"

#include <CogWindowWidgets.h>

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityCollection_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();

    bHasMenu = true;
    bNoPadding = true;

    _Config = GetConfig<UCk_EntityCollection_DebugWindowConfig>();
}

auto
    FCk_EntityCollection_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityCollection_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window displays the content of every Entity Collection of the selected actor");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityCollection_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    RenderMenu();

    auto SelectionEntity = Get_SelectionEntity();

    if (ck::Is_NOT_Valid(SelectionEntity))
    {
        ImGui::Text("Selection Actor is NOT Ecs Ready");
        return;
    }

    if (NOT UCk_Utils_EntityCollection_UE::Has_Any(SelectionEntity))
    {
        ImGui::Text(ck::Format_ANSI(TEXT("Selection Actor has no Entity Collections")).c_str());
        return;
    }

    RenderTable(SelectionEntity);
}

auto
    FCk_EntityCollection_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Checkbox("Display Content Full Name", &_Config->DisplayContentFullName);
            ImGui::Checkbox("Display Content On Single Line", &_Config->DisplayContentOnSingleLine);

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

auto
    FCk_EntityCollection_DebugWindow::
    RenderTable(
        FCk_Handle& InSelectionEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_EntityCollection_DebugWindow_RenderTable)
    if (ImGui::BeginTable("Entity Collections", 3, ImGuiTableFlags_SizingFixedFit
                                                 | ImGuiTableFlags_Resizable
                                                 | ImGuiTableFlags_NoBordersInBodyUntilResize
                                                 | ImGuiTableFlags_ScrollY
                                                 | ImGuiTableFlags_RowBg
                                                 | ImGuiTableFlags_BordersV
                                                 | ImGuiTableFlags_Reorderable
                                                 | ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Count");
        ImGui::TableSetupColumn("Entity Collection");
        ImGui::TableSetupColumn("Content");
        ImGui::TableHeadersRow();

        static int32 Selected = -1;
        int32 Index = 0;

        const auto& ShouldRenderRow = [&](const FCk_Handle_EntityCollection& InEntityCollectionEntity)
        {
            if (NOT _Config->RenderCollectionsWithNoContent && UCk_Utils_EntityCollection_UE::Get_EntitiesInCollection(InEntityCollectionEntity).Get_EntitiesInCollection().IsEmpty())
            { return false; }

            const auto& CollectionLabel = UCk_Utils_GameplayLabel_UE::Get_Label(InEntityCollectionEntity);
            const auto& CollectionName = StringCast<ANSICHAR>(*CollectionLabel.ToString()).Get();

            return _Filter.PassFilter(CollectionName);
        };

        const auto& RenderRow = [&](const FCk_Handle_EntityCollection& InEntityCollectionEntity)
        {
            ImGui::TableNextRow();

            ImGui::PushID(Index);

            const auto& EntitiesInCollection = UCk_Utils_EntityCollection_UE::Get_EntitiesInCollection(InEntityCollectionEntity);
            const auto& ContentEntities = EntitiesInCollection.Get_EntitiesInCollection();

            //------------------------
            // Count
            //------------------------
            ImGui::TableNextColumn();

            ImGui::Text(ck::Format_ANSI(TEXT("{}"), ContentEntities.Num()).c_str());

            //------------------------
            // Name
            //------------------------
            ImGui::TableNextColumn();

            const auto& CollectionLabel = UCk_Utils_GameplayLabel_UE::Get_Label(InEntityCollectionEntity);
            const auto& CollectionName = StringCast<ANSICHAR>(*CollectionLabel.ToString()).Get();

            if (ImGui::Selectable(CollectionName, Selected == Index, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick))
            {
                Selected = Index;
            }

            //------------------------
            // Content
            //------------------------
            ImGui::TableNextColumn();

            const auto& FormatContentEntityName = [&](const FCk_Handle& InContentEntity)
            {
                return _Config->DisplayContentFullName ? ck::Format_UE(TEXT("{}"), InContentEntity) : ck::Format_UE(TEXT("{}"), InContentEntity.Get_Entity());
            };

            if (ContentEntities.IsEmpty())
            {
                ImGui::Text(ck::Format_ANSI(TEXT("-")).c_str());
            }
            else
            {
                if (_Config->DisplayContentOnSingleLine)
                {
                    const auto& StringifiedContent = Algo::Accumulate(ContentEntities, FString{}, [&](const FString& AccumulatedString, const FCk_Handle& InContentEntity)
                    {
                        if (AccumulatedString.IsEmpty())
                        { return ck::Format_UE(TEXT("{}"), FormatContentEntityName(InContentEntity)); }
                        return ck::Format_UE(TEXT("{}, {}"), AccumulatedString, FormatContentEntityName(InContentEntity));
                    });

                    ImGui::Text(ck::Format_ANSI(TEXT("{}"), StringifiedContent).c_str());
                }
                else
                {
                    for (const auto& ContentEntity : ContentEntities)
                    {
                        ImGui::Text(ck::Format_ANSI(TEXT("{}"), FormatContentEntityName(ContentEntity)).c_str());
                    }
                }
            }

            ImGui::PopID();
            ++Index;
        };

        UCk_Utils_EntityCollection_UE::ForEach_EntityCollection_If(InSelectionEntity, RenderRow, ShouldRenderRow);

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------

