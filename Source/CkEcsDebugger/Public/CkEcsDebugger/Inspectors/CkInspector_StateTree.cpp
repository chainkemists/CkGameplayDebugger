#include "CkInspector_StateTree.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkStateTree/CkStateTree_Fragment.h"
#include "CkStateTree/CkStateTree_Fragment_Data.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_StateTree)

// =====================================================================================================================

namespace
{
    auto Format_RunStatus_Color(ECk_StateTree_RunStatus Status) -> FLinearColor
    {
        switch (Status)
        {
            case ECk_StateTree_RunStatus::Running:  return CkDebugStyle::Status_Active();
            case ECk_StateTree_RunStatus::Paused:   return CkDebugStyle::Warn();
            case ECk_StateTree_RunStatus::Stopped:  return CkDebugStyle::TextMute();
            default:                                return CkDebugStyle::None();
        }
    }

    auto Format_RunStatus_String(ECk_StateTree_RunStatus Status) -> FString
    {
        switch (Status)
        {
            case ECk_StateTree_RunStatus::Running:  return TEXT("Running");
            case ECk_StateTree_RunStatus::Paused:   return TEXT("Paused");
            case ECk_StateTree_RunStatus::Stopped:  return TEXT("Stopped");
            default:                                return TEXT("Unknown");
        }
    }
}

// =====================================================================================================================

auto FCkInspector_StateTree::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("State Tree"));
}

auto FCkInspector_StateTree::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has<ck::FFragment_StateTree_Current>();
}

// =====================================================================================================================

auto FCkInspector_StateTree::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    const auto& Frag      = Entity.Get<ck::FFragment_StateTree_Current>();
    const auto  Status    = Frag.Get_RunStatus();
    const auto* OwnerPtr  = Frag.Get_Owner().Get();
    const bool  bOwnerValid = ck::IsValid(OwnerPtr, ck::IsValid_Policy_NullptrOnly{});

    Builder.AddHeader(FText::FromString(TEXT("State Tree")));

    Builder.AddRow(
        FText::FromString(TEXT("Status:")),
        [Status](const FCk_Handle&)
        { return FText::FromString(Format_RunStatus_String(Status)); },
        Format_RunStatus_Color(Status));

    Builder.AddRow(
        FText::FromString(TEXT("Owner:")),
        [bOwnerValid](const FCk_Handle&)
        { return FText::FromString(bOwnerValid ? TEXT("Valid") : TEXT("Invalid")); },
        bOwnerValid ? CkDebugStyle::Ok() : CkDebugStyle::Err());

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_StateTree::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
