#include "CkInspector_IskmProxy.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Utils.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Fragment_Data.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IskmProxy)

// =====================================================================================================================

namespace ck_inspector_iskm_proxy
{
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
    return ck::IsValid(Entity) && UCk_Utils_IskmProxy_UE::Has(Entity);
}

auto FCkInspector_IskmProxy::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildIskmProxyGrid(Entity);
}

// =====================================================================================================================

auto FCkInspector_IskmProxy::BuildIskmProxyGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto MutableEntity = Entity;
    const auto ProxyHandle = UCk_Utils_IskmProxy_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(ProxyHandle))
    {
        return Builder.Build(Entity, FString());
    }

    const auto CapturedProxy = ProxyHandle;

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
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            return ck_inspector_iskm_proxy::Get_PoseSourceText(UCk_Utils_IskmProxy_UE::Get_PoseSource(CapturedProxy));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedProxy]()
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return ECk_Tone::Neutral; }
            return ck_inspector_iskm_proxy::Get_PoseSourceTone(UCk_Utils_IskmProxy_UE::Get_PoseSource(CapturedProxy));
        }));

    // Playing Animation
    Builder.AddRow(
        FText::FromString(TEXT("Playing Animation:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto* Sequence = UCk_Utils_IskmProxy_UE::Get_PlayingAnimation(CapturedProxy);
            if (NOT ck::IsValid(Sequence, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("None")); }
            return FText::FromString(Sequence->GetName());
        },
        CkStyle::Value_Object());

    Builder.AddMeterRow(
        FText::FromString(TEXT("Play Time / Length:")),
        TAttribute<float>::CreateLambda([CapturedProxy]()
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return 0.0f; }
            const auto Length = UCk_Utils_IskmProxy_UE::Get_PlayLength(CapturedProxy);
            if (Length <= 0.0f) { return 0.0f; }
            return UCk_Utils_IskmProxy_UE::Get_PlayTime(CapturedProxy) / Length;
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedProxy]()
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Time   = UCk_Utils_IskmProxy_UE::Get_PlayTime(CapturedProxy);
            const auto Length = UCk_Utils_IskmProxy_UE::Get_PlayLength(CapturedProxy);
            return FText::FromString(ck::Format_UE(TEXT("{:.2f} / {:.2f}s"), Time, Length));
        }));

    // AnimInstance Class
    Builder.AddRow(
        FText::FromString(TEXT("AnimInstance Class:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
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
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto* Montage = UCk_Utils_IskmProxy_UE::Get_ActiveMontage(CapturedProxy);
            if (NOT ck::IsValid(Montage, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("None")); }
            return FText::FromString(Montage->GetName());
        },
        CkStyle::Value_Object());

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Ragdolling:")),
        TAttribute<FText>::CreateLambda([CapturedProxy]()
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(UCk_Utils_IskmProxy_UE::Get_IsRagdolling(CapturedProxy)
                ? TEXT("Yes")
                : TEXT("No"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedProxy]()
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return ECk_Tone::Neutral; }
            return UCk_Utils_IskmProxy_UE::Get_IsRagdolling(CapturedProxy)
                ? ECk_Tone::Warn
                : ECk_Tone::Neutral;
        }));

    // Attached Submeshes
    Builder.AddRow(
        FText::FromString(TEXT("Attached Submeshes:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Count = UCk_Utils_IskmProxy_UE::Get_NumAttachedSubmeshes(CapturedProxy);
            return FText::FromString(FString::FromInt(Count));
        },
        CkStyle::Value_Numeric());

    // Custom Data Slot 0
    Builder.AddRow(
        FText::FromString(TEXT("Custom Data Slot 0:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
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
        [CapturedProxy, VisibilityIntent](bool InIsVisible)
        {
            *VisibilityIntent = InIsVisible;

            auto Mutable = CapturedProxy;
            if (ck::Is_NOT_Valid(Mutable)) { return; }

            UCk_Utils_IskmProxy_UE::Request_SetVisibility(Mutable, InIsVisible, {});
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Play Rate:")),
        TAttribute<float>::CreateLambda([PlayRateIntent]() { return *PlayRateIntent; }),
        [CapturedProxy, PlayRateIntent](float InRate)
        {
            *PlayRateIntent = InRate;

            auto Mutable = CapturedProxy;
            if (ck::Is_NOT_Valid(Mutable)) { return; }

            UCk_Utils_IskmProxy_UE::Request_SetPlayRate(Mutable, InRate, {});
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
                [CapturedProxy]()
                {
                    auto Mutable = CapturedProxy;
                    if (ck::Is_NOT_Valid(Mutable)) { return; }
                    UCk_Utils_IskmProxy_UE::Request_StopAnimation(Mutable, FCk_Request_IskmProxy_StopAnimation{}, {});
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("End Ragdoll")),
                FText::FromString(TEXT("Request_EndRagdoll — returns the proxy from physics to its authored pose source.")),
                [CapturedProxy]()
                {
                    auto Mutable = CapturedProxy;
                    if (ck::Is_NOT_Valid(Mutable)) { return; }
                    UCk_Utils_IskmProxy_UE::Request_EndRagdoll(Mutable, FCk_Request_IskmProxy_EndRagdoll{}, {});
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Clear Morphs")),
                FText::FromString(TEXT("Request_ClearMorphTargets — drops every recorded morph weight on this proxy.")),
                [CapturedProxy]()
                {
                    auto Mutable = CapturedProxy;
                    if (ck::Is_NOT_Valid(Mutable)) { return; }
                    UCk_Utils_IskmProxy_UE::Request_ClearMorphTargets(Mutable, {});
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Clear Materials")),
                FText::FromString(TEXT("Request_ClearMaterialOverrides — restores the mesh's default material on every slot.")),
                [CapturedProxy]()
                {
                    auto Mutable = CapturedProxy;
                    if (ck::Is_NOT_Valid(Mutable)) { return; }
                    UCk_Utils_IskmProxy_UE::Request_ClearMaterialOverrides(Mutable, {});
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Detach Submeshes")),
                FText::FromString(TEXT("Request_DetachAllSubmeshes — strips every attached outfit submesh.")),
                [CapturedProxy]()
                {
                    auto Mutable = CapturedProxy;
                    if (ck::Is_NOT_Valid(Mutable)) { return; }
                    UCk_Utils_IskmProxy_UE::Request_DetachAllSubmeshes(Mutable, {});
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
        [MorphName](FName InName) { *MorphName = InName; },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("  Weight:")),
        TAttribute<float>::CreateLambda([CapturedProxy, MorphName]()
        {
            if (ck::Is_NOT_Valid(CapturedProxy) || MorphName->IsNone()) { return 0.0f; }
            return UCk_Utils_IskmProxy_UE::Get_MorphTarget(CapturedProxy, *MorphName);
        }),
        [CapturedProxy, MorphName](float InValue)
        {
            auto Mutable = CapturedProxy;
            if (ck::Is_NOT_Valid(Mutable) || MorphName->IsNone()) { return; }

            UCk_Utils_IskmProxy_UE::Request_SetMorphTarget(Mutable, *MorphName, InValue, {});
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    // ---- Custom data: slot selector + the value recorded in it ----

    Builder.AddIntegerRow(
        FText::FromString(TEXT("Custom Data Slot:")),
        TAttribute<int32>::CreateLambda([CustomDataSlot]() { return *CustomDataSlot; }),
        [CustomDataSlot](int32 InSlot) { *CustomDataSlot = InSlot; },
        0,
        TOptional<int32>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("  Value:")),
        TAttribute<float>::CreateLambda([CapturedProxy, CustomDataSlot]()
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return 0.0f; }
            return UCk_Utils_IskmProxy_UE::Get_CustomDataFloat(CapturedProxy, *CustomDataSlot);
        }),
        [CapturedProxy, CustomDataSlot](float InValue)
        {
            auto Mutable = CapturedProxy;
            if (ck::Is_NOT_Valid(Mutable)) { return; }

            UCk_Utils_IskmProxy_UE::Request_SetCustomDataFloat(Mutable, *CustomDataSlot, InValue, {});
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_IskmProxy::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
