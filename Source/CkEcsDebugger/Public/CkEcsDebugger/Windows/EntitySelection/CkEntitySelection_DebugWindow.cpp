#include "CkEntitySelection_DebugWindow.h"

#include "CogDebug.h"
#include "imgui_internal.h"

#include <Cog/Public/CogWidgets.h>

#include "CkAbility/Ability/CkAbility_Fragment.h"
#include "CkAbility/AbilityOwner/CkAbilityOwner_Fragment.h"

#include "CkAttribute/CkAttribute_Fragment.h"
#include "CkAttribute/ByteAttribute/CkByteAttribute_Fragment.h"
#include "CkAttribute/FloatAttribute/CkFloatAttribute_Fragment.h"
#include "CkAttribute/VectorAttribute/CkVectorAttribute_Fragment.h"

#include "CkCore/String/CkString_Utils.h"

#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#include "CkEntityCollection/CkEntityCollection_Fragment.h"

#include "CkTimer/CkTimer_Fragment.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntitySelection_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();

    bHasMenu = true;

    Config = GetConfig<UCk_DebugWindowConfig_EntitySelection>();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntitySelection_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window allows picking Entities for other windows to inspect");
    ImGui::Text("Hold Ctrl+Click to multi-select entities");
    ImGui::Text("Use 'Clear Selection' to deselect all entities");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntitySelection_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    auto RequiresUpdate = false;

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Display"))
        {
            auto EnumAsInt = static_cast<int32>(Config->EntitiesListDisplayPolicy);
            const auto OldEnum = EnumAsInt;

            ImGui::RadioButton("All Entities List",      &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListDisplayPolicy::EntityList));
            ImGui::RadioButton("All Entities Hierarchy", &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy));
            ImGui::RadioButton("Only Root Entities",     &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListDisplayPolicy::OnlyRootEntities));

            Config->EntitiesListDisplayPolicy = static_cast<ECkDebugger_EntitiesListDisplayPolicy>(EnumAsInt);

            RequiresUpdate |= OldEnum != EnumAsInt;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Sorting"))
        {
            auto EnumAsInt = static_cast<int32>(Config->EntitiesListSortingPolicy);
            const auto OldEnum = EnumAsInt;

            ImGui::RadioButton("Alphabetical",     &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListSortingPolicy::Alphabetical));
            ImGui::RadioButton("By Entity Number", &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListSortingPolicy::EnitityNumber));
            ImGui::RadioButton("By ID",            &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListSortingPolicy::ID));

            Config->EntitiesListSortingPolicy = static_cast<ECkDebugger_EntitiesListSortingPolicy>(EnumAsInt);

            RequiresUpdate |= OldEnum != EnumAsInt;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Filtering"))
        {
            auto EnumAsInt = static_cast<int32>(Config->EntitiesListFragmentFilteringTypes);
            const auto OldEnum = EnumAsInt;

            ImGui::CheckboxFlags("Attribute",         &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListFragmentFilteringTypes::Attribute));
            ImGui::CheckboxFlags("Ability",           &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListFragmentFilteringTypes::Ability));
            ImGui::CheckboxFlags("AbilityOwner",      &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListFragmentFilteringTypes::AbilityOwner));
            ImGui::CheckboxFlags("Replicated",        &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListFragmentFilteringTypes::Replicated));
            ImGui::CheckboxFlags("EntityScript",      &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListFragmentFilteringTypes::EntityScript));
            ImGui::CheckboxFlags("EntityCollection",  &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListFragmentFilteringTypes::EntityCollection));
            ImGui::CheckboxFlags("Timer",             &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListFragmentFilteringTypes::Timer));

            Config->EntitiesListFragmentFilteringTypes = static_cast<ECkDebugger_EntitiesListFragmentFilteringTypes>(EnumAsInt);

            RequiresUpdate |= OldEnum != EnumAsInt;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Update"))
        {
            auto EnumAsInt = static_cast<int32>(Config->EntitiesListUpdatePolicy);
            const auto OldEnum = EnumAsInt;

            ImGui::RadioButton("On Button",      &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListUpdatePolicy::OnButton));
            ImGui::RadioButton("Per Frame",      &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListUpdatePolicy::PerFrame));
            ImGui::RadioButton("Per Second",     &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListUpdatePolicy::PerSecond));
            ImGui::RadioButton("Per 10 Seconds", &EnumAsInt, static_cast<int32>(ECkDebugger_EntitiesListUpdatePolicy::PerTenSeconds));

            Config->EntitiesListUpdatePolicy = static_cast<ECkDebugger_EntitiesListUpdatePolicy>(EnumAsInt);

            RequiresUpdate |= OldEnum != EnumAsInt;

            if (Config->EntitiesListUpdatePolicy == ECkDebugger_EntitiesListUpdatePolicy::OnButton)
            {
                RequiresUpdate |= ImGui::Button("Do Update");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Selection"))
        {
            const auto& DebuggerSubsystem = GetWorld()->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
            const auto& SelectedEntities = DebuggerSubsystem->Get_SelectionEntities();

            ImGui::Text("Selected: %d entities", SelectedEntities.Num());

            if (ImGui::MenuItem("Clear Selection"))
            {
                DebuggerSubsystem->Clear_SelectionEntities();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    switch (Config->EntitiesListUpdatePolicy)
    {
        case ECkDebugger_EntitiesListUpdatePolicy::PerFrame:
        {
            RequiresUpdate = true;
            break;
        }
        case ECkDebugger_EntitiesListUpdatePolicy::OnButton: break;
        case ECkDebugger_EntitiesListUpdatePolicy::PerSecond:
        {
            RequiresUpdate |= (Get_CurrentTime() - LastUpdateTime) > FCk_Time(1);
            break;
        }
        case ECkDebugger_EntitiesListUpdatePolicy::PerTenSeconds:
        {
            RequiresUpdate |= (Get_CurrentTime() - LastUpdateTime) > FCk_Time(10);
            break;
        }
        default:
            break;
    }

    RenderEntitiesWithFilters(RequiresUpdate);
}

auto
    FCk_EntitySelection_DebugWindow::
    RenderTick(
        float InDeltaT)
    -> void
{
    FCk_Ecs_DebugWindow::RenderTick(InDeltaT);
}

auto
    FCk_EntitySelection_DebugWindow::
    Get_CurrentTime() const
    -> FCk_Time
{
    return UCk_Utils_Time_UE::Get_WorldTime
    (
        FCk_Utils_Time_GetWorldTime_Params{GetWorld()}.Set_TimeType(ECk_Time_WorldTimeType::RealTime)
    ).Get_WorldTime().Get_Time();
}

auto
    FCk_EntitySelection_DebugWindow::
    Get_EntitiesForList(
        const bool InRequiresUpdate) const
    -> TArray<FCk_Handle>
{
    QUICK_SCOPE_CYCLE_COUNTER(Get_EntitiesForList)

    if (NOT InRequiresUpdate)
    { return CachedSelectedEntities; }

    TSet<FCk_Handle> EntitiesSet;
    TMap<FCk_Handle, TArray<FCk_Handle>> EntityParentsCache;

    const auto& DebuggerSubsystem = GetWorld()->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();

    if (ck::Is_NOT_Valid(SelectedWorld))
    { return {}; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(SelectedWorld);

    const auto& BaseSortingFunction = [&](const FCk_Handle& InA, const FCk_Handle& InB)
    {
        // Intentionally disabled for perf, can be disabled if needed for testing
        // QUICK_SCOPE_CYCLE_COUNTER(Get_EntitiesForList_BaseSortingFunction)
        switch (Config->EntitiesListSortingPolicy)
        {
            case ECkDebugger_EntitiesListSortingPolicy::ID:
            {
                return InA < InB;
            }
            case ECkDebugger_EntitiesListSortingPolicy::EnitityNumber:
            {
                if (InA.Get_Entity().Get_EntityNumber() == InB.Get_Entity().Get_EntityNumber())
                {
                    return InA.Get_Entity().Get_VersionNumber() < InB.Get_Entity().Get_VersionNumber();
                }
                return InA.Get_Entity().Get_EntityNumber() < InB.Get_Entity().Get_EntityNumber();
            }
            case ECkDebugger_EntitiesListSortingPolicy::Alphabetical:
            {
                return UCk_Utils_Handle_UE::Get_DebugName(InA).ToString() < UCk_Utils_Handle_UE::Get_DebugName(InB).ToString();
            }
            default:
            {
                return InA < InB;
            }
        }
    };

    const auto& HierarchicalSortingFunction = [&](const FCk_Handle& InA, const FCk_Handle& InB)
    {
        // Intentionally disabled for perf, can be disabled if needed for testing
        // QUICK_SCOPE_CYCLE_COUNTER(Get_EntitiesForList_HierarchicalSortingFunction)

        const auto& ParentsArrayA = EntityParentsCache[InA];
        const auto& ParentsArrayB = EntityParentsCache[InB];

        for (int32 i = 0; i < ParentsArrayA.Num() && i < ParentsArrayB.Num(); i++)
        {
            if (ParentsArrayA[i] == ParentsArrayB[i])
            { continue; }

            return BaseSortingFunction(ParentsArrayA[i], ParentsArrayB[i]);
        }

        // if one is the child of another, sort so the parent is first
        return ParentsArrayA.Num() < ParentsArrayB.Num();
    };

    const auto& SortEntities = [&](TArray<FCk_Handle>& InEntities)
    {
        QUICK_SCOPE_CYCLE_COUNTER(Get_EntitiesForList_SortList)

        if (Config->EntitiesListDisplayPolicy == ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy)
        {
            ck::algo::Sort(InEntities, HierarchicalSortingFunction);
        }
        else
        {
            ck::algo::Sort(InEntities, BaseSortingFunction);
        }
    };

    const auto& Generate_EntityParentsCache = [&EntityParentsCache](const TArray<FCk_Handle>& InEntities)
    {
        QUICK_SCOPE_CYCLE_COUNTER(Generate_EntityParentsCache)
        for (const auto& Entity : InEntities)
        {
            const auto& Get_ParentsArray = [&EntityParentsCache](const FCk_Handle& InHandle) -> TArray<FCk_Handle>
            {
                TArray<FCk_Handle> ParentsArray;
                auto CurrentEntity = InHandle;
                ParentsArray.Add(CurrentEntity);
                while (CurrentEntity.Has<ck::FFragment_LifetimeOwner>())
                {
                    CurrentEntity = CurrentEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
                    ParentsArray.Add(CurrentEntity);
                }
                Algo::Reverse(ParentsArray);
                EntityParentsCache.Add(InHandle, ParentsArray);
                return ParentsArray;
            };

            EntityParentsCache.Add(Entity, Get_ParentsArray(Entity));
        }
    };

    const auto& AddEntityToListFunc = [&](FCk_Entity InEntity)
    {
        const auto Handle = ck::MakeHandle(InEntity, TransientEntity);

        if (EntitiesSet.Contains(Handle))
        { return; }

        if (Handle == TransientEntity)
        { return; }

        if (Config->EntitiesListDisplayPolicy == ECkDebugger_EntitiesListDisplayPolicy::OnlyRootEntities &&
            Handle.Has<ck::FFragment_LifetimeOwner>() &&
            Handle.Get<ck::FFragment_LifetimeOwner>().Get_Entity() != TransientEntity)
        { return; }

        EntitiesSet.Add(Handle);
    };

    {
        QUICK_SCOPE_CYCLE_COUNTER(Get_EntitiesForList_BuildList)

        if (Config->EntitiesListFragmentFilteringTypes == ECkDebugger_EntitiesListFragmentFilteringTypes::None)
        {
            // Hack: View requires at least one fragment to search for, LifetimeOwner is used since it should exist on all entities
            TransientEntity.View<ck::FFragment_LifetimeOwner, CK_IGNORE_PENDING_KILL>().ForEach(
                [&](FCk_Entity InEntity,
                const ck::FFragment_LifetimeOwner& InFragment)
                {
                    AddEntityToListFunc(InEntity);
                });
        }
        else
        {
            #define ADD_FILTERED_ENTITIES_FOR_FRAGMENT(_EnumName_, _Fragment_)\
            if (EnumHasAnyFlags(Config->EntitiesListFragmentFilteringTypes, ECkDebugger_EntitiesListFragmentFilteringTypes::_EnumName_))\
            {\
                TransientEntity.View<_Fragment_, CK_IGNORE_PENDING_KILL>().ForEach(\
                    [&](FCk_Entity InEntity, const _Fragment_& InFragment)\
                    {\
                        AddEntityToListFunc(InEntity);\
                    });\
            }

            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(Attribute, ck::TFragment_ByteAttribute<ECk_MinMaxCurrent::Current>)
            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(Attribute, ck::TFragment_FloatAttribute<ECk_MinMaxCurrent::Current>)
            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(Attribute, ck::TFragment_VectorAttribute<ECk_MinMaxCurrent::Current>)
            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(Ability, ck::FFragment_Ability_Current)
            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(AbilityOwner, ck::FFragment_AbilityOwner_Current)
            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(Replicated, TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>)
            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(EntityScript, ck::FFragment_EntityScript_Current)
            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(EntityCollection, ck::FFragment_EntityCollection_Params)
            ADD_FILTERED_ENTITIES_FOR_FRAGMENT(Timer, ck::FFragment_Timer_Current)

            #undef ADD_FILTERED_ENTITIES_FOR_FRAGMENT
        }
    }

    // If displaying as hierarchy, show all parents of entities shown, otherwise their context in the hierarchy won't make sense
    if (Config->EntitiesListDisplayPolicy == ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy)
    {
        for (auto EntitiesTempArray = EntitiesSet.Array();
             const auto& Entity : EntitiesTempArray)
        {
            auto CurrentEntity = Entity;
            while (CurrentEntity.Has<ck::FFragment_LifetimeOwner>())
            {
                CurrentEntity = CurrentEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
                EntitiesSet.Add(CurrentEntity);
            }
        }
        EntitiesSet.Remove(TransientEntity);
    }

    auto Entities = EntitiesSet.Array();
    Generate_EntityParentsCache(Entities);

    SortEntities(Entities);

    CachedSelectedEntities = Entities;
    LastUpdateTime = Get_CurrentTime();

    return Entities;
}

auto
    FCk_EntitySelection_DebugWindow::
    ApplyFilterToEntitiesAndCache(
        const TArray<FCk_Handle>& Entities) const
    -> const TArray<FCk_Handle>&
{
    QUICK_SCOPE_CYCLE_COUNTER(ApplyFilterToEntitiesAndCache)

    CachedFilteredSelectedEntities.Empty();
    if (NOT _Filter.IsActive())
    {
        CachedFilteredSelectedEntities = Entities;
        return CachedFilteredSelectedEntities;
    }

    if (Config->EntitiesListDisplayPolicy == ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy)
    {
        TSet<FCk_Handle> EntitiesSet;
        for (const auto& Entity : Entities)
        {
            const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
            const auto& DebugNameANSI = StringCast<ANSICHAR>(*DebugName.ToString());
            if (NOT _Filter.PassFilter(DebugNameANSI.Get()))
            { continue; }

            auto CurrentEntity = Entity;
            EntitiesSet.Add(CurrentEntity);
            while (CurrentEntity.Has<ck::FFragment_LifetimeOwner>())
            {
                CurrentEntity = CurrentEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
                EntitiesSet.Add(CurrentEntity);
            }
        }

        CachedFilteredSelectedEntities = ck::algo::Filter(Entities, [&](const FCk_Handle Entity)
        {
            return EntitiesSet.Contains(Entity);
        });
    }
    else
    {
        CachedFilteredSelectedEntities = ck::algo::Filter(Entities, [&](const FCk_Handle Entity)
        {
            const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
            const auto& DebugNameANSI = StringCast<ANSICHAR>(*DebugName.ToString());
            return _Filter.PassFilter(DebugNameANSI.Get());
        });
    }
    return CachedFilteredSelectedEntities;
}

auto
    FCk_EntitySelection_DebugWindow::
    RenderEntitiesList(
        const TArray<FCk_Handle>& Entities)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(RenderEntitiesList)

    const auto& DebuggerSubsystem = GetWorld()->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
    const auto& SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
    const auto& TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(SelectedWorld);

    const auto& SelectedEntities = DebuggerSubsystem->Get_SelectionEntities();

    const auto LocalPlayerPawn = [&]() -> const AActor*
    {
        const auto& LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
        if (LocalPlayer == nullptr)
        { return nullptr; }

        const auto& LocalPlayerController = LocalPlayer->PlayerController;

        if (LocalPlayerController == nullptr)
        { return nullptr; }

        const auto& ClientWorldLocalPlayerPawn = LocalPlayerController->GetPawn();

        return DebuggerSubsystem->Get_ActorOnSelectedWorld(ClientWorldLocalPlayerPawn);
    }();

    if (ImGui::BeginTable("Entities", 2, ImGuiTableFlags_SizingFixedFit
                                       | ImGuiTableFlags_Resizable
                                       | ImGuiTableFlags_NoBordersInBodyUntilResize
                                       | ImGuiTableFlags_ScrollY
                                       | ImGuiTableFlags_RowBg
                                       | ImGuiTableFlags_BordersV
                                       | ImGuiTableFlags_Reorderable
                                       | ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name");
        ImGui::TableHeadersRow();

        ImGuiListClipper Clipper;
        Clipper.Begin(Entities.Num());
        while (Clipper.Step())
        {
            for (int32 i = Clipper.DisplayStart; i < Clipper.DisplayEnd; i++)
            {
                QUICK_SCOPE_CYCLE_COUNTER(RenderEntitiesList_TableRow)

                const auto& Entity = Entities[i];

                if (ck::Is_NOT_Valid(Entity))
                { continue; }

                ImGui::TableNextRow();
                ImGui::PushID(i);

                const auto& EntityActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(Entity);

                ImGui::PushStyleColor(ImGuiCol_Text,
                    EntityActor != nullptr && EntityActor == LocalPlayerPawn ?
                    IM_COL32(255, 255, 0, 255) :
                    IM_COL32(255, 255, 255, 255));

                const bool bIsSelected = SelectedEntities.Contains(Entity);

                //------------------------
                // ID
                //------------------------
                ImGui::TableNextColumn();
                ImGui::Text(ck::Format_ANSI(TEXT("{}"), Entity.Get_Entity()).c_str());

                //------------------------
                // Name
                //------------------------

                // Number of lifetime owning parents, not counting Transient Entity
                const auto& Get_EntityDepth = [&TransientEntity](const FCk_Handle& InHandle) -> int32
                {
                    auto Depth = 0;
                    auto CurrentEntity = InHandle;
                    while (CurrentEntity.Has<ck::FFragment_LifetimeOwner>())
                    {
                        CurrentEntity = CurrentEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
                        if (CurrentEntity == TransientEntity)
                        { break; }

                        Depth++;
                    }
                    return Depth;
                };

                const auto& Depth = Config->EntitiesListDisplayPolicy != ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy ? 0 : Get_EntityDepth(Entity);

                const std::string EntityDisplayName = ck::Format_ANSI(TEXT("{}{}"),
                    UCk_Utils_String_UE::Get_SymbolNTimes(TEXT("  "), Depth), UCk_Utils_Handle_UE::Get_DebugName(Entity)).c_str();

                ImGui::TableNextColumn();
                if (ImGui::Selectable(EntityDisplayName.c_str(), bIsSelected))
                {
                    const auto& IsControlDown = ImGui::GetCurrentContext()->IO.KeyCtrl;

                    if (IsControlDown)
                    {
                        // Multi-select mode: toggle entity in selection
                        DebuggerSubsystem->Toggle_SelectionEntity(Entity, SelectedWorld);
                    }
                    else
                    {
                        // Single select mode: replace selection with this entity
                        DebuggerSubsystem->Set_SelectionEntities({Entity}, SelectedWorld);
                    }
                    FCogDebug::SetSelection(EntityActor);
                }

                ImGui::PopStyleColor(1);

                //------------------------
                // Draw Frame
                //------------------------
                if (ImGui::IsItemHovered() &&
                    ck::IsValid(EntityActor))
                {
                    FCogWidgets::ActorFrame(*EntityActor);
                }

                if (bIsSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }


                ImGui::PopID();
            }
        }
        Clipper.End();
        ImGui::EndTable();
    }
}

auto
    FCk_EntitySelection_DebugWindow::
    RenderEntitiesWithFilters(bool InRequiresUpdate)
    -> void
{
    const auto& SearchChanged = FCogWidgets::SearchBar("##Filter", _Filter);

    auto Entities = Get_EntitiesForList(InRequiresUpdate);

    if (SearchChanged || InRequiresUpdate)
    {
        Entities = ApplyFilterToEntitiesAndCache(Entities);
    }
    else
    {
        Entities = CachedFilteredSelectedEntities;
    }

    ImGui::Separator();

    if (Config->EntitiesListDisplayPolicy == ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy)
    {
        //------------------------
        // Entities Tree
        //------------------------
        ImGui::BeginChild("EntitiesTree", ImVec2(-1, -1), false);
        RenderEntityTree(Entities);
        ImGui::EndChild();
    }
    else
    {
        //------------------------
        // Entities List
        //------------------------
        ImGui::BeginChild("EntitiesList", ImVec2(-1, -1), false);
        RenderEntitiesList(Entities);
        ImGui::EndChild();
    }
}

auto
    FCk_EntitySelection_DebugWindow::
    RenderEntityTree(
        const TArray<FCk_Handle>& Entities)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(RenderEntityTree)

    const auto& DebuggerSubsystem = GetWorld()->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
    const auto& SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
    const auto& TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(SelectedWorld);

    const auto& SelectedEntities = DebuggerSubsystem->Get_SelectionEntities();

    int32 CurrentIndex = 0;
    while (CurrentIndex < Entities.Num())
    {
        CurrentIndex = RenderEntityNode(Entities, CurrentIndex, SelectedEntities, TransientEntity, false);
    }
}

auto
    FCk_EntitySelection_DebugWindow::
    RenderEntityNode(
        const TArray<FCk_Handle>& Entities,
        int32 CurrentIndex,
        const TArray<FCk_Handle>& SelectedEntities,
        const FCk_Handle& TransientEntity,
        bool OpenAllChildren)
    -> int32
{
    QUICK_SCOPE_CYCLE_COUNTER(RenderEntityNode)

    const FCk_Handle& Entity = Entities[CurrentIndex];

    if (ck::Is_NOT_Valid(Entity))
    { return CurrentIndex + 1; }

    const auto& DebuggerSubsystem = GetWorld()->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();

    // Get entity debug name for filtering
    const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);

    const bool HasChildren = CurrentIndex + 1 < Entities.Num() &&
        ck::IsValid(Entities[CurrentIndex + 1]) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(Entities[CurrentIndex + 1]) &&
        UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Entities[CurrentIndex + 1]) == Entity;

    ImGui::PushID(static_cast<int32>(Entity.Get_Entity().Get_ID()));

    if (OpenAllChildren)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    else
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    // Determine tree node flags
    ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_SpanFullWidth;
    if (!HasChildren)
    {
        NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (SelectedEntities.Contains(Entity))
    {
        NodeFlags |= ImGuiTreeNodeFlags_Selected;
    }

    // Get the actor for this entity for highlighting local player
    const auto& EntityActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(Entity);
    const auto LocalPlayerPawn = [&]() -> const AActor*
    {
        const auto& LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
        if (LocalPlayer == nullptr)
        { return nullptr; }

        const auto& LocalPlayerController = LocalPlayer->PlayerController;
        if (LocalPlayerController == nullptr)
        { return nullptr; }

        const auto& ClientWorldLocalPlayerPawn = LocalPlayerController->GetPawn();
        return DebuggerSubsystem->Get_ActorOnSelectedWorld(ClientWorldLocalPlayerPawn);
    }();

    // Set color for local player pawn
    const bool IsLocalPlayer = EntityActor != nullptr && EntityActor == LocalPlayerPawn;
    if (IsLocalPlayer)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
    }

    // Create the tree node
    const auto NodeLabel = ck::Format_ANSI(TEXT("{} [{}]"), DebugName, Entity.Get_Entity());
    bool OpenChildren = false;

    if (HasChildren)
    {
        OpenChildren = ImGui::TreeNodeEx(NodeLabel.c_str(), NodeFlags);
    }
    else
    {
        ImGui::TreeNodeEx(NodeLabel.c_str(), NodeFlags);
    }

    // Handle selection
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        const auto& IsControlDown = ImGui::GetCurrentContext()->IO.KeyCtrl;

        if (IsControlDown)
        {
            // Multi-select mode: toggle entity in selection
            DebuggerSubsystem->Toggle_SelectionEntity(Entity, SelectedWorld);
        }
        else
        {
            // Single select mode: replace selection with this entity
            DebuggerSubsystem->Set_SelectionEntities({Entity}, SelectedWorld);
        }

        FCogDebug::SetSelection(EntityActor);
    }

    // Handle Ctrl+Click to expand all children
    if (const auto& IsControlDown = ImGui::GetCurrentContext()->IO.KeyCtrl;
        ImGui::IsItemClicked(ImGuiMouseButton_Left) && IsControlDown)
    {
        OpenAllChildren = true;
    }

    // Context menu
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Focus on Entity"))
        {
            DebuggerSubsystem->Set_SelectionEntities({Entity}, SelectedWorld);
            FCogDebug::SetSelection(EntityActor);
        }

        if (ImGui::MenuItem("Add to Selection"))
        {
            DebuggerSubsystem->Add_SelectionEntity(Entity, SelectedWorld);
        }

        if (ImGui::MenuItem("Remove from Selection"))
        {
            DebuggerSubsystem->Remove_SelectionEntity(Entity);
        }

        ImGui::EndPopup();
    }

    // Tooltip with entity information
    if (ImGui::IsItemHovered())
    {
        if (ck::IsValid(EntityActor))
        {
            FCogWidgets::ActorFrame(*EntityActor);
        }

        ImGui::BeginTooltip();
        ImGui::Text("Entity: %s", ck::Format_ANSI(TEXT("{}"), Entity).c_str());
        if (EntityActor)
        {
            ImGui::Text("Actor: %s", ck::Format_ANSI(TEXT("{}"), EntityActor).c_str());
        }
        ImGui::Text("Hold Ctrl+Click for multi-select");
        ImGui::EndTooltip();
    }

    if (IsLocalPlayer)
    {
        ImGui::PopStyleColor();
    }

    CurrentIndex++;
    if (OpenChildren)
    {
        while (CurrentIndex < Entities.Num() &&
            ck::IsValid(Entities[CurrentIndex]) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(Entities[CurrentIndex]) &&
            UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Entities[CurrentIndex]) == Entity)
        {
            CurrentIndex = RenderEntityNode(Entities, CurrentIndex, SelectedEntities, TransientEntity, OpenAllChildren);
        }
    }
    else
    {
        const auto& HasEntityInParents = [&](const FCk_Handle& InEntity)
        {
            auto CurrentEntity = InEntity;
            while (ck::IsValid(CurrentEntity))
            {
                if (Entity == CurrentEntity)
                {
                    return true;
                }
                if (UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(CurrentEntity))
                { return false; }
                CurrentEntity = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(CurrentEntity);
            }
            return false;
        };

        while (CurrentIndex < Entities.Num() &&
            HasEntityInParents(Entities[CurrentIndex]))
        {
            CurrentIndex++;
        }
    }

    if (OpenChildren)
    {
        ImGui::TreePop();
    }

    ImGui::PopID();

    return CurrentIndex;
}

// --------------------------------------------------------------------------------------------------------------------

