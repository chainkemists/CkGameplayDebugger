#include "CkEntitySelection_DebugWindow.h"

#include "CogDebug.h"
#include "CogWindowWidgets.h"

#include "CkCore/String/CkString_Utils.h"

#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

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
    ImGui::Text("This window allows picking an Entity for other windows to inspect");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntitySelection_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            // ImGui::Checkbox("Only Root Entities", &Config->OnlyRootEntities);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Display"))
        {
            auto EntityListDisplayPolicyAsInt = static_cast<int32>(Config->EntitiesListDisplayPolicy);

            ImGui::RadioButton("All Entities List",      &EntityListDisplayPolicyAsInt, static_cast<int32>(ECkDebugger_EntitiesListDisplayPolicy::EntityList));
            ImGui::RadioButton("All Entities Hierarchy", &EntityListDisplayPolicyAsInt, static_cast<int32>(ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy));
            ImGui::RadioButton("Only Root Entities",     &EntityListDisplayPolicyAsInt, static_cast<int32>(ECkDebugger_EntitiesListDisplayPolicy::OnlyRootEntities));

            Config->EntitiesListDisplayPolicy = static_cast<ECkDebugger_EntitiesListDisplayPolicy>(EntityListDisplayPolicyAsInt);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Sorting"))
        {
            auto EntityListSortingPolicyAsInt = static_cast<int32>(Config->EntitiesListSortingPolicy);

            ImGui::RadioButton("Alphabetical",     &EntityListSortingPolicyAsInt, static_cast<int32>(ECkDebugger_EntitiesListSortingPolicy::Alphabetical));
            ImGui::RadioButton("By Entity Number", &EntityListSortingPolicyAsInt, static_cast<int32>(ECkDebugger_EntitiesListSortingPolicy::EnitityNumber));
            ImGui::RadioButton("By ID",            &EntityListSortingPolicyAsInt, static_cast<int32>(ECkDebugger_EntitiesListSortingPolicy::ID));

            Config->EntitiesListSortingPolicy = static_cast<ECkDebugger_EntitiesListSortingPolicy>(EntityListSortingPolicyAsInt);

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    EntitiesListWithFilters();
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
    FCk_EntitySelection_DebugWindow::EntitiesList() -> bool
{
    QUICK_SCOPE_CYCLE_COUNTER(EntitySelection_DebugWindow_EntitiesList)
    TArray<FCk_Handle> Entities;

    const auto& DebuggerSubsystem = GetWorld()->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();

    if (ck::Is_NOT_Valid(SelectedWorld))
    { return {}; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(SelectedWorld);

    const auto& BaseSortingFunction = [&](const FCk_Handle& InA, const FCk_Handle& InB)
    {
        switch (Config->EntitiesListSortingPolicy)
        {
        case ECkDebugger_EntitiesListSortingPolicy::ID:
            return InA < InB;
        case ECkDebugger_EntitiesListSortingPolicy::EnitityNumber:
            if (InA.Get_Entity().Get_EntityNumber() == InB.Get_Entity().Get_EntityNumber())
            {
                return InA.Get_Entity().Get_VersionNumber() < InB.Get_Entity().Get_VersionNumber();
            }
            return InA.Get_Entity().Get_EntityNumber() < InB.Get_Entity().Get_EntityNumber();
        case ECkDebugger_EntitiesListSortingPolicy::Alphabetical:
            return UCk_Utils_Handle_UE::Get_DebugName(InA).ToString() < UCk_Utils_Handle_UE::Get_DebugName(InB).ToString();
        default:
            return InA < InB;
        }
    };

    const auto& HeirarchicalSortingFunction = [&](const FCk_Handle& InA, const FCk_Handle& InB)
    {
        const auto& Get_ParentsArray = [](const FCk_Handle& InHandle) -> TArray<FCk_Handle>
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
            return ParentsArray;
        };

        const auto& ParentsArrayA = Get_ParentsArray(InA);
        const auto& ParentsArrayB = Get_ParentsArray(InB);

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
        QUICK_SCOPE_CYCLE_COUNTER(EntitySelection_DebugWindow_EntitiesList_SortList)

        if (Config->EntitiesListDisplayPolicy == ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy)
        {
            ck::algo::Sort(InEntities, HeirarchicalSortingFunction);
        }
        else
        {
            ck::algo::Sort(InEntities, BaseSortingFunction);
        }
    };

    const auto& AddEntityToListFunc = [&](FCk_Entity InEntity)
    {
        const auto Handle = ck::MakeHandle(InEntity, TransientEntity);

        if (Handle == TransientEntity)
        { return; }

        if (Config->EntitiesListDisplayPolicy == ECkDebugger_EntitiesListDisplayPolicy::OnlyRootEntities &&
            Handle.Has<ck::FFragment_LifetimeOwner>() &&
            Handle.Get<ck::FFragment_LifetimeOwner>().Get_Entity() != TransientEntity)
        { return; }

        if (Filter.IsActive())
        {
            const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(Handle);
            const auto& DebugNameANSI = StringCast<ANSICHAR>(*DebugName.ToString());
            if (NOT Filter.PassFilter(DebugNameANSI.Get()))
            { return; }
        }

        Entities.Add(Handle);
    };

    {
        QUICK_SCOPE_CYCLE_COUNTER(EntitySelection_DebugWindow_EntitiesList_BuildList)
        // Hack searching through IsHost and IsClient since view needs at least one valid fragment as a template
        TransientEntity.View<ck::FTag_NetMode_IsHost, CK_IGNORE_PENDING_KILL>().ForEach(AddEntityToListFunc);
        TransientEntity.View<ck::FTag_NetMode_IsClient, CK_IGNORE_PENDING_KILL>().ForEach(AddEntityToListFunc);
    }

    // If displaying as hierarchy, show all parents of entities shown, otherwise their context in the hierarchy won't make sense
    if (Config->EntitiesListDisplayPolicy == ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy)
    {
        auto EntitiesSet = TSet(Entities);
        for (const auto& Entity : Entities)
        {
            auto CurrentEntity = Entity;
            while (CurrentEntity.Has<ck::FFragment_LifetimeOwner>())
            {
                CurrentEntity = CurrentEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
                EntitiesSet.Add(CurrentEntity);
            }
        }
        EntitiesSet.Remove(TransientEntity);

        Entities = EntitiesSet.Array();
    }

    SortEntities(Entities);

    const auto& OldSelectionEntity = DebuggerSubsystem->Get_SelectionEntity();
    auto NewSelectionEntity = OldSelectionEntity;

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

                const bool bIsSelected = Entity == NewSelectionEntity;

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
                    DebuggerSubsystem->Set_SelectionEntity(Entity, SelectedWorld);
                    FCogDebug::SetSelection(GetWorld(), EntityActor);
                    NewSelectionEntity = Entity;
                }

                ImGui::PopStyleColor(1);

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


                ImGui::PopID();
            }
        }
        Clipper.End();
        ImGui::EndTable();
    }

    return NewSelectionEntity != OldSelectionEntity;
}

auto
    FCk_EntitySelection_DebugWindow::EntitiesListWithFilters() -> bool
{
    FCogWindowWidgets::SearchBar(Filter);

    ImGui::Separator();

    //------------------------
    // Entities List
    //------------------------
    ImGui::BeginChild("EntitiesList", ImVec2(-1, -1), false);
    const bool SelectionChanged = EntitiesList();
    ImGui::EndChild();

    return SelectionChanged;
}

// --------------------------------------------------------------------------------------------------------------------

