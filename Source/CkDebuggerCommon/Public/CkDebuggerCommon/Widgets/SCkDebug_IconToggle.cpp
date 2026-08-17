#include "SCkDebug_IconToggle.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCkDebug_IconToggle"

// ====================================================================================================================

namespace ck_debug_icon_toggle
{
    // The toolbar glyph box. 16 is the size this control has always drawn, and it is also the
    // IconSize axis' own default, so Apply_IconSize is a zero delta under Classic. Bound as an
    // attribute so a Style Lab flip resizes toolbars that are already on screen.
    constexpr auto GlyphSizeBase = 16.0f;

    auto Get_GlyphSize() -> FOptionalSize
    {
        return FOptionalSize{ck::debug_axes::Apply_IconSize(GlyphSizeBase)};
    }
}

// ====================================================================================================================

FCkDebug_IconToggleAction::FCkDebug_IconToggleAction(
    FName InId,
    FName InIconId,
    FText InLabel,
    TAttribute<FText> InToolTip,
    TAttribute<bool> InIsOn,
    FOnCkDebug_IconToggleChanged InOnStateChanged,
    TAttribute<bool> InIsEnabled)
    : Id(InId)
    , IconId(InIconId)
    , Label(MoveTemp(InLabel))
    , ToolTip(MoveTemp(InToolTip))
    , IsOn(MoveTemp(InIsOn))
    , OnStateChanged(MoveTemp(InOnStateChanged))
    , IsEnabled(MoveTemp(InIsEnabled))
{
}

auto
    FCkDebug_IconToggleAction::
    IsValid() const
    -> bool
{
    return NOT Id.IsNone()
        && NOT IconId.IsNone()
        && FCkDebuggerCommonStyle::Get_IconBrush(IconId) != nullptr
        && NOT Label.IsEmpty()
        && IsOn.IsSet()
        && OnStateChanged.IsBound()
        && IsEnabled.IsSet();
}

auto
    FCkDebug_IconToolbarLayout::
    TryBuild(
        const TArray<FCkDebug_IconToggleAction>& InActions,
        FCkDebug_IconToolbarLayout& OutLayout)
    -> bool
{
    OutLayout = {};

    auto SeenIds = TSet<FName>{};
    SeenIds.Reserve(InActions.Num());

    for (const auto& Action : InActions)
    {
        if (NOT Action.IsValid() || SeenIds.Contains(Action.Id))
        { return false; }

        SeenIds.Add(Action.Id);
    }

    OutLayout.ActionCount = InActions.Num();
    return true;
}

// ====================================================================================================================

auto
    SCkDebug_IconToggle::
    Construct(const FArguments& InArgs)
    -> void
{
    _Label = InArgs._Label;
    _ToolTip = InArgs._ToolTip;
    _IsOn = InArgs._IsOn;

    const auto* IconBrush = FCkDebuggerCommonStyle::Get_IconBrush(InArgs._IconId);
    const auto IconIsValid = IconBrush != nullptr;
    CK_ENSURE_IF_NOT(
        IconIsValid,
        TEXT("Missing common debugger icon [{}] for toggle [{}]"),
        InArgs._IconId,
        InArgs._Label.ToString())
    {}
    if (NOT IconIsValid)
    {
        ChildSlot[SNullWidget::NullWidget];
        return;
    }

    auto Icon = SNew(SBox)
        .WidthOverride_Static(&ck_debug_icon_toggle::Get_GlyphSize)
        .HeightOverride_Static(&ck_debug_icon_toggle::Get_GlyphSize)
        [
            SNew(SImage)
            .Image(IconBrush)
            .ColorAndOpacity(FSlateColor::UseForeground())
        ];

    TSharedRef<SWidget> Content = Icon;
    if (InArgs._ShowLabel)
    {
        Content = SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                Icon
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(InArgs._Label)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ];
    }

    ChildSlot
    .HAlign(HAlign_Left)
    .VAlign(VAlign_Center)
    [
        SNew(SCheckBox)
        .Style(&FCkDebuggerCommonStyle::Get_IconToggleStyle())
        .IsChecked_Lambda([IsOn = _IsOn]()
        {
            return IsOn.Get(false) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([OnStateChanged = InArgs._OnStateChanged](ECheckBoxState InState)
        {
            if (OnStateChanged.IsBound())
            { OnStateChanged.Execute(InState == ECheckBoxState::Checked); }
        })
        .IsEnabled(InArgs._IsEnabled)
        .AccessibleText(InArgs._Label)
        .ToolTipText(this, &SCkDebug_IconToggle::Get_ToolTipText)
        [
            Content
        ]
    ];
}

auto
    SCkDebug_IconToggle::
    Get_ToolTipText() const
    -> FText
{
    const auto State = _IsOn.Get(false) ? LOCTEXT("StateOn", "On") : LOCTEXT("StateOff", "Off");

    // Resolved on every call, which is the whole point of the attribute: this function is already bound as the
    // widget's live tooltip delegate, so a caller that binds a lambda here gets a number that tracks the thing it
    // describes instead of the value it had when the toolbar was built.
    const auto ToolTip = _ToolTip.Get(FText::GetEmpty());

    if (ToolTip.IsEmpty())
    {
        return FText::Format(LOCTEXT("LabelAndState", "{0}\nState: {1}"), _Label, State);
    }

    return FText::Format(LOCTEXT("TooltipAndState", "{0}\n{1}\nState: {2}"), _Label, ToolTip, State);
}

// ====================================================================================================================

auto
    SCkDebug_IconToolbar::
    Construct(const FArguments& InArgs)
    -> void
{
    auto Layout = FCkDebug_IconToolbarLayout{};
    const auto LayoutIsValid = FCkDebug_IconToolbarLayout::TryBuild(InArgs._Actions, Layout);

    CK_ENSURE_IF_NOT(LayoutIsValid, TEXT("Invalid common debugger icon toolbar configuration"))
    {}
    if (NOT LayoutIsValid)
    {
        ChildSlot[SNullWidget::NullWidget];
        return;
    }

    _Actions = InArgs._Actions;

    ChildSlot
    [
        Build_Row()
    ];
}

auto
    SCkDebug_IconToolbar::
    Build_Row()
    -> TSharedRef<SWidget>
{
    auto Row = SNew(SHorizontalBox);
    for (const auto& Action : _Actions)
    {
        Row->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
        [
            Build_Action(Action)
        ];
    }

    return Row;
}

auto
    SCkDebug_IconToolbar::
    Build_Action(const FCkDebug_IconToggleAction& InAction) const
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_IconToggle)
        .IconId(InAction.IconId)
        .Label(InAction.Label)
        .ToolTip(InAction.ToolTip)
        .IsOn(InAction.IsOn)
        .OnStateChanged(InAction.OnStateChanged)
        .IsEnabled(InAction.IsEnabled)
        .ShowLabel(false);
}

// ====================================================================================================================

#undef LOCTEXT_NAMESPACE
