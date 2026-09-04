#include "SCkDebug_CommandBar.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"

// ====================================================================================================================

namespace ck_debug_command_bar
{
    // Width of the dissolve at each lane edge. Wide enough that a clipped control is visibly cut
    // rather than merely truncated, narrow enough that it never hides a whole control.
    constexpr auto EdgeFadeWidth = CkStyle::SpaceXXL;

    // A scroller resting within a pixel of an end IS at that end: the offset is a float that lands
    // on fractional values after inertial scrolling and DPI-scaled layout, so an exact comparison
    // would leave a hairline fade permanently lit at both extremes.
    constexpr auto EdgeFadeEpsilon = 1.0f;

    auto LeftFade_IsVisible(const TWeakPtr<SScrollBox>& InLane) -> bool
    {
        const auto Lane = InLane.Pin();
        if (NOT Lane.IsValid())
        { return false; }

        return Lane->GetScrollOffset() > EdgeFadeEpsilon;
    }

    auto RightFade_IsVisible(const TWeakPtr<SScrollBox>& InLane) -> bool
    {
        const auto Lane = InLane.Pin();
        if (NOT Lane.IsValid())
        { return false; }

        // GetScrollOffsetOfEnd is max(0, content - viewport), so content that fits reports 0 and
        // both fades stay collapsed without a separate "is scrollable at all" question.
        return Lane->GetScrollOffset() < Lane->GetScrollOffsetOfEnd() - EdgeFadeEpsilon;
    }
}

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
                    Build_Lane(_Layout.PrimaryGroupIndices, 1)
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
                Build_Lane(_Layout.ContextGroupIndices, 0)
            ]
        ]
    ];
}

auto
    SCkDebug_CommandBar::
    Build_Lane(const TArray<int32>& InGroupIndices, int32 InSurfaceDepth) const
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

    // The lane stays ONE physical line that scrolls horizontally — the scrollbar stays collapsed
    // because a bar under a two-lane chrome strip costs more vertical space than the strip has.
    // What the collapsed bar removed was the affordance, not the scrolling, so the overlay below
    // puts the affordance back without reserving any layout at all.
    const auto Lane = SNew(SScrollBox)
        .Orientation(Orient_Horizontal)
        .ScrollBarVisibility(EVisibility::Collapsed)
        .ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)

        + SScrollBox::Slot()
        [
            Row
        ];

    return SNew(SOverlay)

        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [
            Lane
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Fill)
        [
            Build_EdgeFade(Lane, true, InSurfaceDepth)
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Fill)
        [
            Build_EdgeFade(Lane, false, InSurfaceDepth)
        ];
}

auto
    SCkDebug_CommandBar::
    Build_EdgeFade(const TSharedRef<SScrollBox>& InLane, bool InIsLeftEdge, int32 InSurfaceDepth) const
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .WidthOverride(ck_debug_command_bar::EdgeFadeWidth)
        .Visibility_Lambda([LaneWeak = TWeakPtr<SScrollBox>{InLane}, InIsLeftEdge]()
        {
            const auto FadeIsWarranted = InIsLeftEdge
                ? ck_debug_command_bar::LeftFade_IsVisible(LaneWeak)
                : ck_debug_command_bar::RightFade_IsVisible(LaneWeak);

            // HitTestInvisible rather than merely Hidden: the fade lies ON TOP of live controls,
            // so a hit-testable layer would swallow clicks on the very buttons it points at.
            return FadeIsWarranted ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
        })
        [
            SNew(SComplexGradient)
            .Visibility(EVisibility::HitTestInvisible)
            // Orient_Vertical means the STOPS are vertical lines, i.e. the color sweeps left to
            // right. A horizontal fade therefore asks for Orient_Vertical — see the gradient
            // branch of FSlateElementBatcher::AddGradientElement.
            .Orientation(Orient_Vertical)
            .GradientColors_Lambda([InIsLeftEdge, InSurfaceDepth]()
            {
                // Read live so a Style Lab palette or SurfaceElevation flip moves the fade with
                // the border it dissolves into, without rebuilding the bar (axes R1).
                const auto Surface = ck::debug_axes::Get_SurfaceTint(InSurfaceDepth);
                const auto Clear = Surface.CopyWithNewOpacity(0.0f);

                return InIsLeftEdge
                    ? TArray<FLinearColor>{Surface, Clear}
                    : TArray<FLinearColor>{Clear, Surface};
            })
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
