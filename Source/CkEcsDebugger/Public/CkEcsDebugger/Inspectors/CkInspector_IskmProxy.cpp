#include "CkInspector_IskmProxy.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Fragment.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Utils.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Fragment_Data.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IskmProxy)

// =====================================================================================================================

namespace ck_inspector_iskm_proxy
{
    auto TryGetProxy(const FCk_Handle& InEntity, FCk_Handle_IskmProxy& OutProxy) -> bool
    {
        OutProxy = {};
        if (ck::Is_NOT_Valid(InEntity)
            || InEntity.Has<ck::FTag_DestroyEntity_Initiate>()
            || InEntity.Has<ck::FTag_DestroyEntity_EndPlay>()
            || InEntity.Has<ck::FTag_DestroyEntity_Teardown>()
            || InEntity.Has<ck::FTag_DestroyEntity_Await>()
            || InEntity.Has<ck::FTag_DestroyEntity_Finalize>())
        { return false; }

        auto Mutable = InEntity;
        OutProxy = UCk_Utils_IskmProxy_UE::Cast(Mutable);
        return ck::IsValid(OutProxy) && InEntity.Has_All<
            ck::FFragment_IskmProxy_Params,
            ck::FFragment_IskmProxy_Current,
            ck::FFragment_IskmProxy_AnimState,
            ck::FFragment_IskmProxy_PoseSource,
            ck::FFragment_IskmProxy_CustomData,
            ck::FFragment_IskmProxy_MaterialOverrides,
            ck::FFragment_IskmProxy_MorphTargets,
            ck::FFragment_IskmProxy_Requests>();
    }

    auto GetGate(const FCk_Handle& InEntity) -> FCk_DebugRequest_GateVerdict
    {
        auto Proxy = FCk_Handle_IskmProxy{};
        if (NOT TryGetProxy(InEntity, Proxy))
        { return {false, FText::FromString(TEXT("ISKM Proxy is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::CosmeticOnly);
    }

    auto IsReadable(const FCk_Handle_IskmProxy& InProxy) -> bool
    {
        auto Resolved = FCk_Handle_IskmProxy{};
        return TryGetProxy(InProxy, Resolved);
    }

    auto DiffColor(const bool InDiffMarked) -> FLinearColor
    { return InDiffMarked ? CkStyle::Accent() : CkStyle::Text(); }

    static auto Get_PoseSourceText(ECk_IskmProxy_PoseSource InPoseSource) -> FText
    {
        switch (InPoseSource)
        {
            case ECk_IskmProxy_PoseSource::Sequence: return FText::FromString(TEXT("Sequence"));
            case ECk_IskmProxy_PoseSource::AnimBP:   return FText::FromString(TEXT("AnimBP"));
            case ECk_IskmProxy_PoseSource::Ragdoll:  return FText::FromString(TEXT("Ragdoll"));
            default:                                 return FText::FromString(TEXT("Unknown"));
        }
    }

    // Ragdoll is the one pose source that means "physics took over" — warn tone; the two
    // authored sources are informational.
    static auto Get_PoseSourceTone(ECk_IskmProxy_PoseSource InPoseSource) -> ECk_Tone
    {
        switch (InPoseSource)
        {
            case ECk_IskmProxy_PoseSource::Sequence: return ECk_Tone::Info;
            case ECk_IskmProxy_PoseSource::AnimBP:   return ECk_Tone::Accent;
            case ECk_IskmProxy_PoseSource::Ragdoll:  return ECk_Tone::Warn;
            default:                                 return ECk_Tone::Neutral;
        }
    }
}

// =====================================================================================================================

auto FCkInspector_IskmProxy::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Iskm Proxy"));
}

auto FCkInspector_IskmProxy::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return ck_inspector_iskm_proxy::TryGetProxy(Entity, Proxy);
}

auto FCkInspector_IskmProxy::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = BuildIskmProxyGrid(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : TArray<FString>{
        TEXT("Pose Source:"), TEXT("Playing Animation:"), TEXT("Play Time / Length:"),
        TEXT("AnimInstance Class:"), TEXT("Active Montage:"), TEXT("Ragdolling:"),
        TEXT("Attached Submeshes:"), TEXT("Custom Data Slot 0:"), TEXT("Visible:"),
        TEXT("Play Rate:"), TEXT("Actions:"), TEXT("Morph Target:"), TEXT("  Weight:"),
        TEXT("Custom Data Slot:"), TEXT("  Value:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }
    const TSharedRef<SCkInspector_IskmProxyAuthored> Authored = SNew(SCkInspector_IskmProxyAuthored)
        .Entity(Entity).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

// =====================================================================================================================

auto FCkInspector_IskmProxy::BuildIskmProxyGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto ProxyHandle = FCk_Handle_IskmProxy{};
    if (NOT ck_inspector_iskm_proxy::TryGetProxy(Entity, ProxyHandle))
    {
        return Builder.Build(Entity, FString());
    }

    const auto CapturedProxy = ProxyHandle;
    const auto CapturedEntity = Entity;

    // Row-owned intent mirrors. Visibility and play rate have SET requests but no getter — nothing in
    // the proxy's Current fragment records either — so these rows show the last value THIS inspector
    // asked for (seeded at the feature defaults), not an engine read. They die with the row, exactly
    // like the Inventories occupancy cache; a rebuild reseeds them.
    const auto VisibilityIntent = MakeShared<bool>(true);
    const auto PlayRateIntent   = MakeShared<float>(1.0f);

    // Slot / morph selectors: which slot or morph the value editor beside them addresses. Both values
    // ARE readable, so only the SELECTOR is row-owned state — the value row itself is a live read.
    const auto CustomDataSlot = MakeShared<int32>(0);
    const auto MorphName      = MakeShared<FName>(NAME_None);

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Pose Source:")),
        TAttribute<FText>::CreateLambda([CapturedProxy]()
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            return ck_inspector_iskm_proxy::Get_PoseSourceText(UCk_Utils_IskmProxy_UE::Get_PoseSource(CapturedProxy));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedProxy]()
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return ECk_Tone::Neutral; }
            return ck_inspector_iskm_proxy::Get_PoseSourceTone(UCk_Utils_IskmProxy_UE::Get_PoseSource(CapturedProxy));
        }));

    // Playing Animation
    Builder.AddRow(
        FText::FromString(TEXT("Playing Animation:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto* Sequence = UCk_Utils_IskmProxy_UE::Get_PlayingAnimation(CapturedProxy);
            if (NOT ck::IsValid(Sequence, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("None")); }
            return FText::FromString(Sequence->GetName());
        },
        CkStyle::Value_Object());

    Builder.AddMeterRow(
        FText::FromString(TEXT("Play Time / Length:")),
        TAttribute<float>::CreateLambda([CapturedProxy]()
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return 0.0f; }
            const auto Length = UCk_Utils_IskmProxy_UE::Get_PlayLength(CapturedProxy);
            if (Length <= 0.0f) { return 0.0f; }
            return UCk_Utils_IskmProxy_UE::Get_PlayTime(CapturedProxy) / Length;
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedProxy]()
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Time   = UCk_Utils_IskmProxy_UE::Get_PlayTime(CapturedProxy);
            const auto Length = UCk_Utils_IskmProxy_UE::Get_PlayLength(CapturedProxy);
            return FText::FromString(ck::Format_UE(TEXT("{:.2f} / {:.2f}s"), Time, Length));
        }));

    // AnimInstance Class
    Builder.AddRow(
        FText::FromString(TEXT("AnimInstance Class:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto* Instance = UCk_Utils_IskmProxy_UE::Get_AnimInstance(CapturedProxy);
            if (NOT ck::IsValid(Instance, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("(none — Sequence mode)")); }
            return FText::FromString(Instance->GetClass()->GetName());
        },
        CkStyle::Value_Object());

    // Active Montage
    Builder.AddRow(
        FText::FromString(TEXT("Active Montage:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto* Montage = UCk_Utils_IskmProxy_UE::Get_ActiveMontage(CapturedProxy);
            if (NOT ck::IsValid(Montage, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("None")); }
            return FText::FromString(Montage->GetName());
        },
        CkStyle::Value_Object());

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Ragdolling:")),
        TAttribute<FText>::CreateLambda([CapturedProxy]()
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(UCk_Utils_IskmProxy_UE::Get_IsRagdolling(CapturedProxy)
                ? TEXT("Yes")
                : TEXT("No"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedProxy]()
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return ECk_Tone::Neutral; }
            return UCk_Utils_IskmProxy_UE::Get_IsRagdolling(CapturedProxy)
                ? ECk_Tone::Warn
                : ECk_Tone::Neutral;
        }));

    // Attached Submeshes
    Builder.AddRow(
        FText::FromString(TEXT("Attached Submeshes:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Count = UCk_Utils_IskmProxy_UE::Get_NumAttachedSubmeshes(CapturedProxy);
            return FText::FromString(FString::FromInt(Count));
        },
        CkStyle::Value_Numeric());

    // Custom Data Slot 0
    Builder.AddRow(
        FText::FromString(TEXT("Custom Data Slot 0:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Value = UCk_Utils_IskmProxy_UE::Get_CustomDataFloat(CapturedProxy, 0);
            return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Value));
        },
        CkStyle::Value_Numeric());

    // ================================================================================================
    // Controls. Everything an Iskm proxy writes is CosmeticOnly — a dedicated server owns no SKMC, so
    // these greyed out there rather than firing requests nothing will ever render.
    // ================================================================================================

    Builder.AddHeader(FText::FromString(TEXT("Controls")));

    Builder.AddToggleRow(
        FText::FromString(TEXT("Visible:")),
        TAttribute<bool>::CreateLambda([VisibilityIntent]() { return *VisibilityIntent; }),
        [CapturedEntity, VisibilityIntent](bool InIsVisible)
        {
            auto Current = FCk_Handle_IskmProxy{};
            if (NOT ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                || NOT ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
            { return; }
            *VisibilityIntent = InIsVisible;
            UCk_Utils_IskmProxy_UE::Request_SetVisibility(Current, InIsVisible, {});
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Play Rate:")),
        TAttribute<float>::CreateLambda([PlayRateIntent]() { return *PlayRateIntent; }),
        [CapturedEntity, PlayRateIntent](float InRate)
        {
            auto Current = FCk_Handle_IskmProxy{};
            if (NOT ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                || NOT ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
            { return; }
            *PlayRateIntent = InRate;
            UCk_Utils_IskmProxy_UE::Request_SetPlayRate(Current, InRate, {});
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddActionRow(
        FText::FromString(TEXT("Actions:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Stop Anim")),
                FText::FromString(TEXT("Request_StopAnimation — halts the sequence currently driving the proxy.")),
                [CapturedEntity]()
                {
                    auto Current = FCk_Handle_IskmProxy{};
                    if (ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                        && ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
                    { UCk_Utils_IskmProxy_UE::Request_StopAnimation(Current, {}, {}); }
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("End Ragdoll")),
                FText::FromString(TEXT("Request_EndRagdoll — returns the proxy from physics to its authored pose source.")),
                [CapturedEntity]()
                {
                    auto Current = FCk_Handle_IskmProxy{};
                    if (ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                        && UCk_Utils_IskmProxy_UE::Get_IsRagdolling(Current)
                        && ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
                    { UCk_Utils_IskmProxy_UE::Request_EndRagdoll(Current, {}, {}); }
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Clear Morphs")),
                FText::FromString(TEXT("Request_ClearMorphTargets — drops every recorded morph weight on this proxy.")),
                [CapturedEntity]()
                {
                    auto Current = FCk_Handle_IskmProxy{};
                    if (ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                        && ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
                    { UCk_Utils_IskmProxy_UE::Request_ClearMorphTargets(Current, {}); }
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Clear Materials")),
                FText::FromString(TEXT("Request_ClearMaterialOverrides — restores the mesh's default material on every slot.")),
                [CapturedEntity]()
                {
                    auto Current = FCk_Handle_IskmProxy{};
                    if (ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                        && ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
                    { UCk_Utils_IskmProxy_UE::Request_ClearMaterialOverrides(Current, {}); }
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Detach Submeshes")),
                FText::FromString(TEXT("Request_DetachAllSubmeshes — strips every attached outfit submesh.")),
                [CapturedEntity]()
                {
                    auto Current = FCk_Handle_IskmProxy{};
                    if (ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                        && ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
                    { UCk_Utils_IskmProxy_UE::Request_DetachAllSubmeshes(Current, {}); }
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
        });

    // ---- Morph target: name selector + the weight recorded for it ----

    Builder.AddNameEntryRow(
        FText::FromString(TEXT("Morph Target:")),
        TAttribute<FText>::CreateLambda([MorphName]()
        {
            return MorphName->IsNone() ? FText::FromString(TEXT("(none)")) : FText::FromName(*MorphName);
        }),
        [CapturedEntity, MorphName](FName InName)
        {
            auto Current = FCk_Handle_IskmProxy{};
            if (ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                && ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
            { *MorphName = InName; }
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("  Weight:")),
        TAttribute<float>::CreateLambda([CapturedProxy, MorphName]()
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy) || MorphName->IsNone()) { return 0.0f; }
            return UCk_Utils_IskmProxy_UE::Get_MorphTarget(CapturedProxy, *MorphName);
        }),
        [CapturedEntity, MorphName](float InValue)
        {
            auto Current = FCk_Handle_IskmProxy{};
            if (MorphName->IsNone()
                || NOT ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                || NOT ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
            { return; }
            UCk_Utils_IskmProxy_UE::Request_SetMorphTarget(Current, *MorphName, InValue, {});
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    // ---- Custom data: slot selector + the value recorded in it ----

    Builder.AddIntegerRow(
        FText::FromString(TEXT("Custom Data Slot:")),
        TAttribute<int32>::CreateLambda([CustomDataSlot]() { return *CustomDataSlot; }),
        [CapturedEntity, CustomDataSlot](int32 InSlot)
        {
            auto Current = FCk_Handle_IskmProxy{};
            if (ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                && ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
            { *CustomDataSlot = InSlot; }
        },
        0,
        TOptional<int32>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("  Value:")),
        TAttribute<float>::CreateLambda([CapturedProxy, CustomDataSlot]()
        {
            if (NOT ck_inspector_iskm_proxy::IsReadable(CapturedProxy)) { return 0.0f; }
            return UCk_Utils_IskmProxy_UE::Get_CustomDataFloat(CapturedProxy, *CustomDataSlot);
        }),
        [CapturedEntity, CustomDataSlot](float InValue)
        {
            auto Current = FCk_Handle_IskmProxy{};
            if (NOT ck_inspector_iskm_proxy::TryGetProxy(CapturedEntity, Current)
                || NOT ck_inspector_iskm_proxy::GetGate(CapturedEntity).IsEnabled)
            { return; }
            UCk_Utils_IskmProxy_UE::Request_SetCustomDataFloat(Current, *CustomDataSlot, InValue, {});
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto SCkInspector_IskmProxyAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView()) { Host->SetContent(_View->GetRegion(TEXT("main"))); return; }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_IskmProxyAuthored::~SCkInspector_IskmProxyAuthored()
{ Release(); }

auto SCkInspector_IskmProxyAuthored::Get_IsAvailable() const -> bool
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy);
}

auto SCkInspector_IskmProxyAuthored::Get_CanRequest() const -> bool
{ return Get_IsAvailable() && ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled; }

auto SCkInspector_IskmProxyAuthored::Get_RequestDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("ISKM Proxy is unavailable."); }
    const FCk_DebugRequest_GateVerdict Verdict = ck_inspector_iskm_proxy::GetGate(_Entity);
    if (NOT Verdict.IsEnabled) { return Verdict.Reason.ToString(); }
    return Get_CanEndRagdoll() ? FString{} : TEXT("End Ragdoll requires an active ragdoll.");
}

auto SCkInspector_IskmProxyAuthored::Get_PoseSourceText() const -> FString
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        ? ck_inspector_iskm_proxy::Get_PoseSourceText(UCk_Utils_IskmProxy_UE::Get_PoseSource(Proxy)).ToString() : TEXT("--");
}

auto SCkInspector_IskmProxyAuthored::Get_PlayingAnimationText() const -> FString
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)) { return TEXT("--"); }
    const auto* Sequence = UCk_Utils_IskmProxy_UE::Get_PlayingAnimation(Proxy);
    return ck::IsValid(Sequence, ck::IsValid_Policy_NullptrOnly{}) ? Sequence->GetName() : TEXT("None");
}

auto SCkInspector_IskmProxyAuthored::Get_PlayTimeText() const -> FString
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)) { return TEXT("--"); }
    return ck::Format_UE(TEXT("{:.2f} / {:.2f}s"), UCk_Utils_IskmProxy_UE::Get_PlayTime(Proxy), UCk_Utils_IskmProxy_UE::Get_PlayLength(Proxy));
}

auto SCkInspector_IskmProxyAuthored::Get_PoseSourceForeground() const -> FLinearColor
{
    auto Proxy = FCk_Handle_IskmProxy{};
    const ECk_Tone Tone = _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        ? ck_inspector_iskm_proxy::Get_PoseSourceTone(UCk_Utils_IskmProxy_UE::Get_PoseSource(Proxy))
        : ECk_Tone::Neutral;
    return _Active ? CkStyle::GetToneColor(Tone) : FLinearColor::Transparent;
}

auto SCkInspector_IskmProxyAuthored::Get_PoseSourceBackground() const -> FLinearColor
{
    auto Proxy = FCk_Handle_IskmProxy{};
    const ECk_Tone Tone = _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        ? ck_inspector_iskm_proxy::Get_PoseSourceTone(UCk_Utils_IskmProxy_UE::Get_PoseSource(Proxy))
        : ECk_Tone::Neutral;
    return _Active ? CkStyle::GetToneDimColor(Tone) : FLinearColor::Transparent;
}

auto SCkInspector_IskmProxyAuthored::Get_PlayTimeFraction() const -> float
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)) { return 0.0f; }
    const float Length = UCk_Utils_IskmProxy_UE::Get_PlayLength(Proxy);
    return Length > 0.0f ? FMath::Clamp(UCk_Utils_IskmProxy_UE::Get_PlayTime(Proxy) / Length, 0.0f, 1.0f) : 0.0f;
}

auto SCkInspector_IskmProxyAuthored::Get_AnimInstanceText() const -> FString
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)) { return TEXT("--"); }
    const auto* Instance = UCk_Utils_IskmProxy_UE::Get_AnimInstance(Proxy);
    return ck::IsValid(Instance, ck::IsValid_Policy_NullptrOnly{}) ? Instance->GetClass()->GetName() : TEXT("(none — Sequence mode)");
}

auto SCkInspector_IskmProxyAuthored::Get_ActiveMontageText() const -> FString
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)) { return TEXT("--"); }
    const auto* Montage = UCk_Utils_IskmProxy_UE::Get_ActiveMontage(Proxy);
    return ck::IsValid(Montage, ck::IsValid_Policy_NullptrOnly{}) ? Montage->GetName() : TEXT("None");
}

auto SCkInspector_IskmProxyAuthored::Get_RagdollingText() const -> FString
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        ? (UCk_Utils_IskmProxy_UE::Get_IsRagdolling(Proxy) ? TEXT("Yes") : TEXT("No")) : TEXT("--");
}

auto SCkInspector_IskmProxyAuthored::Get_RagdollingForeground() const -> FLinearColor
{
    auto Proxy = FCk_Handle_IskmProxy{};
    const ECk_Tone Tone = _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        && UCk_Utils_IskmProxy_UE::Get_IsRagdolling(Proxy) ? ECk_Tone::Warn : ECk_Tone::Neutral;
    return _Active ? CkStyle::GetToneColor(Tone) : FLinearColor::Transparent;
}

auto SCkInspector_IskmProxyAuthored::Get_RagdollingBackground() const -> FLinearColor
{
    auto Proxy = FCk_Handle_IskmProxy{};
    const ECk_Tone Tone = _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        && UCk_Utils_IskmProxy_UE::Get_IsRagdolling(Proxy) ? ECk_Tone::Warn : ECk_Tone::Neutral;
    return _Active ? CkStyle::GetToneDimColor(Tone) : FLinearColor::Transparent;
}

auto SCkInspector_IskmProxyAuthored::Get_SubmeshesText() const -> FString
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        ? FString::FromInt(UCk_Utils_IskmProxy_UE::Get_NumAttachedSubmeshes(Proxy)) : TEXT("--");
}

auto SCkInspector_IskmProxyAuthored::Get_CustomDataSlotZeroText() const -> FString
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        ? ck::Format_UE(TEXT("{:.3f}"), UCk_Utils_IskmProxy_UE::Get_CustomDataFloat(Proxy, 0)) : TEXT("--");
}

auto SCkInspector_IskmProxyAuthored::Get_MorphNameText() const -> FString
{ return _MorphName.IsNone() ? TEXT("(none)") : _MorphName.ToString(); }

auto SCkInspector_IskmProxyAuthored::Get_MorphWeight() const -> float
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return _Active && NOT _MorphName.IsNone() && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        ? UCk_Utils_IskmProxy_UE::Get_MorphTarget(Proxy, _MorphName) : 0.0f;
}

auto SCkInspector_IskmProxyAuthored::Get_CustomDataValue() const -> float
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return _Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        ? UCk_Utils_IskmProxy_UE::Get_CustomDataFloat(Proxy, _CustomDataSlot) : 0.0f;
}

auto SCkInspector_IskmProxyAuthored::Get_CanEndRagdoll() const -> bool
{
    auto Proxy = FCk_Handle_IskmProxy{};
    return Get_CanRequest() && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        && UCk_Utils_IskmProxy_UE::Get_IsRagdolling(Proxy);
}

auto SCkInspector_IskmProxyAuthored::Set_VisibilityIntent(const bool InVisible) -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy) || NOT ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled) { return; }
    _VisibilityIntent = InVisible;
    UCk_Utils_IskmProxy_UE::Request_SetVisibility(Proxy, InVisible, {});
}

auto SCkInspector_IskmProxyAuthored::Commit_PlayRate(const float InRate) -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy) || NOT ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled) { return; }
    _PlayRateIntent = InRate;
    UCk_Utils_IskmProxy_UE::Request_SetPlayRate(Proxy, InRate, {});
}

auto SCkInspector_IskmProxyAuthored::Commit_MorphName(const FString& InName) -> void
{ if (_Active && Get_CanRequest()) { _MorphName = FName{InName}; } }

auto SCkInspector_IskmProxyAuthored::Commit_MorphWeight(const float InValue) -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || _MorphName.IsNone() || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy) || NOT ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled) { return; }
    UCk_Utils_IskmProxy_UE::Request_SetMorphTarget(Proxy, _MorphName, InValue, {});
}

auto SCkInspector_IskmProxyAuthored::Commit_CustomDataSlot(const float InSlot) -> void
{ if (_Active && Get_CanRequest()) { _CustomDataSlot = FMath::Max(0, FMath::RoundToInt(InSlot)); } }

auto SCkInspector_IskmProxyAuthored::Commit_CustomDataValue(const float InValue) -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (NOT _Active || NOT ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy) || NOT ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled) { return; }
    UCk_Utils_IskmProxy_UE::Request_SetCustomDataFloat(Proxy, _CustomDataSlot, InValue, {});
}

auto SCkInspector_IskmProxyAuthored::Request_StopAnimation() -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (_Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy) && ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled)
    { UCk_Utils_IskmProxy_UE::Request_StopAnimation(Proxy, {}, {}); }
}

auto SCkInspector_IskmProxyAuthored::Request_EndRagdoll() -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (_Active && Get_CanEndRagdoll() && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy)
        && ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled)
    { UCk_Utils_IskmProxy_UE::Request_EndRagdoll(Proxy, {}, {}); }
}

auto SCkInspector_IskmProxyAuthored::Request_ClearMorphs() -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (_Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy) && ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled)
    { UCk_Utils_IskmProxy_UE::Request_ClearMorphTargets(Proxy, {}); }
}

auto SCkInspector_IskmProxyAuthored::Request_ClearMaterials() -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (_Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy) && ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled)
    { UCk_Utils_IskmProxy_UE::Request_ClearMaterialOverrides(Proxy, {}); }
}

auto SCkInspector_IskmProxyAuthored::Request_DetachSubmeshes() -> void
{
    auto Proxy = FCk_Handle_IskmProxy{};
    if (_Active && ck_inspector_iskm_proxy::TryGetProxy(_Entity, Proxy) && ck_inspector_iskm_proxy::GetGate(_Entity).IsEnabled)
    { UCk_Utils_IskmProxy_UE::Request_DetachAllSubmeshes(Proxy, {}); }
}

auto SCkInspector_IskmProxyAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_IskmProxyAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& Key, FString (SCkInspector_IskmProxyAuthored::* Getter)() const)
    {
        Data.Text.Add(Key, TAttribute<FText>::CreateLambda([Weak, Getter]()
        { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString((Widget.Get()->*Getter)()) : FText::GetEmpty(); }));
    };
    const auto BindColor = [&Data, Weak](const FString& Key, FLinearColor (SCkInspector_IskmProxyAuthored::* Getter)() const)
    {
        Data.Color.Add(Key, TAttribute<FLinearColor>::CreateLambda([Weak, Getter]()
        { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? (Widget.Get()->*Getter)() : FLinearColor::Transparent; }));
    };
    BindText(TEXT("iskm-proxy-pose"), &SCkInspector_IskmProxyAuthored::Get_PoseSourceText);
    BindColor(TEXT("iskm-proxy-pose-foreground"), &SCkInspector_IskmProxyAuthored::Get_PoseSourceForeground);
    BindColor(TEXT("iskm-proxy-pose-background"), &SCkInspector_IskmProxyAuthored::Get_PoseSourceBackground);
    BindText(TEXT("iskm-proxy-animation"), &SCkInspector_IskmProxyAuthored::Get_PlayingAnimationText);
    BindText(TEXT("iskm-proxy-time"), &SCkInspector_IskmProxyAuthored::Get_PlayTimeText);
    Data.Number.Add(TEXT("iskm-proxy-time-fraction"), TAttribute<float>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() ? W->Get_PlayTimeFraction() : 0.0f; }));
    Data.Color.Add(TEXT("iskm-proxy-time-fill"), TAttribute<FLinearColor>::CreateLambda([]() { return CkStyle::Accent(); }));
    BindText(TEXT("iskm-proxy-anim-instance"), &SCkInspector_IskmProxyAuthored::Get_AnimInstanceText);
    BindText(TEXT("iskm-proxy-montage"), &SCkInspector_IskmProxyAuthored::Get_ActiveMontageText);
    BindText(TEXT("iskm-proxy-ragdoll"), &SCkInspector_IskmProxyAuthored::Get_RagdollingText);
    BindColor(TEXT("iskm-proxy-ragdoll-foreground"), &SCkInspector_IskmProxyAuthored::Get_RagdollingForeground);
    BindColor(TEXT("iskm-proxy-ragdoll-background"), &SCkInspector_IskmProxyAuthored::Get_RagdollingBackground);
    BindText(TEXT("iskm-proxy-submeshes"), &SCkInspector_IskmProxyAuthored::Get_SubmeshesText);
    BindText(TEXT("iskm-proxy-slot-zero"), &SCkInspector_IskmProxyAuthored::Get_CustomDataSlotZeroText);
    BindText(TEXT("iskm-proxy-disabled-reason"), &SCkInspector_IskmProxyAuthored::Get_RequestDisabledReason);
    BindText(TEXT("iskm-proxy-morph-name"), &SCkInspector_IskmProxyAuthored::Get_MorphNameText);
    Data.Text.Add(TEXT("iskm-proxy-stop-animation-label"), FText::FromString(TEXT("Stop Anim")));
    Data.Text.Add(TEXT("iskm-proxy-end-ragdoll-label"), FText::FromString(TEXT("End Ragdoll")));
    Data.Text.Add(TEXT("iskm-proxy-clear-morphs-label"), FText::FromString(TEXT("Clear Morphs")));
    Data.Text.Add(TEXT("iskm-proxy-clear-materials-label"), FText::FromString(TEXT("Clear Materials")));
    Data.Text.Add(TEXT("iskm-proxy-detach-submeshes-label"), FText::FromString(TEXT("Detach Submeshes")));
    Data.Visibility.Add(TEXT("iskm-proxy-available"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("iskm-proxy-unavailable"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return NOT W.IsValid() || NOT W->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("iskm-proxy-can-request"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_CanRequest(); }));
    Data.Visibility.Add(TEXT("iskm-proxy-can-end-ragdoll"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_CanEndRagdoll(); }));
    Data.Visibility.Add(TEXT("iskm-proxy-visibility-intent"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_VisibilityIntent(); }));
    Data.Number.Add(TEXT("iskm-proxy-play-rate"), TAttribute<float>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() ? W->Get_PlayRateIntent() : 0.0f; }));
    Data.Number.Add(TEXT("iskm-proxy-morph-weight"), TAttribute<float>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() ? W->Get_MorphWeight() : 0.0f; }));
    Data.Number.Add(TEXT("iskm-proxy-custom-slot"), TAttribute<float>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() ? W->Get_CustomDataSlot() : 0.0f; }));
    Data.Number.Add(TEXT("iskm-proxy-custom-value"), TAttribute<float>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() ? W->Get_CustomDataValue() : 0.0f; }));
    Data.BoolChanged.Add(TEXT("iskm-proxy-visibility-changed"), FCkUiOnBoolChanged::CreateLambda([Weak](const bool Value) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Set_VisibilityIntent(Value); } }));
    Data.NumberCommitted.Add(TEXT("iskm-proxy-play-rate-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak](const float Value, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Commit_PlayRate(Value); } }));
    Data.NumberCommitted.Add(TEXT("iskm-proxy-morph-weight-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak](const float Value, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Commit_MorphWeight(Value); } }));
    Data.NumberCommitted.Add(TEXT("iskm-proxy-custom-slot-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak](const float Value, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Commit_CustomDataSlot(Value); } }));
    Data.NumberCommitted.Add(TEXT("iskm-proxy-custom-value-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak](const float Value, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Commit_CustomDataValue(Value); } }));
    Data.TextCommitted.Add(TEXT("iskm-proxy-morph-name-committed"), FOnTextCommitted::CreateLambda([Weak](const FText& Value, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Commit_MorphName(Value.ToString()); } }));
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("iskm-proxy-stop-animation"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto W = Weak.Pin(); W.IsValid()) { W->Request_StopAnimation(); } }));
    Actions.Add(TEXT("iskm-proxy-end-ragdoll"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto W = Weak.Pin(); W.IsValid()) { W->Request_EndRagdoll(); } }));
    Actions.Add(TEXT("iskm-proxy-clear-morphs"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto W = Weak.Pin(); W.IsValid()) { W->Request_ClearMorphs(); } }));
    Actions.Add(TEXT("iskm-proxy-clear-materials"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto W = Weak.Pin(); W.IsValid()) { W->Request_ClearMaterials(); } }));
    Actions.Add(TEXT("iskm-proxy-detach-submeshes"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto W = Weak.Pin(); W.IsValid()) { W->Request_DetachSubmeshes(); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_CanRequest(); });

    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("pose"), TEXT("Pose Source:")}, {TEXT("animation"), TEXT("Playing Animation:")}, {TEXT("time"), TEXT("Play Time / Length:")},
        {TEXT("anim-instance"), TEXT("AnimInstance Class:")}, {TEXT("montage"), TEXT("Active Montage:")}, {TEXT("ragdoll"), TEXT("Ragdolling:")},
        {TEXT("submeshes"), TEXT("Attached Submeshes:")}, {TEXT("slot-zero"), TEXT("Custom Data Slot 0:")}, {TEXT("visibility"), TEXT("Visible:")},
        {TEXT("play-rate"), TEXT("Play Rate:")}, {TEXT("morph"), TEXT("Morph Target:")}, {TEXT("weight"), TEXT("  Weight:")},
        {TEXT("custom-slot"), TEXT("Custom Data Slot:")}, {TEXT("custom-value"), TEXT("  Value:")}})
    { Data.Color.Add(TEXT("iskm-proxy-") + Pair.Key + TEXT("-diff-color"), TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]() { const auto W = Weak.Pin(); return W.IsValid() && NOT W->Is_Inert() ? ck_inspector_iskm_proxy::DiffColor(W->Is_DiffMarked(Label)) : FLinearColor::Transparent; })); }

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create({}, MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorIskmProxy.ui.html")), FPaths::Combine(Root, TEXT("EcsInspectorIskmProxy.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded) { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate; _Mounted = true; _LoadError.Reset(); return true;
}

auto SCkInspector_IskmProxyAuthored::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_IskmProxyAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false; _Entity = {}; _DiffLabels.Reset(); _View.Reset(); _Mounted = false;
}

FCkInspector_IskmProxy::~FCkInspector_IskmProxy()
{ OnDeactivated(); }

auto FCkInspector_IskmProxy::Tick(const FCk_Handle&, const float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_IskmProxyAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_IskmProxy::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_IskmProxyAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_IskmProxyAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
