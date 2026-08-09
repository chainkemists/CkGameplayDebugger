#include "CkInspector_UI.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkWorldSpaceWidget/CkWorldSpaceWidget_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_UI)

// =====================================================================================================================

namespace ck_inspector_ui
{
    // Live reads: the row attributes outlive the fragment, so every read re-validates the handle first.
    static auto Get_IsWrapperValid(
        const FCk_Handle& InEntity)
        -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Current>())
        { return false; }

        const auto* WrapperPtr = InEntity.Get<ck::FFragment_WorldSpaceWidget_Current>().Get_WrapperWidget().Get();
        return ck::IsValid(WrapperPtr, ck::IsValid_Policy_NullptrOnly{});
    }

    static auto Get_IsOwningPlayerValid(
        const FCk_Handle& InEntity)
        -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Current>())
        { return false; }

        return InEntity.Get<ck::FFragment_WorldSpaceWidget_Current>().Get_WidgetOwningPlayer().IsValid();
    }
}

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

    const auto CapturedEntity = Entity;

    Builder.AddHeader(FText::FromString(TEXT("World Space Widget")));

    // Both were snapshotted at compose time before, so a widget that went away mid-session kept reading "Valid"
    // until the panel happened to rebuild. Pills read the fragment live instead.
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Wrapper Widget:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        { return FText::FromString(ck_inspector_ui::Get_IsWrapperValid(CapturedEntity) ? TEXT("Valid") : TEXT("Invalid")); }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_IsWrapperValid(CapturedEntity) ? ECk_Tone::Ok : ECk_Tone::Err; }));

    // An absent owning player is normal for a server-side or shared widget, so it reads Neutral rather than Err.
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Owning Player:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        { return FText::FromString(ck_inspector_ui::Get_IsOwningPlayerValid(CapturedEntity) ? TEXT("Valid") : TEXT("Invalid")); }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_IsOwningPlayerValid(CapturedEntity) ? ECk_Tone::Ok : ECk_Tone::Neutral; }));

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_UI::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
