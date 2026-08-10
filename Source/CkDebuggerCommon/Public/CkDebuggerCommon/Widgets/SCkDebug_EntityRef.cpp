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

#include "Styling/StyleDefaults.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_debug_entityref
{
    // Same hash -> HSV idiom the overlay uses for provider hues (SCkDebugOverlay_FocusCard::
    // Get_ProviderColor): fixed saturation/value so every entity lands on a readable,
    // mutually-distinct hue that is stable for the life of the id.
    auto Get_HashTint(uint32 InSeed) -> FLinearColor
    {
        const auto Hue = static_cast<uint8>(GetTypeHash(InSeed) % 256);
        return FLinearColor::MakeFromHSV8(Hue, 150, 205);
    }

    // The label is ONE text block under every treatment, so Monochrome cannot put the id alone in a
    // monospace face without splitting the tree the other three share. It takes the mono face for
    // the whole composition instead — which is what the option is for, the id being the bulk of it.
    auto Get_TreatmentFont() -> FSlateFontInfo
    {
        const auto Face = UCkDebuggerStyleSettings::Get_Selection().EntityRefStyle
            == ECkDebugAxis_EntityRefStyle::Monochrome
                ? "Mono"
                : "Bold";

        return ck::debug_axes::ScaledFont(Face, CkStyle::FontSizeSmall());
    }
}

// ====================================================================================================================

auto
    SCkDebug_EntityRef::
    Construct(const FArguments& InArgs)
    -> void
{
    _Entity         = InArgs._Entity;
    _Font           = InArgs._Font;
    _ShowName       = InArgs._ShowName;
    _CustomTooltip  = InArgs._Tooltip;
    _PreviewName    = InArgs._PreviewName;
    _PreviewIdText  = InArgs._PreviewIdText;

    // The chip frame exists unconditionally and every one of its visuals — brush included — is an
    // attribute, so both axes apply live without ever rebuilding this widget. Flat and Monochrome
    // resolve to a no-brush, zero-padding shell, which is layout-identical to the bare text block
    // this widget shipped as.
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(this, &SCkDebug_EntityRef::Get_ChipBrush)
        .BorderBackgroundColor(this, &SCkDebug_EntityRef::Get_ChipColor)
        .Padding(this, &SCkDebug_EntityRef::Get_ChipPadding)
        [
            SNew(STextBlock)
            .Text(this, &SCkDebug_EntityRef::Get_DisplayText)
            .Font(this, &SCkDebug_EntityRef::Get_Font)
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
    if (NOT Has_Subject())
    { return FText::FromString(TEXT("None")); }

    // ShowName(false) sites deliberately feed an empty name: every EntityIdStyle option
    // degrades to the bare id when there is no name, which is exactly the old behaviour.
    auto CleanName = FString{};

    if (Is_Preview())
    {
        CleanName = _PreviewName;
    }
    else if (_ShowName)
    {
        const auto Name = UCk_Utils_Handle_UE::Get_DebugName(_Entity.Get());
        if (NOT Name.IsNone())
        { CleanName = ck::DebugNameClean::Get_CleanName(Name.ToString()); }
    }

    // Re-read per frame — the axis applies live with no rebuild and no invalidation.
    return ck::debug_axes::Make_EntityIdText(
        UCkDebuggerStyleSettings::Get_Selection(), CleanName, Get_IdText());
}

auto
    SCkDebug_EntityRef::
    Get_TextColor() const
    -> FSlateColor
{
    if (NOT Has_Subject())
    { return FSlateColor(CkStyle::None()); }

    const auto RestInk = ck::debug_axes::Get_EntityRefInk(Get_Accent());

    // The lift is the affordance that says this reference is a link. A reference that cannot
    // navigate — invalid handle, no registered navigator, a Lab preview — never moves under the
    // cursor, so hover never promises something the click will not deliver.
    if (Is_Clickable() && IsHovered())
    { return FSlateColor(ck::debug_axes::Get_EntityRefHoverInk(RestInk)); }

    return FSlateColor(RestInk);
}

auto
    SCkDebug_EntityRef::
    Get_ChipColor() const
    -> FSlateColor
{
    if (NOT Has_Subject())
    { return FSlateColor(FLinearColor::Transparent); }

    // One tint, whatever the treatment's brush paints with it: the body under Pill, the ring under
    // OutlinePill, nothing under Flat and Monochrome (whose fill resolves transparent). Always a
    // composition choice, never an alpha wrapper around the styled child.
    return FSlateColor(ck::debug_axes::Get_EntityRefFill(Get_Accent()));
}

auto
    SCkDebug_EntityRef::
    Get_ChipPadding() const
    -> FMargin
{
    if (NOT Has_Subject())
    { return FMargin{0.0f}; }

    return ck::debug_axes::Get_EntityRefPadding();
}

auto
    SCkDebug_EntityRef::
    Get_ChipBrush() const
    -> const FSlateBrush*
{
    if (NOT Has_Subject())
    { return FStyleDefaults::GetNoBrush(); }

    return ck::debug_axes::Get_EntityRefBrush();
}

auto
    SCkDebug_EntityRef::
    Get_Font() const
    -> FSlateFontInfo
{
    // A caller-supplied font owns the face AND the size: the overlay focus card sizes its pill
    // through its own FontScale, and scaling it a second time here would compound the two.
    if (_Font.IsSet())
    { return _Font.Get(); }

    return ck_debug_entityref::Get_TreatmentFont();
}

auto
    SCkDebug_EntityRef::
    Get_Accent() const
    -> FLinearColor
{
    if (NOT ck::debug_axes::EntityRef_UsesHashTint())
    { return CkStyle::EntityId(); }

    return ck_debug_entityref::Get_HashTint(Is_Preview()
        ? GetTypeHash(_PreviewIdText)
        : static_cast<uint32>(_Entity.Get().Get_Entity().Get_ID()));
}

auto
    SCkDebug_EntityRef::
    Get_IdText() const
    -> FString
{
    if (Is_Preview())
    { return _PreviewIdText; }

    if (ck::Is_NOT_Valid(_Entity.Get()))
    { return FString{}; }

    return ck::Format_UE(TEXT("{}"), _Entity.Get().Get_Entity());
}

auto
    SCkDebug_EntityRef::
    Is_Preview() const
    -> bool
{
    return NOT _PreviewIdText.IsEmpty();
}

auto
    SCkDebug_EntityRef::
    Has_Subject() const
    -> bool
{
    return Is_Preview() || ck::IsValid(_Entity.Get());
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

    if (NOT Has_Subject())
    { return FString{}; }

    const auto IdText = Get_IdText();

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

    if (NOT Has_Subject())
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
