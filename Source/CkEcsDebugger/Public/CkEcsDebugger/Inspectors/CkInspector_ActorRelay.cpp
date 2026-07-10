#include "CkInspector_ActorRelay.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkActorRelay/CkActorRelay_Actor.h"
#include "CkActorRelay/CkActorRelay_GroupSubsystem.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ActorRelay)

// =====================================================================================================================

namespace
{
    auto Get_RelayActor(const FCk_Handle& InEntity) -> ACk_ActorRelay_UE*
    {
        if (ck::Is_NOT_Valid(InEntity))
        { return nullptr; }

        auto* OwningActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(InEntity);
        return Cast<ACk_ActorRelay_UE>(OwningActor);
    }
}

// =====================================================================================================================

auto FCkInspector_ActorRelay::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Actor Relay"));
}

auto FCkInspector_ActorRelay::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return Get_RelayActor(Entity) != nullptr;
}

auto FCkInspector_ActorRelay::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto* RelayActor = Get_RelayActor(Entity);
    if (RelayActor == nullptr)
    { return Builder.Build(Entity, FString()); }

    const auto CapturedEntity = Entity;
    const auto CapturedActor  = TWeakObjectPtr<ACk_ActorRelay_UE>(RelayActor);

    // ---- Identity ----

    Builder.AddRow(
        FText::FromString(TEXT("Class:")),
        [CapturedActor](const FCk_Handle&)
        {
            auto* A = CapturedActor.Get();
            return FText::FromString(A != nullptr ? A->GetClass()->GetName() : TEXT("--"));
        },
        CkStyle::Value_Object());

    Builder.AddRow(
        FText::FromString(TEXT("Actor:")),
        [CapturedActor](const FCk_Handle&)
        {
            auto* A = CapturedActor.Get();
            return FText::FromString(A != nullptr ? A->GetName() : TEXT("--"));
        },
        CkStyle::Value_String());

    // ---- Group subsystem details ----

    Builder.AddRow(
        FText::FromString(TEXT("Group Tag:")),
        [CapturedActor](const FCk_Handle&)
        {
            auto* A = CapturedActor.Get();
            if (A == nullptr) { return FText::FromString(TEXT("--")); }
            const auto Subsystem = A->Get_GroupSubsystem();
            if (NOT Subsystem.IsValid()) { return FText::FromString(TEXT("<unregistered>")); }
            const auto Tag = Subsystem->Get_GroupTag();
            return FText::FromString(Tag.IsValid() ? Tag.ToString() : TEXT("None"));
        },
        CkStyle::Value_Tag());

    Builder.AddRow(
        FText::FromString(TEXT("Ownership:")),
        [CapturedActor](const FCk_Handle&)
        {
            auto* A = CapturedActor.Get();
            if (A == nullptr) { return FText::FromString(TEXT("--")); }
            const auto Subsystem = A->Get_GroupSubsystem();
            if (NOT Subsystem.IsValid()) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), Subsystem->Get_OwnershipPolicy()));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("Selection:")),
        [CapturedActor](const FCk_Handle&)
        {
            auto* A = CapturedActor.Get();
            if (A == nullptr) { return FText::FromString(TEXT("--")); }
            const auto Subsystem = A->Get_GroupSubsystem();
            if (NOT Subsystem.IsValid()) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), Subsystem->Get_SelectionAlgorithm()));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("Disconnect:")),
        [CapturedActor](const FCk_Handle&)
        {
            auto* A = CapturedActor.Get();
            if (A == nullptr) { return FText::FromString(TEXT("--")); }
            const auto Subsystem = A->Get_GroupSubsystem();
            if (NOT Subsystem.IsValid()) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), Subsystem->Get_DisconnectPolicy()));
        },
        CkStyle::Value_Enum());

    // ---- Capacity ----

    Builder.AddRow(
        FText::FromString(TEXT("Channels:")),
        [CapturedActor](const FCk_Handle&)
        {
            auto* A = CapturedActor.Get();
            if (A == nullptr) { return FText::FromString(TEXT("--")); }
            const auto Subsystem = A->Get_GroupSubsystem();
            if (NOT Subsystem.IsValid()) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{} active / {} configured"),
                Subsystem->Get_ChannelCount_Active(),
                Subsystem->Get_ChannelCount()));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Max Entities/Ch:")),
        [CapturedActor](const FCk_Handle&)
        {
            auto* A = CapturedActor.Get();
            if (A == nullptr) { return FText::FromString(TEXT("--")); }
            const auto Subsystem = A->Get_GroupSubsystem();
            if (NOT Subsystem.IsValid()) { return FText::FromString(TEXT("--")); }
            const auto Max = Subsystem->Get_MaxEntitiesPerChannel();
            return FText::FromString(Max >= 0
                ? ck::Format_UE(TEXT("{}"), Max)
                : FString(TEXT("unlimited")));
        },
        CkStyle::Value_Numeric());

    // ---- Live channel occupancy ----

    Builder.AddRow(
        FText::FromString(TEXT("Entities On Channel:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity)) { return FText::FromString(TEXT("--")); }
            const auto Dependents = UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(CapturedEntity);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Dependents.Num()));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
