#include "SCkDebug_CommandBar.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

// ====================================================================================================================

FCkDebug_CommandGroup::FCkDebug_CommandGroup(
    FName InId,
    FText InAccessibleLabel,
    ECkDebug_CommandBarLane InLane,
    TSharedRef<SWidget> InContent)
    : Id(InId)
    , AccessibleLabel(MoveTemp(InAccessibleLabel))
    , Lane(InLane)
    , Content(InContent)
{
}

auto
    FCkDebug_CommandGroup::
    Primary(FName InId, FText InAccessibleLabel, TSharedRef<SWidget> InContent)
    -> FCkDebug_CommandGroup
{
    return FCkDebug_CommandGroup{
        InId,
        MoveTemp(InAccessibleLabel),
        ECkDebug_CommandBarLane::Primary,
        MoveTemp(InContent)};
}

auto
    FCkDebug_CommandGroup::
    Context(FName InId, FText InAccessibleLabel, TSharedRef<SWidget> InContent)
    -> FCkDebug_CommandGroup
{
    return FCkDebug_CommandGroup{
        InId,
        MoveTemp(InAccessibleLabel),
        ECkDebug_CommandBarLane::Context,
        MoveTemp(InContent)};
}

auto
    FCkDebug_CommandGroup::
    IsValid() const
    -> bool
{
    return NOT Id.IsNone()
        && NOT AccessibleLabel.IsEmpty()
        && (Lane == ECkDebug_CommandBarLane::Primary || Lane == ECkDebug_CommandBarLane::Context)
        && Content.IsValid()
        && Content.ToSharedRef() != SNullWidget::NullWidget;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_CommandBarLayout::
    Get_SeparatorCount() const
    -> int32
{
    return FMath::Max(0, PrimaryGroupIndices.Num() - 1)
        + FMath::Max(0, ContextGroupIndices.Num() - 1);
}

auto
    FCkDebug_CommandBarLayout::
    TryBuild(
        const TArray<FCkDebug_CommandGroup>& InGroups,
        FCkDebug_CommandBarLayout& OutLayout)
    -> bool
{
    OutLayout = {};

    auto SeenIds = TSet<FName>{};
    SeenIds.Reserve(InGroups.Num());

    for (auto Index = 0; Index < InGroups.Num(); ++Index)
    {
        const auto& Group = InGroups[Index];
        if (NOT Group.IsValid() || SeenIds.Contains(Group.Id))
        {
            OutLayout = {};
            return false;
        }

        SeenIds.Add(Group.Id);
        if (Group.Lane == ECkDebug_CommandBarLane::Primary)
        { OutLayout.PrimaryGroupIndices.Add(Index); }
        else
        { OutLayout.ContextGroupIndices.Add(Index); }
    }

    return true;
}

// ====================================================================================================================

auto
    SCkDebug_CommandBar::
    Construct(const FArguments& InArgs)
    -> void
{
    auto Layout = FCkDebug_CommandBarLayout{};
    const auto LayoutIsValid = FCkDebug_CommandBarLayout::TryBuild(InArgs._Groups, Layout);
    CK_ENSURE_IF_NOT(LayoutIsValid, TEXT("Invalid common debugger command bar configuration"))
    {}
    if (NOT LayoutIsValid)
    {
        ChildSlot[SNullWidget::NullWidget];
        return;
    }

    _Groups = InArgs._Groups;
    _Layout = MoveTemp(Layout);

    const auto UtilityContent = InArgs._UtilityContent.Widget;
    const auto HasUtilityContent = UtilityContent != SNullWidget::NullWidget;
    const auto HasContextGroups = NOT _Layout.ContextGroupIndices.IsEmpty();

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBorder)
            .BorderImage_Lambda([]{ return ck::debug_axes::Get_SurfaceBrush(1); })
            .BorderBackgroundColor_Lambda([]{ return FSlateColor{ck::debug_axes::Get_SurfaceTint(1)}; })
            .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Center)
                [
                    Build_Lane(_Layout.PrimaryGroupIndices)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .Visibility(HasUtilityContent ? EVisibility::Visible : EVisibility::Collapsed)
                    [
                        UtilityContent
                    ]
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBorder)
            .Visibility(HasContextGroups ? EVisibility::Visible : EVisibility::Collapsed)
            .BorderImage_Lambda([]{ return ck::debug_axes::Get_SurfaceBrush(0); })
            .BorderBackgroundColor_Lambda([]{ return FSlateColor{ck::debug_axes::Get_SurfaceTint(0)}; })
            .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceXS})
            [
                Build_Lane(_Layout.ContextGroupIndices)
            ]
        ]
    ];
}

auto
    SCkDebug_CommandBar::
    Build_Lane(const TArray<int32>& InGroupIndices) const
    -> TSharedRef<SWidget>
{
    auto Row = SNew(SHorizontalBox);

    for (auto LaneIndex = 0; LaneIndex < InGroupIndices.Num(); ++LaneIndex)
    {
        const auto GroupIndex = InGroupIndices[LaneIndex];
        Row->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            Build_Group(_Groups[GroupIndex], LaneIndex > 0)
        ];
    }

    return SNew(SScrollBox)
        .Orientation(Orient_Horizontal)
        .ScrollBarVisibility(EVisibility::Collapsed)
        .ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)

        + SScrollBox::Slot()
        [
            Row
        ];
}

auto
    SCkDebug_CommandBar::
    Build_Group(const FCkDebug_CommandGroup& InGroup, bool InHasLeadingSeparator) const
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        .AccessibleText(InGroup.AccessibleLabel)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .Visibility(InHasLeadingSeparator ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
            [
                Build_Separator()
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(InHasLeadingSeparator ? CkStyle::SpaceS : 0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            InGroup.Content.ToSharedRef()
        ];
}

auto
    SCkDebug_CommandBar::
    Build_Separator() const
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .WidthOverride_Lambda([]() -> FOptionalSize
        {
            return FOptionalSize{ck::debug_axes::Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection())};
        })
        .HeightOverride(18.0f)
        .Visibility_Lambda([]()
        {
            return ck::debug_axes::Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection()) > 0.0f
                ? EVisibility::HitTestInvisible
                : EVisibility::Collapsed;
        })
        [
            SNew(SImage)
            .Image(FCkDebuggerStyle::Get_SeparatorBrush())
        ];
}

// ====================================================================================================================
