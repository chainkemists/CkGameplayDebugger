#include "CkInspector_Aggro.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAggro/CkAggro_Utils.h"
#include "CkAggro/CkAggroTarget_Fragment.h"
#include "CkAggro/CkAggroTarget_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Aggro)

// =====================================================================================================================

namespace ck_inspector_aggro
{
    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity) || InEntity.Has_Any<
            ck::FTag_DestroyEntity_Initiate,
            ck::FTag_DestroyEntity_EndPlay,
            ck::FTag_DestroyEntity_Teardown,
            ck::FTag_DestroyEntity_Await,
            ck::FTag_DestroyEntity_Finalize>();
    }

    auto TryGetOwner(const FCk_Handle& InEntity, FCk_Handle_Aggro& OutOwner) -> bool
    {
        OutOwner = {};
        if (IsDestroying(InEntity) || NOT UCk_Utils_Aggro_UE::Has(InEntity))
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_Aggro_Current>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_Aggro_TargetMap>())
        { return false; }

        auto Mutable = InEntity;
        OutOwner = UCk_Utils_Aggro_UE::Cast(Mutable);
        return ck::IsValid(OutOwner);
    }

    auto TryGetTarget(const FCk_Handle& InEntity, FCk_Handle_AggroTarget& OutTarget) -> bool
    {
        OutTarget = {};
        if (IsDestroying(InEntity) || NOT UCk_Utils_AggroTarget_UE::Has(InEntity))
        { return false; }
        if (NOT ck::UAggroTarget_TrackedEntity_Utils::Has(InEntity))
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_TargetInfo>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_ThreatParams>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_SpatialParams>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_ForgetParams>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_ScoreParams>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_LifetimeParams>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_Threat>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_Perception>())
        { return false; }
        if (NOT InEntity.Has<ck::FFragment_AggroTarget_Score>())
        { return false; }

        auto Mutable = InEntity;
        OutTarget = UCk_Utils_AggroTarget_UE::Cast(Mutable);
        return ck::IsValid(OutTarget);
    }

    auto GateReason(const FCk_Handle& InEntity) -> FString
    {
        return ck::DebugRequestGate::Evaluate(
            InEntity,
            ECk_DebugRequest_Requirement::AuthorityOnly).Reason.ToString();
    }

    auto Get_TargetThreat(const FCk_Handle& InTarget) -> float
    {
        return (ck::IsValid(InTarget) && InTarget.Has<ck::FFragment_AggroTarget_Threat>())
            ? InTarget.Get<ck::FFragment_AggroTarget_Threat>().Get_Threat() : 0.0f;
    }

    auto Get_TargetScore(const FCk_Handle& InTarget) -> float
    {
        return (ck::IsValid(InTarget) && InTarget.Has<ck::FFragment_AggroTarget_Score>())
            ? InTarget.Get<ck::FFragment_AggroTarget_Score>().Get_Score() : 0.0f;
    }

    // The meter fills RELATIVE to the strongest target of the same owner (so the top threat reads full) — the raw
    // threat clamp ceiling (default 10000) would make an absolute bar read empty. Standalone targets fall back to
    // their own clamp ceiling.
    auto Get_PeerMax(
        const FCk_Handle&                          InTarget,
        float (*InValueGetter)(const FCk_Handle&))
        -> float
    {
        if (ck::Is_NOT_Valid(InTarget) || NOT InTarget.Has<ck::FFragment_AggroTarget_TargetInfo>())
        { return 0.0f; }

        auto Owner = InTarget.Get<ck::FFragment_AggroTarget_TargetInfo>().Get_AggroOwner();

        if (ck::IsValid(Owner) && Owner.Has<ck::FFragment_Aggro_TargetMap>())
        {
            auto Max = 0.0f;
            for (const auto& Pair : Owner.Get<ck::FFragment_Aggro_TargetMap>().Get_TargetsByTrackedEntity())
            {
                if (ck::IsValid(Pair.Value))
                { Max = FMath::Max(Max, InValueGetter(Pair.Value)); }
            }
            return Max;
        }

        return InTarget.Has<ck::FFragment_AggroTarget_ThreatParams>()
            ? static_cast<float>(InTarget.Get<ck::FFragment_AggroTarget_ThreatParams>().Get_ThreatClampRange().Get_Max())
            : 0.0f;
    }

    auto Get_PeerFraction(
        const FCk_Handle&                          InTarget,
        float (*InValueGetter)(const FCk_Handle&))
        -> float
    {
        if (ck::Is_NOT_Valid(InTarget))
        { return 0.0f; }

        const auto Max = Get_PeerMax(InTarget, InValueGetter);

        return Max > KINDA_SMALL_NUMBER ? FMath::Clamp(InValueGetter(InTarget) / Max, 0.0f, 1.0f) : 0.0f;
    }

    auto Format_Value(
        const FCk_Handle&                          InTarget,
        float (*InValueGetter)(const FCk_Handle&))
        -> FText
    {
        if (ck::Is_NOT_Valid(InTarget))
        { return FText::FromString(TEXT("--")); }

        return FText::FromString(ck::Format_UE(TEXT("{:.1f}"), InValueGetter(InTarget)));
    }
}

// =====================================================================================================================

auto FCkInspector_Aggro::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Aggro"));
}

auto FCkInspector_Aggro::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Aggro_Current,
        ck::FFragment_AggroTarget_Score>();
}

// =====================================================================================================================

auto FCkInspector_Aggro::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    if (ck_inspector_aggro::IsDestroying(Entity))
    { return Builder.Build(Entity); }

    // ---- Aggro owner (this entity holds the threat table) ----
    auto NativeOwner = FCk_Handle_Aggro{};
    if (ck_inspector_aggro::TryGetOwner(Entity, NativeOwner))
    {
        Builder.AddHeader(FText::FromString(TEXT("Aggro Owner")));

        const auto CapturedEntity = Entity;

        // The top target is an entity, so it renders as the same clickable pill every other entity surface uses —
        // the pill reads "None" on its own when there is no active target.
        Builder.AddWidgetRow(
            FText::FromString(TEXT("Active Target:")),
            SNew(SCkDebug_EntityRef)
                .Entity_Lambda([CapturedEntity]() -> FCk_Handle
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Aggro_Current>())
                    { return {}; }

                    const auto Active = CapturedEntity.Get<ck::FFragment_Aggro_Current>().Get_ActiveTarget();

                    if (ck::Is_NOT_Valid(Active))
                    { return {}; }

                    auto Target = FCk_Handle_AggroTarget{};
                    return ck_inspector_aggro::TryGetTarget(Active, Target)
                        ? ck::UAggroTarget_TrackedEntity_Utils::Get_StoredEntity(Target)
                        : FCk_Handle{};
                })
                .ShowName(true));

        Builder.AddRow(
            FText::FromString(TEXT("Tracked Targets:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Aggro_TargetMap>())
                { return FText::FromString(TEXT("--")); }
                const auto Num = CapturedEntity.Get<ck::FFragment_Aggro_TargetMap>().Get_TargetsByTrackedEntity().Num();
                return FText::FromString(FString::Printf(TEXT("%d"), Num));
            },
            CkStyle::Value_Numeric());

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Enabled:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]() -> FText
            {
                if (ck::Is_NOT_Valid(CapturedEntity))
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(CapturedEntity.Has<ck::FTag_Aggro_Disabled>() ? TEXT("Disabled") : TEXT("Enabled"));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]() -> ECk_Tone
            {
                if (ck::Is_NOT_Valid(CapturedEntity))
                { return ECk_Tone::Neutral; }
                return CapturedEntity.Has<ck::FTag_Aggro_Disabled>() ? ECk_Tone::Err : ECk_Tone::Ok;
            }));

        Builder.AddRow(
            FText::FromString(TEXT("Eval Count:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Aggro_EvaluationClock>())
                { return FText::FromString(TEXT("--")); }
                const auto Count = CapturedEntity.Get<ck::FFragment_Aggro_EvaluationClock>().Get_DebugEvaluationCount();
                return FText::FromString(FString::Printf(TEXT("%lld"), Count));
            },
            CkStyle::Value_Numeric());

        // ---- Owner-side verbs ----
        // Every Aggro request processor is authority-gated (ensures + drops off-authority), so all of
        // these are AuthorityOnly. The pill above stays as the live read-back; this row is the write.
        const auto CapturedAggro = NativeOwner;

        if (ck::IsValid(CapturedAggro))
        {
            Builder.AddToggleRow(
                FText::FromString(TEXT("Enable/Disable:")),
                TAttribute<bool>::CreateLambda([CapturedAggro]()
                {
                    if (ck::Is_NOT_Valid(CapturedAggro)) { return false; }
                    return UCk_Utils_Aggro_UE::Get_IsEnabled(CapturedAggro);
                }),
                [CapturedAggro](bool InIsEnabled)
                {
                    auto MutableAggro = CapturedAggro;
                    if (ck::Is_NOT_Valid(MutableAggro)) { return; }

                    UCk_Utils_Aggro_UE::Request_EnableDisable(MutableAggro,
                        InIsEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable, {});
                },
                ECk_DebugRequest_Requirement::AuthorityOnly);

            Builder.AddActionRow(
                FText::FromString(TEXT("Targets:")),
                {
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Clear All")),
                        FText::FromString(TEXT("UCk_Utils_Aggro_UE::Request_ClearAllTargets")),
                        [CapturedAggro]()
                        {
                            auto MutableAggro = CapturedAggro;
                            if (ck::Is_NOT_Valid(MutableAggro)) { return; }
                            UCk_Utils_Aggro_UE::Request_ClearAllTargets(MutableAggro, {});
                        },
                        ECk_DebugRequest_Requirement::AuthorityOnly
                    },
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Clear Active")),
                        FText::FromString(TEXT("UCk_Utils_Aggro_UE::Request_ClearActiveTarget")),
                        [CapturedAggro]()
                        {
                            auto MutableAggro = CapturedAggro;
                            if (ck::Is_NOT_Valid(MutableAggro)) { return; }
                            UCk_Utils_Aggro_UE::Request_ClearActiveTarget(MutableAggro, {});
                        },
                        ECk_DebugRequest_Requirement::AuthorityOnly
                    },
                });

            // Request_SetActiveTarget / Request_RemoveTarget are addressed BY TRACKED ENTITY — they
            // need an entity picker, which this vocabulary does not have yet. Deferred, not forgotten.
        }
    }

    // ---- Aggro target (this entity is one tracked target of an owner) ----
    auto NativeTarget = FCk_Handle_AggroTarget{};
    if (ck_inspector_aggro::TryGetTarget(Entity, NativeTarget))
    {
        Builder.AddHeader(FText::FromString(TEXT("Aggro Target")));

        const auto CapturedEntity = Entity;

        Builder.AddWidgetRow(
            FText::FromString(TEXT("Tracked Entity:")),
            SNew(SCkDebug_EntityRef)
                .Entity_Lambda([CapturedEntity]() -> FCk_Handle
                {
                    auto Target = FCk_Handle_AggroTarget{};
                    return ck_inspector_aggro::TryGetTarget(CapturedEntity, Target)
                        ? ck::UAggroTarget_TrackedEntity_Utils::Get_StoredEntity(Target)
                        : FCk_Handle{};
                })
                .ShowName(true));

        // Threat + Score as live meters, each filled relative to the owner's strongest target. Threat reads Err
        // (the hostility axis), Score reads Info (the derived ranking) — the two tones the hand-rolled bars
        // approximated with literal red / blue fills.
        Builder.AddMeterRow(
            FText::FromString(TEXT("Threat:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            { return ck_inspector_aggro::Get_PeerFraction(CapturedEntity, &ck_inspector_aggro::Get_TargetThreat); }),
            ECk_Tone::Err,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            { return ck_inspector_aggro::Format_Value(CapturedEntity, &ck_inspector_aggro::Get_TargetThreat); }));

        Builder.AddMeterRow(
            FText::FromString(TEXT("Score:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            { return ck_inspector_aggro::Get_PeerFraction(CapturedEntity, &ck_inspector_aggro::Get_TargetScore); }),
            ECk_Tone::Info,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            { return ck_inspector_aggro::Format_Value(CapturedEntity, &ck_inspector_aggro::Get_TargetScore); }));

        Builder.AddRow(
            FText::FromString(TEXT("Distance:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AggroTarget_Score>())
                { return FText::FromString(TEXT("--")); }
                const auto Distance = CapturedEntity.Get<ck::FFragment_AggroTarget_Score>().Get_Distance();
                return FText::FromString(FString::Printf(TEXT("%.0f"), Distance));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("State:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity))
                { return FText::FromString(TEXT("--")); }

                auto State = FString();
                if (CapturedEntity.Has<ck::FTag_AggroTarget_IsActive>())         { State += TEXT("Active "); }
                if (CapturedEntity.Has<ck::FTag_AggroTarget_Perceived>())        { State += TEXT("Perceived "); }
                if (CapturedEntity.Has<ck::FTag_AggroTarget_WithinRetention>())  { State += TEXT("InRetention "); }
                if (CapturedEntity.Has<ck::FTag_AggroTarget_PendingForget>())    { State += TEXT("PendingForget "); }

                return FText::FromString(State.IsEmpty() ? FString(TEXT("--")) : State.TrimEnd());
            },
            // Left as live text, not chips: these four tags flip continuously (Perceived / retention), and a chips
            // row is a compose-time snapshot that would need a RequestRebuild per flip to stay honest.
            CkStyle::Value_Enum());

        // ---- Per-target verbs ----
        // This section IS the per-target surface: the owner section above only reports a count, so the
        // threat/perception controls live here, on the inspected target entity. AuthorityOnly, same
        // processor gate as the owner requests.
        const auto CapturedTarget = NativeTarget;

        if (ck::IsValid(CapturedTarget))
        {
            Builder.AddNumericRow(
                FText::FromString(TEXT("Set Threat:")),
                TAttribute<float>::CreateLambda([CapturedTarget]()
                {
                    if (ck::Is_NOT_Valid(CapturedTarget)) { return 0.0f; }
                    return UCk_Utils_AggroTarget_UE::Get_Threat(CapturedTarget);
                }),
                [CapturedTarget](float InThreat)
                {
                    auto MutableTarget = CapturedTarget;
                    if (ck::Is_NOT_Valid(MutableTarget)) { return; }
                    UCk_Utils_AggroTarget_UE::Request_SetThreat(MutableTarget, InThreat, {});
                },
                TOptional<float>{},
                TOptional<float>{},
                ECk_DebugRequest_Requirement::AuthorityOnly);

            // A DELTA field, not a value field — the getter reads 0 because there is no "pending
            // delta" state to read back; the committed number is the increment that gets applied.
            Builder.AddNumericRow(
                FText::FromString(TEXT("Add Threat (delta):")),
                TAttribute<float>::CreateLambda([]() { return 0.0f; }),
                [CapturedTarget](float InDelta)
                {
                    auto MutableTarget = CapturedTarget;
                    if (ck::Is_NOT_Valid(MutableTarget)) { return; }
                    UCk_Utils_AggroTarget_UE::Request_AddThreat(MutableTarget, InDelta, {});
                },
                TOptional<float>{},
                TOptional<float>{},
                ECk_DebugRequest_Requirement::AuthorityOnly);

            Builder.AddActionRow(
                FText::FromString(TEXT("Perception:")),
                {
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Mark Unperceived")),
                        FText::FromString(TEXT("UCk_Utils_AggroTarget_UE::Request_MarkUnperceived")),
                        [CapturedTarget]()
                        {
                            auto MutableTarget = CapturedTarget;
                            if (ck::Is_NOT_Valid(MutableTarget)) { return; }
                            UCk_Utils_AggroTarget_UE::Request_MarkUnperceived(MutableTarget, {});
                        },
                        ECk_DebugRequest_Requirement::AuthorityOnly
                    },
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Reset Perception")),
                        FText::FromString(TEXT("UCk_Utils_AggroTarget_UE::Request_ResetPerception")),
                        [CapturedTarget]()
                        {
                            auto MutableTarget = CapturedTarget;
                            if (ck::Is_NOT_Valid(MutableTarget)) { return; }
                            UCk_Utils_AggroTarget_UE::Request_ResetPerception(MutableTarget, {});
                        },
                        ECk_DebugRequest_Requirement::AuthorityOnly
                    },
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Forget")),
                        FText::FromString(TEXT("UCk_Utils_AggroTarget_UE::Request_Forget")),
                        [CapturedTarget]()
                        {
                            auto MutableTarget = CapturedTarget;
                            if (ck::Is_NOT_Valid(MutableTarget)) { return; }
                            UCk_Utils_AggroTarget_UE::Request_Forget(MutableTarget, {});
                        },
                        ECk_DebugRequest_Requirement::AuthorityOnly
                    },
                });
        }
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto SCkInspector_AggroAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Root = SNew(SBox);
    ChildSlot[Root];
    if (Build_AuthoredView())
    {
        Root->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    Root->SetContent(SNullWidget::NullWidget);
}

SCkInspector_AggroAuthored::~SCkInspector_AggroAuthored()
{
    Release();
}

auto SCkInspector_AggroAuthored::Get_IsOwnerAvailable() const -> bool
{
    auto Owner = FCk_Handle_Aggro{};
    return _Active && ck_inspector_aggro::TryGetOwner(_Entity, Owner);
}

auto SCkInspector_AggroAuthored::Get_IsTargetAvailable() const -> bool
{
    auto Target = FCk_Handle_AggroTarget{};
    return _Active && ck_inspector_aggro::TryGetTarget(_Entity, Target);
}

auto SCkInspector_AggroAuthored::Get_CanRequestOwner() const -> bool
{
    return Get_IsOwnerAvailable() && ck::DebugRequestGate::Evaluate(
        _Entity,
        ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled;
}

auto SCkInspector_AggroAuthored::Get_CanRequestTarget() const -> bool
{
    return Get_IsTargetAvailable() && ck::DebugRequestGate::Evaluate(
        _Entity,
        ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled;
}

auto SCkInspector_AggroAuthored::Get_OwnerDisabledReason() const -> FString
{
    return Get_IsOwnerAvailable() ? ck_inspector_aggro::GateReason(_Entity) : TEXT("Aggro owner is unavailable.");
}

auto SCkInspector_AggroAuthored::Get_TargetDisabledReason() const -> FString
{
    return Get_IsTargetAvailable() ? ck_inspector_aggro::GateReason(_Entity) : TEXT("Aggro target is unavailable.");
}

auto SCkInspector_AggroAuthored::Get_OwnerActiveTrackedEntity() const -> FCk_Handle
{
    auto Owner = FCk_Handle_Aggro{};
    if (NOT _Active || NOT ck_inspector_aggro::TryGetOwner(_Entity, Owner))
    { return {}; }

    const FCk_Handle Active = Owner.Get<ck::FFragment_Aggro_Current>().Get_ActiveTarget();
    auto Target = FCk_Handle_AggroTarget{};
    return ck_inspector_aggro::TryGetTarget(Active, Target)
        ? ck::UAggroTarget_TrackedEntity_Utils::Get_StoredEntity(Target)
        : FCk_Handle{};
}

auto SCkInspector_AggroAuthored::Get_TargetTrackedEntity() const -> FCk_Handle
{
    auto Target = FCk_Handle_AggroTarget{};
    return _Active && ck_inspector_aggro::TryGetTarget(_Entity, Target)
        ? ck::UAggroTarget_TrackedEntity_Utils::Get_StoredEntity(Target)
        : FCk_Handle{};
}

auto SCkInspector_AggroAuthored::Get_Text(const FString& InKey) const -> FString
{
    if (InKey.StartsWith(TEXT("owner-")))
    {
        auto Owner = FCk_Handle_Aggro{};
        if (NOT _Active || NOT ck_inspector_aggro::TryGetOwner(_Entity, Owner))
        { return TEXT("--"); }

        if (InKey == TEXT("owner-active-id") || InKey == TEXT("owner-active-name"))
        {
            const FCk_Handle Tracked = Get_OwnerActiveTrackedEntity();
            if (ck::Is_NOT_Valid(Tracked))
            { return InKey == TEXT("owner-active-name") ? TEXT("None") : TEXT("--"); }
            return InKey == TEXT("owner-active-id")
                ? ck::Format_UE(TEXT("{}"), Tracked.Get_Entity())
                : UCk_Utils_Handle_UE::Get_DebugName(Tracked).ToString();
        }
        if (InKey == TEXT("owner-tracked"))
        { return FString::Printf(TEXT("%d"), Owner.Get<ck::FFragment_Aggro_TargetMap>().Get_TargetsByTrackedEntity().Num()); }
        if (InKey == TEXT("owner-enabled"))
        { return Owner.Has<ck::FTag_Aggro_Disabled>() ? TEXT("Disabled") : TEXT("Enabled"); }
        if (InKey == TEXT("owner-eval"))
        {
            return Owner.Has<ck::FFragment_Aggro_EvaluationClock>()
                ? FString::Printf(TEXT("%lld"), Owner.Get<ck::FFragment_Aggro_EvaluationClock>().Get_DebugEvaluationCount())
                : TEXT("--");
        }
    }

    if (InKey.StartsWith(TEXT("target-")))
    {
        auto Target = FCk_Handle_AggroTarget{};
        if (NOT _Active || NOT ck_inspector_aggro::TryGetTarget(_Entity, Target))
        { return TEXT("--"); }

        if (InKey == TEXT("target-tracked-id") || InKey == TEXT("target-tracked-name"))
        {
            const FCk_Handle Tracked = Get_TargetTrackedEntity();
            if (ck::Is_NOT_Valid(Tracked))
            { return InKey == TEXT("target-tracked-name") ? TEXT("(Invalid)") : TEXT("--"); }
            return InKey == TEXT("target-tracked-id")
                ? ck::Format_UE(TEXT("{}"), Tracked.Get_Entity())
                : UCk_Utils_Handle_UE::Get_DebugName(Tracked).ToString();
        }
        if (InKey == TEXT("target-threat"))
        { return ck::Format_UE(TEXT("{:.1f}"), ck_inspector_aggro::Get_TargetThreat(Target)); }
        if (InKey == TEXT("target-score"))
        { return ck::Format_UE(TEXT("{:.1f}"), ck_inspector_aggro::Get_TargetScore(Target)); }
        if (InKey == TEXT("target-distance"))
        { return FString::Printf(TEXT("%.0f"), Target.Get<ck::FFragment_AggroTarget_Score>().Get_Distance()); }
        if (InKey == TEXT("target-state"))
        {
            auto State = FString{};
            if (Target.Has<ck::FTag_AggroTarget_IsActive>())        { State += TEXT("Active "); }
            if (Target.Has<ck::FTag_AggroTarget_Perceived>())       { State += TEXT("Perceived "); }
            if (Target.Has<ck::FTag_AggroTarget_WithinRetention>()) { State += TEXT("InRetention "); }
            if (Target.Has<ck::FTag_AggroTarget_PendingForget>())   { State += TEXT("PendingForget "); }
            return State.IsEmpty() ? TEXT("--") : State.TrimEnd();
        }
    }
    return TEXT("--");
}

auto SCkInspector_AggroAuthored::Get_Number(const FString& InKey) const -> float
{
    auto Target = FCk_Handle_AggroTarget{};
    if (NOT _Active || NOT ck_inspector_aggro::TryGetTarget(_Entity, Target))
    { return 0.0f; }
    if (InKey == TEXT("target-threat-fraction"))
    { return ck_inspector_aggro::Get_PeerFraction(Target, &ck_inspector_aggro::Get_TargetThreat); }
    if (InKey == TEXT("target-score-fraction"))
    { return ck_inspector_aggro::Get_PeerFraction(Target, &ck_inspector_aggro::Get_TargetScore); }
    if (InKey == TEXT("target-set-threat"))
    { return UCk_Utils_AggroTarget_UE::Get_Threat(Target); }
    return 0.0f;
}

auto SCkInspector_AggroAuthored::Get_Bool(const FString& InKey) const -> bool
{
    auto Owner = FCk_Handle_Aggro{};
    return InKey == TEXT("owner-enabled")
        && _Active
        && ck_inspector_aggro::TryGetOwner(_Entity, Owner)
        && UCk_Utils_Aggro_UE::Get_IsEnabled(Owner);
}

auto SCkInspector_AggroAuthored::Get_DiffColor(const FString& InLabel) const -> FLinearColor
{
    return _DiffLabels.Contains(InLabel) ? CkStyle::Accent() : CkStyle::Text();
}

auto SCkInspector_AggroAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        _LoadError = Plugin.IsValid()
            ? FString::Join(RegistryResult.Errors, TEXT("\n"))
            : TEXT("CkDebugger plugin could not be resolved for Aggro inspector authored resources.");
        return false;
    }

    const TWeakPtr<SCkInspector_AggroAuthored> Weak = SharedThis(this);
    auto Data = FCkUiView::FDataBindings{};
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert();
    });
    Data.Visibility.Add(TEXT("aggro-owner-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsOwnerAvailable(); }));
    Data.Visibility.Add(TEXT("aggro-target-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsTargetAvailable(); }));
    Data.Visibility.Add(TEXT("aggro-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Get_IsOwnerAvailable() && NOT Widget->Get_IsTargetAvailable(); }));
    Data.Visibility.Add(TEXT("aggro-owner-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequestOwner(); }));
    Data.Visibility.Add(TEXT("aggro-target-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequestTarget(); }));

    for (const TCHAR* Key : {
        TEXT("owner-active-id"), TEXT("owner-active-name"), TEXT("owner-tracked"), TEXT("owner-enabled"),
        TEXT("owner-eval"), TEXT("target-tracked-id"), TEXT("target-tracked-name"), TEXT("target-threat"),
        TEXT("target-score"), TEXT("target-distance"), TEXT("target-state")})
    {
        const FString Name{Key};
        Data.Text.Add(TEXT("aggro-") + Name, TAttribute<FText>::CreateLambda([Weak, Name]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString(Widget->Get_Text(Name))
                : FText::GetEmpty();
        }));
    }
    Data.Text.Add(TEXT("aggro-owner-disabled"), TAttribute<FText>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_OwnerDisabledReason()) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("aggro-target-disabled"), TAttribute<FText>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_TargetDisabledReason()) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("aggro-owner-clear-all-label"), FText::FromString(TEXT("Clear All")));
    Data.Text.Add(TEXT("aggro-owner-clear-active-label"), FText::FromString(TEXT("Clear Active")));
    Data.Text.Add(TEXT("aggro-target-mark-unperceived-label"), FText::FromString(TEXT("Mark Unperceived")));
    Data.Text.Add(TEXT("aggro-target-reset-perception-label"), FText::FromString(TEXT("Reset Perception")));
    Data.Text.Add(TEXT("aggro-target-forget-label"), FText::FromString(TEXT("Forget")));
    Data.Text.Add(TEXT("aggro-owner-clear-all-tooltip"), FText::FromString(TEXT("UCk_Utils_Aggro_UE::Request_ClearAllTargets")));
    Data.Text.Add(TEXT("aggro-owner-clear-active-tooltip"), FText::FromString(TEXT("UCk_Utils_Aggro_UE::Request_ClearActiveTarget")));
    Data.Text.Add(TEXT("aggro-target-mark-unperceived-tooltip"), FText::FromString(TEXT("UCk_Utils_AggroTarget_UE::Request_MarkUnperceived")));
    Data.Text.Add(TEXT("aggro-target-reset-perception-tooltip"), FText::FromString(TEXT("UCk_Utils_AggroTarget_UE::Request_ResetPerception")));
    Data.Text.Add(TEXT("aggro-target-forget-tooltip"), FText::FromString(TEXT("UCk_Utils_AggroTarget_UE::Request_Forget")));

    const TMap<FString, FString> DiffBindings = {
        {TEXT("aggro-owner-active-diff"), TEXT("Active Target:")},
        {TEXT("aggro-owner-tracked-diff"), TEXT("Tracked Targets:")},
        {TEXT("aggro-owner-enabled-diff"), TEXT("Enabled:")},
        {TEXT("aggro-owner-eval-diff"), TEXT("Eval Count:")},
        {TEXT("aggro-owner-toggle-diff"), TEXT("Enable/Disable:")},
        {TEXT("aggro-owner-targets-diff"), TEXT("Targets:")},
        {TEXT("aggro-target-tracked-diff"), TEXT("Tracked Entity:")},
        {TEXT("aggro-target-threat-diff"), TEXT("Threat:")},
        {TEXT("aggro-target-score-diff"), TEXT("Score:")},
        {TEXT("aggro-target-distance-diff"), TEXT("Distance:")},
        {TEXT("aggro-target-state-diff"), TEXT("State:")},
        {TEXT("aggro-target-set-threat-diff"), TEXT("Set Threat:")},
        {TEXT("aggro-target-add-threat-diff"), TEXT("Add Threat (delta):")},
        {TEXT("aggro-target-perception-diff"), TEXT("Perception:")},
    };
    for (const auto& Pair : DiffBindings)
    {
        const FString Label = Pair.Value;
        Data.Color.Add(Pair.Key, TAttribute<FLinearColor>::CreateLambda([Weak, Label]()
        { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_DiffColor(Label) : CkStyle::Text(); }));
    }
    Data.Color.Add(TEXT("aggro-owner-status-foreground"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return CkStyle::GetToneColor(Widget.IsValid() && Widget->Get_Bool(TEXT("owner-enabled")) ? ECk_Tone::Ok : ECk_Tone::Err);
    }));
    Data.Color.Add(TEXT("aggro-owner-status-background"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return CkStyle::GetToneDimColor(Widget.IsValid() && Widget->Get_Bool(TEXT("owner-enabled")) ? ECk_Tone::Ok : ECk_Tone::Err);
    }));
    Data.Color.Add(TEXT("aggro-threat-fill"), CkStyle::GetToneColor(ECk_Tone::Err));
    Data.Color.Add(TEXT("aggro-score-fill"), CkStyle::GetToneColor(ECk_Tone::Info));

    for (const TCHAR* Key : {TEXT("target-threat-fraction"), TEXT("target-score-fraction"), TEXT("target-set-threat"), TEXT("target-add-threat")})
    {
        const FString Name{Key};
        Data.Number.Add(TEXT("aggro-") + Name, TAttribute<float>::CreateLambda([Weak, Name]()
        { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_Number(Name) : 0.0f; }));
    }
    Data.Visibility.Add(TEXT("aggro-owner-enabled"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_Bool(TEXT("owner-enabled")); }));
    Data.BoolChanged.Add(TEXT("aggro-owner-enabled-changed"), FCkUiOnBoolChanged::CreateLambda([Weak](const bool bInEnabled)
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Set_OwnerEnabled(bInEnabled); } }));
    Data.NumberCommitted.Add(TEXT("aggro-target-set-threat-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak](const float InValue, ETextCommit::Type)
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Commit_SetThreat(InValue); } }));
    Data.NumberCommitted.Add(TEXT("aggro-target-add-threat-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak](const float InValue, ETextCommit::Type)
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Commit_AddThreat(InValue); } }));

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("aggro-owner-active-navigate"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Navigate_OwnerActive(); } }));
    Actions.Add(TEXT("aggro-target-tracked-navigate"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Navigate_TargetTracked(); } }));
    Actions.Add(TEXT("aggro-owner-clear-all"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_ClearAllTargets(); } }));
    Actions.Add(TEXT("aggro-owner-clear-active"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_ClearActiveTarget(); } }));
    Actions.Add(TEXT("aggro-target-mark-unperceived"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_MarkUnperceived(); } }));
    Actions.Add(TEXT("aggro-target-reset-perception"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_ResetPerception(); } }));
    Actions.Add(TEXT("aggro-target-forget"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Forget(); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(Root, TEXT("EcsInspectorAggro.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorAggro.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }

    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_AggroAuthored::Navigate_OwnerActive() -> void
{
    const FCk_Handle Current = Get_OwnerActiveTrackedEntity();
    if (_Active && ck::IsValid(Current))
    { ck::DebugNav::Goto_Entity(Current); }
}

auto SCkInspector_AggroAuthored::Navigate_TargetTracked() -> void
{
    const FCk_Handle Current = Get_TargetTrackedEntity();
    if (_Active && ck::IsValid(Current))
    { ck::DebugNav::Goto_Entity(Current); }
}

auto SCkInspector_AggroAuthored::Set_OwnerEnabled(const bool bInEnabled) -> void
{
    auto Owner = FCk_Handle_Aggro{};
    if (NOT _Active || NOT ck_inspector_aggro::TryGetOwner(_Entity, Owner)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    UCk_Utils_Aggro_UE::Request_EnableDisable(
        Owner,
        bInEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable,
        {});
}

auto SCkInspector_AggroAuthored::Request_ClearAllTargets() -> void
{
    auto Owner = FCk_Handle_Aggro{};
    if (NOT _Active || NOT ck_inspector_aggro::TryGetOwner(_Entity, Owner)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    UCk_Utils_Aggro_UE::Request_ClearAllTargets(Owner, {});
}

auto SCkInspector_AggroAuthored::Request_ClearActiveTarget() -> void
{
    auto Owner = FCk_Handle_Aggro{};
    if (NOT _Active || NOT ck_inspector_aggro::TryGetOwner(_Entity, Owner)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    UCk_Utils_Aggro_UE::Request_ClearActiveTarget(Owner, {});
}

auto SCkInspector_AggroAuthored::Commit_SetThreat(const float InThreat) -> void
{
    auto Target = FCk_Handle_AggroTarget{};
    if (NOT _Active || NOT FMath::IsFinite(InThreat) || NOT ck_inspector_aggro::TryGetTarget(_Entity, Target)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    UCk_Utils_AggroTarget_UE::Request_SetThreat(Target, InThreat, {});
}

auto SCkInspector_AggroAuthored::Commit_AddThreat(const float InDelta) -> void
{
    auto Target = FCk_Handle_AggroTarget{};
    if (NOT _Active || NOT FMath::IsFinite(InDelta) || NOT ck_inspector_aggro::TryGetTarget(_Entity, Target)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    UCk_Utils_AggroTarget_UE::Request_AddThreat(Target, InDelta, {});
}

auto SCkInspector_AggroAuthored::Request_MarkUnperceived() -> void
{
    auto Target = FCk_Handle_AggroTarget{};
    if (NOT _Active || NOT ck_inspector_aggro::TryGetTarget(_Entity, Target)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    UCk_Utils_AggroTarget_UE::Request_MarkUnperceived(Target, {});
}

auto SCkInspector_AggroAuthored::Request_ResetPerception() -> void
{
    auto Target = FCk_Handle_AggroTarget{};
    if (NOT _Active || NOT ck_inspector_aggro::TryGetTarget(_Entity, Target)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    UCk_Utils_AggroTarget_UE::Request_ResetPerception(Target, {});
}

auto SCkInspector_AggroAuthored::Request_Forget() -> void
{
    auto Target = FCk_Handle_AggroTarget{};
    if (NOT _Active || NOT ck_inspector_aggro::TryGetTarget(_Entity, Target)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    UCk_Utils_AggroTarget_UE::Request_Forget(Target, {});
}

auto SCkInspector_AggroAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid())
    { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else
    { _LoadError.Reset(); }
}

auto SCkInspector_AggroAuthored::Release() -> void
{
    if (NOT _Active)
    { return; }
    _Active = false;
    _Entity = {};
    _DiffLabels.Reset();
    _View.Reset();
    _Mounted = false;
}

FCkInspector_Aggro::~FCkInspector_Aggro()
{
    OnDeactivated();
}

auto FCkInspector_Aggro::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> Native = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return Native; }

    auto DiffLabels = TSet<FString>{};
    for (const TCHAR* Label : {
        TEXT("Active Target:"), TEXT("Tracked Targets:"), TEXT("Enabled:"), TEXT("Eval Count:"),
        TEXT("Enable/Disable:"), TEXT("Targets:"), TEXT("Tracked Entity:"), TEXT("Threat:"),
        TEXT("Score:"), TEXT("Distance:"), TEXT("State:"), TEXT("Set Threat:"),
        TEXT("Add Threat (delta):"), TEXT("Perception:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }

    const TSharedRef<SCkInspector_AggroAuthored> Authored = SNew(SCkInspector_AggroAuthored)
        .Entity(Entity)
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return Native;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Aggro::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_AggroAuthored>& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}

// =====================================================================================================================

auto FCkInspector_Aggro::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
