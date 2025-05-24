#include "CkEcs_DebugWindowWidgets.h"

#include "CogDebug.h"
#include "CogWindowHelper.h"
#include "CogWindowWidgets.h"
#include "EngineUtils.h"
#include "imgui.h"

#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCkEcs_DebugWindowWidgets::
    EntitiesList(
        const UWorld& World,
        bool OnlyRootEntities,
        const ImGuiTextFilter* Filter,
        const FCogWindowEntityContextMenuFunction& ContextMenuFunction)
    -> bool
{
    QUICK_SCOPE_CYCLE_COUNTER(DebugWindowWidgets_EntitiesList)
    TArray<FCk_Handle> Entities;

    const auto& DebuggerSubsystem = World.GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();

    if (ck::Is_NOT_Valid(SelectedWorld))
    { return {}; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(SelectedWorld);

    const auto& AddEntityToListFunc = [&](FCk_Entity InEntity)
    {
        const auto Handle = ck::MakeHandle(InEntity, TransientEntity);

        if (Handle == TransientEntity)
        { return; }

        if (OnlyRootEntities &&
            Handle.Has<ck::FFragment_LifetimeOwner>() &&
            Handle.Get<ck::FFragment_LifetimeOwner>().Get_Entity() != TransientEntity)
        { return; }

        if (ck::IsValid(Filter, ck::IsValid_Policy_NullptrOnly{}) && Filter->IsActive())
        {
            const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(Handle);
            const auto& DebugNameANSI = StringCast<ANSICHAR>(*DebugName.ToString());
            if (NOT Filter->PassFilter(DebugNameANSI.Get()))
            { return; }
        }

        Entities.Add(Handle);
    };

    {
        QUICK_SCOPE_CYCLE_COUNTER(DebugWindowWidgets_EntitiesList_BuildList)
        // Hack searching through IsHost and IsClient since view needs at least one valid fragment as a template
        TransientEntity.View<ck::FTag_NetMode_IsHost, CK_IGNORE_PENDING_KILL>().ForEach(AddEntityToListFunc);
        TransientEntity.View<ck::FTag_NetMode_IsClient, CK_IGNORE_PENDING_KILL>().ForEach(AddEntityToListFunc);
    }

    {
        QUICK_SCOPE_CYCLE_COUNTER(DebugWindowWidgets_EntitiesList_SortList)
        ck::algo::Sort(Entities, [&](const FCk_Handle& InA, const FCk_Handle& InB)
        {
            return UCk_Utils_Handle_UE::Get_DebugName(InA).ToString() < UCk_Utils_Handle_UE::Get_DebugName(InB).ToString();
        });
    }

    const auto& OldSelectionEntity = DebuggerSubsystem->Get_SelectionEntity();
    auto NewSelectionEntity = OldSelectionEntity;

    const auto LocalPlayerPawn = [&]() -> const AActor*
    {
        const auto& LocalPlayer = World.GetFirstLocalPlayerFromController();
        if (LocalPlayer == nullptr)
        { return nullptr; }

        const auto& LocalPlayerController = LocalPlayer->PlayerController;

        if (LocalPlayerController == nullptr)
        { return nullptr; }

        const auto& ClientWorldLocalPlayerPawn = LocalPlayerController->GetPawn();

        return DebuggerSubsystem->Get_ActorOnSelectedWorld(ClientWorldLocalPlayerPawn);
    }();

    ImGuiListClipper Clipper;
    Clipper.Begin(Entities.Num());
    while (Clipper.Step())
    {
        for (int32 i = Clipper.DisplayStart; i < Clipper.DisplayEnd; i++)
        {
	        const auto& Entity = Entities[i];

            if (ck::Is_NOT_Valid(Entity))
            {
                continue;
            }

            const auto& EntityActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(Entity);

            ImGui::PushStyleColor(ImGuiCol_Text,
                EntityActor != nullptr && EntityActor == LocalPlayerPawn ?
                IM_COL32(255, 255, 0, 255) :
                IM_COL32(255, 255, 255, 255));

            const bool bIsSelected = Entity == NewSelectionEntity;

            if (ImGui::Selectable(TCHAR_TO_ANSI(*UCk_Utils_Handle_UE::Get_DebugName(Entity).ToString()), bIsSelected))
            {
                World.GetSubsystem<UCk_EcsDebugger_Subsystem_UE>()->Set_SelectionEntity(Entity, SelectedWorld);
                FCogDebug::SetSelection(&World, EntityActor);
                NewSelectionEntity = Entity;
            }

            ImGui::PopStyleColor(1);

            //------------------------
            // Context Menu
            //------------------------
            [&]()
            {
                if (ContextMenuFunction == nullptr)
	            {
                    return;
	            }

                ImGui::SetNextWindowSize(ImVec2(FCogWindowWidgets::GetFontWidth() * 30, 0));
                if (ImGui::BeginPopupContextItem())
                {
                    ContextMenuFunction(Entity);
                    ImGui::EndPopup();
                }
            }();

            //------------------------
            // Draw Frame
            //------------------------
            if (ImGui::IsItemHovered() &&
                ck::IsValid(EntityActor))
            {
                FCogWindowWidgets::ActorFrame(*EntityActor);
            }

            if (bIsSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
    }
    Clipper.End();

    return NewSelectionEntity != OldSelectionEntity;
}

auto
    FCkEcs_DebugWindowWidgets::
    EntitiesListWithFilters(
        const UWorld& World,
        bool OnlyRootEntities,
        ImGuiTextFilter* Filter,
        const FCogWindowEntityContextMenuFunction& ContextMenuFunction)
    -> bool
{
    CK_ENSURE_IF_NOT(ck::IsValid(Filter, ck::IsValid_Policy_NullptrOnly{}), TEXT("No valid filter!"))
    { return {}; }

    FCogWindowWidgets::SearchBar(*Filter);

    ImGui::Separator();

    //------------------------
    // Entities List
    //------------------------
    ImGui::BeginChild("EntitiesList", ImVec2(-1, -1), false);
    const bool SelectionChanged = EntitiesList(World, OnlyRootEntities, Filter, ContextMenuFunction);
    ImGui::EndChild();

    return SelectionChanged;
}

//--------------------------------------------------------------------------------------------------------------------------
