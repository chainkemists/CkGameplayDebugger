#include "SCkDebug_IconToggle.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SCkDebug_IconToggle"

// ====================================================================================================================

FCkDebug_IconToggleAction::FCkDebug_IconToggleAction(
    FName InId,
    FName InIconId,
    FText InLabel,
    FText InToolTip,
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
    FCkDebug_IconToolbarPartition::
    TryBuild(
        const TArray<FCkDebug_IconToggleAction>& InActions,
        int32 InDirectLimit,
        FCkDebug_IconToolbarPartition& OutPartition)
    -> bool
{
    OutPartition = {};

    if (InDirectLimit <= 0)
    { return false; }

    auto SeenIds = TSet<FName>{};
    SeenIds.Reserve(InActions.Num());

    for (const auto& Action : InActions)
    {
        if (NOT Action.IsValid() || SeenIds.Contains(Action.Id))
        { return false; }

        SeenIds.Add(Action.Id);
    }

    OutPartition.DirectCount = FMath::Min(InActions.Num(), InDirectLimit);
    OutPartition.OverflowCount = InActions.Num() - OutPartition.DirectCount;
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
        .WidthOverride(16.0f)
        .HeightOverride(16.0f)
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
    if (_ToolTip.IsEmpty())
    {
        return FText::Format(LOCTEXT("LabelAndState", "{0}\nState: {1}"), _Label, State);
    }

    return FText::Format(LOCTEXT("TooltipAndState", "{0}\n{1}\nState: {2}"), _Label, _ToolTip, State);
}

// ====================================================================================================================

auto
    SCkDebug_IconToolbar::
    Construct(const FArguments& InArgs)
    -> void
{
    auto WidePartition = FCkDebug_IconToolbarPartition{};
    auto CompactPartition = FCkDebug_IconToolbarPartition{};
    const auto WideIsValid = FCkDebug_IconToolbarPartition::TryBuild(
        InArgs._Actions, InArgs._WideDirectCount, WidePartition);
    const auto CompactIsValid = FCkDebug_IconToolbarPartition::TryBuild(
        InArgs._Actions, InArgs._CompactDirectCount, CompactPartition);
    const auto LayoutIsValid = WideIsValid
        && CompactIsValid
        && InArgs._CompactDirectCount <= InArgs._WideDirectCount
        && InArgs._CompactWidthThreshold > 0.0f;

    CK_ENSURE_IF_NOT(LayoutIsValid, TEXT("Invalid common debugger icon toolbar configuration"))
    {}
    if (NOT LayoutIsValid)
    {
        ChildSlot[SNullWidget::NullWidget];
        return;
    }

    _Actions = InArgs._Actions;
    _CompactWidthThreshold = InArgs._CompactWidthThreshold;

    ChildSlot
    [
        SAssignNew(_LayoutSwitcher, SWidgetSwitcher)
        .WidgetIndex(0)

        + SWidgetSwitcher::Slot()
        [
            Build_Row(WidePartition.DirectCount)
        ]

        + SWidgetSwitcher::Slot()
        [
            Build_Row(CompactPartition.DirectCount)
        ]
    ];
}

auto
    SCkDebug_IconToolbar::
    Tick(
        const FGeometry& InAllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    const auto IsCompactNow = InAllottedGeometry.GetLocalSize().X < _CompactWidthThreshold;
    if (IsCompactNow != _IsCompact && _LayoutSwitcher.IsValid())
    {
        _IsCompact = IsCompactNow;
        _LayoutSwitcher->SetActiveWidgetIndex(_IsCompact ? 1 : 0);
    }
}

auto
    SCkDebug_IconToolbar::
    Build_Row(int32 InDirectCount)
    -> TSharedRef<SWidget>
{
    auto Row = SNew(SHorizontalBox);
    for (auto Index = 0; Index < InDirectCount; ++Index)
    {
        Row->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
        [
            Build_Action(_Actions[Index], false)
        ];
    }

    if (InDirectCount < _Actions.Num())
    {
        Row->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SComboButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin{CkStyle::SpaceS, 4.0f})
            .HasDownArrow(false)
            .ToolTipText(LOCTEXT("MoreTooltip", "More display options"))
            .OnGetMenuContent_Lambda([this, InDirectCount]()
            {
                return Build_Overflow(InDirectCount);
            })
            .ButtonContent()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("MoreGlyph", "•••"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            ]
        ];
    }

    return Row;
}

auto
    SCkDebug_IconToolbar::
    Build_Overflow(int32 InDirectCount)
    -> TSharedRef<SWidget>
{
    auto Column = SNew(SVerticalBox);
    for (auto Index = InDirectCount; Index < _Actions.Num(); ++Index)
    {
        Column->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [
            Build_Action(_Actions[Index], true)
        ];
    }

    return SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(CkStyle::Bg1())
        .Padding(CkStyle::SpaceS)
        [
            SNew(SBox)
            .MinDesiredWidth(220.0f)
            [
                Column
            ]
        ];
}

auto
    SCkDebug_IconToolbar::
    Build_Action(const FCkDebug_IconToggleAction& InAction, bool InShowLabel) const
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_IconToggle)
        .IconId(InAction.IconId)
        .Label(InAction.Label)
        .ToolTip(InAction.ToolTip)
        .IsOn(InAction.IsOn)
        .OnStateChanged(InAction.OnStateChanged)
        .IsEnabled(InAction.IsEnabled)
        .ShowLabel(InShowLabel);
}

// ====================================================================================================================

#undef LOCTEXT_NAMESPACE
