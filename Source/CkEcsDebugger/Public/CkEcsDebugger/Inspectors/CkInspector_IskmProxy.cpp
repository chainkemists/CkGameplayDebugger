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

    auto MutableEntity = Entity;
    const auto ProxyHandle = UCk_Utils_IskmProxy_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(ProxyHandle))
    {
        return Builder.Build(Entity, FString());
    }

    const auto CapturedProxy = ProxyHandle;

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

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_IskmProxy::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
