#include "CkInspector_IskmProxy.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Utils.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Fragment_Data.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IskmProxy)

static const FLinearColor Color_PoseSource = FLinearColor(0.55f, 0.78f, 0.95f);
static const FLinearColor Color_Anim       = FLinearColor(0.85f, 0.75f, 0.55f);
static const FLinearColor Color_State      = FLinearColor(0.95f, 0.65f, 0.65f);
static const FLinearColor Color_Data       = FLinearColor(0.65f, 0.90f, 0.65f);

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

    // Pose Source
    Builder.AddRow(
        FText::FromString(TEXT("Pose Source:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto PoseSource = UCk_Utils_IskmProxy_UE::Get_PoseSource(CapturedProxy);
            switch (PoseSource)
            {
                case ECk_IskmProxy_PoseSource::Sequence: return FText::FromString(TEXT("Sequence"));
                case ECk_IskmProxy_PoseSource::AnimBP:   return FText::FromString(TEXT("AnimBP"));
                case ECk_IskmProxy_PoseSource::Ragdoll:  return FText::FromString(TEXT("Ragdoll"));
                default:                                 return FText::FromString(TEXT("Unknown"));
            }
        },
        Color_PoseSource);

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
        Color_Anim);

    // Play Time / Length
    Builder.AddRow(
        FText::FromString(TEXT("Play Time / Length:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Time   = UCk_Utils_IskmProxy_UE::Get_PlayTime(CapturedProxy);
            const auto Length = UCk_Utils_IskmProxy_UE::Get_PlayLength(CapturedProxy);
            return FText::FromString(ck::Format_UE(TEXT("{:.2f} / {:.2f}s"), Time, Length));
        },
        Color_Anim);

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
        Color_PoseSource);

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
        Color_Anim);

    // Ragdolling
    Builder.AddRow(
        FText::FromString(TEXT("Ragdolling:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto IsRagdolling = UCk_Utils_IskmProxy_UE::Get_IsRagdolling(CapturedProxy);
            return FText::FromString(IsRagdolling ? TEXT("Yes") : TEXT("No"));
        },
        Color_State);

    // Attached Submeshes
    Builder.AddRow(
        FText::FromString(TEXT("Attached Submeshes:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Count = UCk_Utils_IskmProxy_UE::Get_NumAttachedSubmeshes(CapturedProxy);
            return FText::FromString(FString::FromInt(Count));
        },
        Color_Data);

    // Custom Data Slot 0
    Builder.AddRow(
        FText::FromString(TEXT("Custom Data Slot 0:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Value = UCk_Utils_IskmProxy_UE::Get_CustomDataFloat(CapturedProxy, 0);
            return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Value));
        },
        Color_Data);

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_IskmProxy::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
