#include "CkInspector_UI.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkUI/WorldSpaceWidget/CkWorldSpaceWidget_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_UI)

// =====================================================================================================================

auto FCkInspector_UI::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("UI"));
}

auto FCkInspector_UI::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has<ck::FFragment_WorldSpaceWidget_Current>();
}

// =====================================================================================================================

auto FCkInspector_UI::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    const auto& Frag         = Entity.Get<ck::FFragment_WorldSpaceWidget_Current>();
    const auto* WrapperPtr   = Frag.Get_WrapperWidget().Get();
    const bool  bWrapperValid = ck::IsValid(WrapperPtr, ck::IsValid_Policy_NullptrOnly{});
    const bool  bPlayerValid  = Frag.Get_WidgetOwningPlayer().IsValid();

    Builder.AddHeader(FText::FromString(TEXT("World Space Widget")));

    Builder.AddRow(
        FText::FromString(TEXT("Wrapper Widget:")),
        [bWrapperValid](const FCk_Handle&)
        { return FText::FromString(bWrapperValid ? TEXT("Valid") : TEXT("Invalid")); },
        bWrapperValid ? CkStyle::Ok() : CkStyle::Err());

    Builder.AddRow(
        FText::FromString(TEXT("Owning Player:")),
        [bPlayerValid](const FCk_Handle&)
        { return FText::FromString(bPlayerValid ? TEXT("Valid") : TEXT("Invalid")); },
        bPlayerValid ? CkStyle::Ok() : CkStyle::TextMute());

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_UI::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
