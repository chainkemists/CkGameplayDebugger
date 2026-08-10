#include "SCkDebug_EntityRef.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Entity/CkEntity.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_debug_entityref
{
    // Chip geometry — only HashTintedChip draws anything; every other option keeps the
    // border transparent at zero padding, which is layout-identical to the bare text block.
    constexpr auto ChipPaddingX = CkStyle::SpaceS;
    constexpr auto ChipPaddingY = 1.0f;

    // Same hash -> HSV idiom the overlay uses for provider hues (SCkDebugOverlay_FocusCard::
    // Get_ProviderColor): fixed saturation/value so every entity lands on a readable,
    // mutually-distinct hue that is stable for the life of the id.
    auto Get_HashTint(const FCk_Handle& InEntity) -> FLinearColor
    {
        const auto Id  = static_cast<uint32>(InEntity.Get_Entity().Get_ID());
        const auto Hue = static_cast<uint8>(GetTypeHash(Id) % 256);
        return FLinearColor::MakeFromHSV8(Hue, 150, 205);
    }
}

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

    // The chip frame exists unconditionally and is driven entirely by attributes, so the
    // EntityIdStyle axis applies live without ever rebuilding this widget. Under every
    // option but HashTintedChip it is a transparent, zero-padding no-op.
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush_Small())
        .BorderBackgroundColor(this, &SCkDebug_EntityRef::Get_ChipColor)
        .Padding(this, &SCkDebug_EntityRef::Get_ChipPadding)
        [
            SNew(STextBlock)
            .Text(this, &SCkDebug_EntityRef::Get_DisplayText)
            .Font(MonoFont)
            .ColorAndOpacity(this, &SCkDebug_EntityRef::Get_TextColor)
            .ToolTipText(this, &SCkDebug_EntityRef::Get_Tooltip)
        ]
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

    // ShowName(false) sites deliberately feed an empty name: every EntityIdStyle option
    // degrades to the bare id when there is no name, which is exactly the old behaviour.
    auto CleanName = FString{};

    if (_ShowName)
    {
        const auto Name = UCk_Utils_Handle_UE::Get_DebugName(Entity);
        if (NOT Name.IsNone())
        { CleanName = ck::DebugNameClean::Get_CleanName(Name.ToString()); }
    }

    // Re-read per frame — the axis applies live with no rebuild and no invalidation.
    return ck::debug_axes::Make_EntityIdText(
        UCkDebuggerStyleSettings::Get_Selection(), CleanName, IdText);
}

auto
    SCkDebug_EntityRef::
    Get_TextColor() const
    -> FSlateColor
{
    const auto Entity = _Entity.Get();

    if (ck::Is_NOT_Valid(Entity))
    { return FSlateColor(CkStyle::None()); }

    if (Is_HashTinted())
    { return FSlateColor(ck_debug_entityref::Get_HashTint(Entity)); }

    return FSlateColor(CkStyle::EntityId());
}

auto
    SCkDebug_EntityRef::
    Get_ChipColor() const
    -> FSlateColor
{
    const auto Entity = _Entity.Get();

    if (ck::Is_NOT_Valid(Entity) || NOT Is_HashTinted())
    { return FSlateColor(FLinearColor::Transparent); }

    // Dim fill behind the bright text — a composition choice, never an alpha wrapper
    // around the styled child.
    return FSlateColor(CkStyle::OverlayOf(ck_debug_entityref::Get_HashTint(Entity), 0.18f));
}

auto
    SCkDebug_EntityRef::
    Get_ChipPadding() const
    -> FMargin
{
    if (NOT Is_HashTinted())
    { return FMargin{0.0f}; }

    return FMargin{ck_debug_entityref::ChipPaddingX, ck_debug_entityref::ChipPaddingY};
}

auto
    SCkDebug_EntityRef::
    Is_HashTinted() const
    -> bool
{
    return UCkDebuggerStyleSettings::Get_Selection().EntityIdStyle
        == ECkDebugAxis_EntityIdStyle::HashTintedChip;
}

auto
    SCkDebug_EntityRef::
    Get_TooltipIdPrefix() const
    -> FString
{
    // NameOnly is the only option whose visible text carries no identifier. Rather than leaving the
    // user with a name they cannot resolve, the id moves into the tooltip for exactly that option —
    // every other option already shows it, so their tooltip stays byte-identical.
    if (UCkDebuggerStyleSettings::Get_Selection().EntityIdStyle != ECkDebugAxis_EntityIdStyle::NameOnly)
    { return FString{}; }

    const auto Entity = _Entity.Get();

    if (ck::Is_NOT_Valid(Entity))
    { return FString{}; }

    const auto IdText = ck::Format_UE(TEXT("{}"), Entity.Get_Entity());

    // A nameless entity already renders AS its id under NameOnly — repeating it would be noise.
    return Get_DisplayText().ToString() == IdText ? FString{} : IdText;
}

auto
    SCkDebug_EntityRef::
    Get_Tooltip() const
    -> FText
{
    // A caller-supplied tooltip owns the whole surface, id included.
    if (NOT _CustomTooltip.IsEmpty())
    { return _CustomTooltip; }

    if (ck::Is_NOT_Valid(_Entity.Get()))
    { return FText::FromString(TEXT("Invalid entity")); }

    const auto Base = ck::DebugNav::Has_EntityNavigator()
        ? FString{TEXT("Click to open in CK ECS Debugger — right-click to copy")}
        : FString{TEXT("Entity ID — right-click to copy")};

    const auto IdPrefix = Get_TooltipIdPrefix();

    if (IdPrefix.IsEmpty())
    { return FText::FromString(Base); }

    return FText::FromString(ck::Format_UE(TEXT("{}\n{}"), IdPrefix, Base));
}

auto
    SCkDebug_EntityRef::
    Is_Clickable() const
    -> bool
{
    return ck::IsValid(_Entity.Get()) && ck::DebugNav::Has_EntityNavigator();
}

// ====================================================================================================================
