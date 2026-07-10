#include "SCkDebug_EntityRef.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Entity/CkEntity.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
    SCkDebug_EntityRef::
    Construct(const FArguments& InArgs)
    -> void
{
    _Entity         = InArgs._Entity;
    _ShowName       = InArgs._ShowName;
    _CustomTooltip  = InArgs._Tooltip;

    const auto MonoFont = InArgs._Font.IsSet()
        ? InArgs._Font
        : TAttribute<FSlateFontInfo>(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeSmall()));

    ChildSlot
    [
        SNew(STextBlock)
        .Text(this, &SCkDebug_EntityRef::Get_DisplayText)
        .Font(MonoFont)
        .ColorAndOpacity(this, &SCkDebug_EntityRef::Get_TextColor)
        .ToolTipText(this, &SCkDebug_EntityRef::Get_Tooltip)
    ];
}

// ----------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_EntityRef::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    const auto Entity = _Entity.Get();

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        // Always allow Copy of whatever is rendered, even for invalid handles —
        // copying "TOMBSTONE" or an empty string is harmless and the user may
        // want to grab it for a bug report.
        const auto Text = Get_DisplayText().ToString();
        return ck::DebugCopyMenu::Handle_RightClickToCopy(SharedThis(this), InMouseEvent, Text);
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Is_Clickable())
    {
        ck::DebugNav::Goto_Entity(Entity);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto
    SCkDebug_EntityRef::
    OnCursorQuery(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) const
    -> FCursorReply
{
    if (Is_Clickable())
    { return FCursorReply::Cursor(EMouseCursor::Hand); }

    return FCursorReply::Unhandled();
}

// ----------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_EntityRef::
    Get_DisplayText() const
    -> FText
{
    const auto Entity = _Entity.Get();

    if (ck::Is_NOT_Valid(Entity))
    { return FText::FromString(TEXT("None")); }

    const auto IdText = ck::Format_UE(TEXT("{}"), Entity.Get_Entity());

    if (_ShowName)
    {
        const auto Name = UCk_Utils_Handle_UE::Get_DebugName(Entity);
        if (NOT Name.IsNone())
        {
            const auto CleanName = ck::DebugNameClean::Get_CleanName(Name.ToString());
            return FText::FromString(ck::Format_UE(TEXT("{} | {}"), CleanName, IdText));
        }
    }

    return FText::FromString(IdText);
}

auto
    SCkDebug_EntityRef::
    Get_TextColor() const
    -> FSlateColor
{
    if (ck::Is_NOT_Valid(_Entity.Get()))
    { return FSlateColor(CkStyle::None()); }

    return FSlateColor(CkStyle::EntityId());
}

auto
    SCkDebug_EntityRef::
    Get_Tooltip() const
    -> FText
{
    if (NOT _CustomTooltip.IsEmpty())
    { return _CustomTooltip; }

    if (ck::Is_NOT_Valid(_Entity.Get()))
    { return FText::FromString(TEXT("Invalid entity")); }

    if (NOT ck::DebugNav::Has_EntityNavigator())
    { return FText::FromString(TEXT("Entity ID — right-click to copy")); }

    return FText::FromString(TEXT("Click to open in CK ECS Debugger — right-click to copy"));
}

auto
    SCkDebug_EntityRef::
    Is_Clickable() const
    -> bool
{
    return ck::IsValid(_Entity.Get()) && ck::DebugNav::Has_EntityNavigator();
}

// ====================================================================================================================
