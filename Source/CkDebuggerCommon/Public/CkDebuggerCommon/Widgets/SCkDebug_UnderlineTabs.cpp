#include "SCkDebug_UnderlineTabs.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"

#include "CkEditorTools/Style/CkIconStyle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================
// Named, not anonymous: this module builds with unity on, and a merged TU collides file-local
// helpers by name.
// ====================================================================================================================

namespace ck_debug_underline_tabs
{
    // What the overflow control is assumed to need before Slate has measured it once.
    // Deliberately generous -- under-reserving hands a tab a slot the button then covers.
    constexpr auto OverflowButtonWidthFallback = 46.0f;

    // Deadband on the allotted width. The partition itself is cheap, but re-running it within a
    // pixel of a fit boundary is what makes a tab strip flicker while a splitter is dragged.
    constexpr auto WidthDeadband = 4.0f;

    constexpr auto WarnDotSize = 6.0f;

    auto Get_IsMeasurable(float InWidth) -> bool
    {
        return FMath::IsFinite(InWidth) && InWidth > 0.0f;
    }

    auto Get_OccupiesSpace(EVisibility InVisibility) -> bool
    {
        // Collapsed costs no layout width; Hidden still does, so a hidden tab keeps its slot.
        return InVisibility != EVisibility::Collapsed;
    }
}

// ====================================================================================================================

auto
    FCkDebug_UnderlineTabLayout::
    Get_IsEquivalentTo(
        const FCkDebug_UnderlineTabLayout& InOther) const
    -> bool
{
    return VisibleIndices == InOther.VisibleIndices
        && OverflowIndices == InOther.OverflowIndices;
}

auto
    FCkDebug_UnderlineTabLayout::
    Compute(
        float InAvailableWidth,
        const TArray<float>& InDesiredWidths,
        float InOverflowButtonWidth,
        int32 InActiveIndex)
    -> FCkDebug_UnderlineTabLayout
{
    using namespace ck_debug_underline_tabs;

    auto Result = FCkDebug_UnderlineTabLayout{};

    const auto Count = InDesiredWidths.Num();

    if (Count <= 0)
    { return Result; }

    auto InputIsMeasurable = Get_IsMeasurable(InAvailableWidth)
        && FMath::IsFinite(InOverflowButtonWidth)
        && InOverflowButtonWidth >= 0.0f;

    for (auto Index = 0; InputIsMeasurable && Index < Count; ++Index)
    { InputIsMeasurable = Get_IsMeasurable(InDesiredWidths[Index]); }

    // Fail CLOSED. One anchor tab on the line and every other tab in the overflow menu is still a
    // strip in which nothing is unreachable -- which is the only guarantee worth keeping when the
    // measurements cannot be trusted.
    if (NOT InputIsMeasurable)
    {
        const auto Anchor = InDesiredWidths.IsValidIndex(InActiveIndex) ? InActiveIndex : 0;

        Result.VisibleIndices.Add(Anchor);

        for (auto Index = 0; Index < Count; ++Index)
        {
            if (Index != Anchor)
            { Result.OverflowIndices.Add(Index); }
        }

        return Result;
    }

    auto TotalWidth = 0.0f;
    for (const auto Width : InDesiredWidths)
    { TotalWidth += Width; }

    if (TotalWidth <= InAvailableWidth)
    {
        Result.VisibleIndices.Reserve(Count);

        for (auto Index = 0; Index < Count; ++Index)
        { Result.VisibleIndices.Add(Index); }

        return Result;
    }

    const auto Budget = InAvailableWidth - InOverflowButtonWidth;
    const auto ActiveIndex = InDesiredWidths.IsValidIndex(InActiveIndex) ? InActiveIndex : INDEX_NONE;

    auto Kept = TArray<bool>{};
    Kept.Init(false, Count);

    auto Used = 0.0f;

    // The active tab is reserved BEFORE the greedy pass, and its width is allowed to exceed the
    // budget on its own: a strip whose selected page has scrolled out of reach is exactly the
    // defect this partition exists to prevent.
    if (ActiveIndex != INDEX_NONE)
    {
        Kept[ActiveIndex] = true;
        Used = InDesiredWidths[ActiveIndex];
    }

    for (auto Index = 0; Index < Count; ++Index)
    {
        if (Index == ActiveIndex)
        { continue; }

        if (Used + InDesiredWidths[Index] > Budget)
        { break; }

        Used += InDesiredWidths[Index];
        Kept[Index] = true;
    }

    for (auto Index = 0; Index < Count; ++Index)
    {
        if (Kept[Index])
        { Result.VisibleIndices.Add(Index); }
        else
        { Result.OverflowIndices.Add(Index); }
    }

    return Result;
}

// ====================================================================================================================

auto
    SCkDebug_UnderlineTabs::
    Construct(const FArguments& InArgs)
    -> void
{
    using namespace ck_debug_underline_tabs;

    _Tabs          = InArgs._Tabs;
    _ActiveTabId   = InArgs._ActiveTabId;
    _OnTabSelected = InArgs._OnTabSelected;
    _TabPadding    = InArgs._TabPadding;
    _FontSize      = InArgs._FontSize > 0 ? InArgs._FontSize : CkStyle::FontSizeBody();

    _Tabs.StableSort([](const FCkDebug_UnderlineTabDesc& InLhs, const FCkDebug_UnderlineTabDesc& InRhs)
    {
        return InLhs.SortOrder < InRhs.SortOrder;
    });

    _MeasuredWidths.Init(0.0f, _Tabs.Num());
    _TabWidgets.Reserve(_Tabs.Num());

    for (const auto& Tab : _Tabs)
    { _TabWidgets.Add(Build_Tab(Tab)); }

    _OverflowButtonWidth = OverflowButtonWidthFallback;

    const auto WeakTabs = TWeakPtr<SCkDebug_UnderlineTabs>(SharedThis(this));

    _OverflowButton = SNew(SComboButton)
        .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
        .ContentPadding(FMargin{CkStyle::SpaceM, _TabPadding.Top})
        .HasDownArrow(false)
        .ToolTipText_Lambda([WeakTabs]() -> FText
        {
            const auto Pinned = WeakTabs.Pin();
            const auto Count = Pinned.IsValid() ? Pinned->_Layout.OverflowIndices.Num() : 0;

            return FText::FromString(ck::Format_UE(TEXT("{} more tab(s) - click to reach them"), Count));
        })
        .Visibility_Lambda([WeakTabs]() -> EVisibility
        {
            const auto Pinned = WeakTabs.Pin();
            const auto HasOverflow = Pinned.IsValid() && NOT Pinned->_Layout.OverflowIndices.IsEmpty();

            return HasOverflow ? EVisibility::Visible : EVisibility::Collapsed;
        })
        .OnGetMenuContent(FOnGetContent::CreateSP(this, &SCkDebug_UnderlineTabs::Build_OverflowMenu))
        .ButtonContent()
        [
            SNew(STextBlock)
            .Font(CkStyle::BoldFont(_FontSize))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            .Text_Lambda([WeakTabs]() -> FText
            {
                const auto Pinned = WeakTabs.Pin();
                const auto Count = Pinned.IsValid() ? Pinned->_Layout.OverflowIndices.Num() : 0;

                // U+22EF MIDLINE HORIZONTAL ELLIPSIS, built by code point so this file stays
                // plain ASCII and no hex-escape greediness can swallow the next character.
                const auto Ellipsis = FString::Chr(TCHAR(0x22EF));

                return FText::FromString(ck::Format_UE(TEXT("{} {}"), Ellipsis, Count));
            })
        ];

    _Layout = {};
    _Layout.VisibleIndices.Reserve(_Tabs.Num());

    for (auto Index = 0; Index < _Tabs.Num(); ++Index)
    { _Layout.VisibleIndices.Add(Index); }

    ChildSlot
    [
        SAssignNew(_Row, SHorizontalBox)
    ];

    Rebuild_Row();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_UnderlineTabs::
    Build_TabDecorations(
        const TSharedRef<SHorizontalBox>& InRow,
        const FCkDebug_UnderlineTabDesc& InTab) const
    -> void
{
    using namespace ck_debug_underline_tabs;

    if (InTab.CountText.IsSet() || InTab.CountText.IsBound())
    {
        const auto CountText = InTab.CountText;

        InRow->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush_Small())
                .BorderBackgroundColor(FSlateColor{CkStyle::NeutralDim()})
                .Padding(FMargin{5.0f, 1.0f})
                .Visibility_Lambda([CountText]
                {
                    return CountText.Get(FText::GetEmpty()).IsEmpty()
                        ? EVisibility::Collapsed
                        : EVisibility::SelfHitTestInvisible;
                })
                [
                    SNew(STextBlock)
                    .Text(CountText)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ];
    }

    const auto ShowWarnDot = InTab.ShowWarnDot;

    InRow->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(WarnDotSize)
            .HeightOverride(WarnDotSize)
            .Visibility_Lambda([ShowWarnDot]
            {
                return ShowWarnDot.Get(false) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
            })
            [
                SNew(SImage)
                .Image(CkStyle::GetRoundedBrush_Pill())
                .ColorAndOpacity(FSlateColor{CkStyle::Warn()})
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_UnderlineTabs::
    Build_Tab(const FCkDebug_UnderlineTabDesc& InTab) const
    -> TSharedRef<SWidget>
{
    const auto ActiveTabId = _ActiveTabId;
    const auto OnSelected  = _OnTabSelected;
    const auto TabId       = InTab.Id;
    const auto IsActive    = [ActiveTabId, TabId] { return ActiveTabId.Get(NAME_None) == TabId; };

    auto LabelRow = SNew(SHorizontalBox);

    // The tab button carries no tooltip of its own, so the glyph owns its Meaning rather than
    // deferring to a richer wrapper tooltip.
    if (const auto* IconBrush = FCkIconStyle::Get_Brush(InTab.IconId, ECk_Icon_BrushSize::Size_16x16))
    {
        LabelRow->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(IconBrush)
                .Meaning(InTab.Label)
                .ColorAndOpacity_Lambda([IsActive]
                {
                    return FSlateColor{IsActive() ? CkStyle::Text() : CkStyle::TextDim()};
                })
            ];
    }

    LabelRow->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(InTab.Label)
            .Font(CkStyle::BoldFont(_FontSize))
            .ColorAndOpacity_Lambda([IsActive]
            {
                return FSlateColor{IsActive() ? CkStyle::Text() : CkStyle::TextDim()};
            })
        ];

    Build_TabDecorations(LabelRow, InTab);

    return SNew(SBox)
        .Visibility(InTab.Visibility)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SButton)
                .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                .ContentPadding(_TabPadding)
                .OnClicked_Lambda([OnSelected, TabId]
                {
                    OnSelected.ExecuteIfBound(TabId);
                    return FReply::Handled();
                })
                [
                    LabelRow
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .HeightOverride(2.0f)
                [
                    SNew(SImage)
                    .Image(CkStyle::GetFilledBrush())
                    .ColorAndOpacity_Lambda([IsActive]
                    {
                        return FSlateColor{IsActive() ? CkStyle::Accent() : FLinearColor::Transparent};
                    })
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_UnderlineTabs::
    Build_OverflowMenu()
    -> TSharedRef<SWidget>
{
    // Built ON OPEN, never per frame: the menu only exists while the user is looking at it.
    auto MenuColumn = SNew(SVerticalBox);

    for (const auto TabIndex : _Layout.OverflowIndices)
    {
        if (NOT _Tabs.IsValidIndex(TabIndex))
        { continue; }

        const auto& Tab = _Tabs[TabIndex];

        const auto ActiveTabId = _ActiveTabId;
        const auto OnSelected  = _OnTabSelected;
        const auto TabId       = Tab.Id;
        const auto IsActive    = [ActiveTabId, TabId] { return ActiveTabId.Get(NAME_None) == TabId; };

        auto EntryRow = SNew(SHorizontalBox);

        if (const auto* IconBrush = FCkIconStyle::Get_Brush(Tab.IconId, ECk_Icon_BrushSize::Size_16x16))
        {
            EntryRow->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(IconBrush)
                    .Meaning(Tab.Label)
                    .ColorAndOpacity_Lambda([IsActive]
                    {
                        return FSlateColor{IsActive() ? CkStyle::Text() : CkStyle::TextDim()};
                    })
                ];
        }

        EntryRow->AddSlot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(Tab.Label)
                .Font(CkStyle::BoldFont(_FontSize))
                .ColorAndOpacity_Lambda([IsActive]
                {
                    return FSlateColor{IsActive() ? CkStyle::Text() : CkStyle::TextDim()};
                })
            ];

        Build_TabDecorations(EntryRow, Tab);

        MenuColumn->AddSlot()
            .AutoHeight()
            [
                SNew(SButton)
                .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                .ContentPadding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
                .HAlign(HAlign_Fill)
                .Visibility(Tab.Visibility)
                .OnClicked_Lambda([OnSelected, TabId]
                {
                    if (FSlateApplication::IsInitialized())
                    { FSlateApplication::Get().DismissAllMenus(); }

                    OnSelected.ExecuteIfBound(TabId);
                    return FReply::Handled();
                })
                [
                    EntryRow
                ]
            ];
    }

    return SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(FSlateColor{CkStyle::Bg1()})
        .Padding(FMargin{CkStyle::SpaceS})
        [
            SNew(SBox)
            .MinDesiredWidth(160.0f)
            [
                MenuColumn
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_UnderlineTabs::
    Get_ActiveIndex() const
    -> int32
{
    const auto ActiveId = _ActiveTabId.Get(NAME_None);

    if (ActiveId.IsNone())
    { return INDEX_NONE; }

    return _Tabs.IndexOfByPredicate([ActiveId](const FCkDebug_UnderlineTabDesc& InTab)
    {
        return InTab.Id == ActiveId;
    });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_UnderlineTabs::
    Refresh_Measurements()
    -> void
{
    using namespace ck_debug_underline_tabs;

    for (auto Index = 0; Index < _TabWidgets.Num(); ++Index)
    {
        const auto& TabWidget = _TabWidgets[Index];

        if (NOT TabWidget.IsValid() || NOT _MeasuredWidths.IsValidIndex(Index))
        { continue; }

        // Only ADOPT a real measurement. A tab parked in the overflow menu is out of the layout
        // tree and keeps the last width Slate gave it, which is what makes the partition stable
        // instead of oscillating the moment a tab leaves the line.
        const auto Measured = static_cast<float>(TabWidget->GetDesiredSize().X);

        if (Get_IsMeasurable(Measured))
        { _MeasuredWidths[Index] = Measured; }
    }

    if (_OverflowButton.IsValid())
    {
        const auto Measured = static_cast<float>(_OverflowButton->GetDesiredSize().X);

        if (Get_IsMeasurable(Measured))
        { _OverflowButtonWidth = Measured; }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_UnderlineTabs::
    Refresh_Layout(float InAvailableWidth)
    -> void
{
    using namespace ck_debug_underline_tabs;

    const auto TabCount = _Tabs.Num();

    if (TabCount <= 0 || NOT _Row.IsValid())
    { return; }

    // Tabs that cost no layout width are partitioned around rather than through: a collapsed tab
    // in the overflow menu would be an entry the reader cannot see and cannot pick.
    auto Participating = TArray<int32>{};
    auto Widths        = TArray<float>{};
    auto WidthSum      = 0.0f;
    auto Signature     = uint32{0};
    auto AllMeasured   = true;

    Participating.Reserve(TabCount);
    Widths.Reserve(TabCount);

    for (auto Index = 0; Index < TabCount; ++Index)
    {
        const auto Occupies = Get_OccupiesSpace(_Tabs[Index].Visibility.Get(EVisibility::Visible));
        Signature = HashCombine(Signature, Occupies ? 1u : 0u);

        if (NOT Occupies)
        { continue; }

        const auto Width = _MeasuredWidths.IsValidIndex(Index) ? _MeasuredWidths[Index] : 0.0f;

        if (NOT Get_IsMeasurable(Width))
        { AllMeasured = false; }

        Participating.Add(Index);
        Widths.Add(Width);
        WidthSum += Width;
    }

    // Before Slate's first prepass nothing has a width. Staying on "everything on the line" until
    // then is both the honest answer and the pre-overflow behaviour.
    if (NOT AllMeasured)
    { return; }

    const auto ActiveIndex = Get_ActiveIndex();

    const auto WidthIsStable = FMath::Abs(InAvailableWidth - _LastLayoutWidth) < WidthDeadband
        && FMath::Abs(WidthSum - _LastLayoutWidthSum) < 0.5f
        && ActiveIndex == _LastLayoutActiveIndex
        && Signature == _LastLayoutVisibilitySignature;

    if (WidthIsStable)
    { return; }

    _LastLayoutWidth               = InAvailableWidth;
    _LastLayoutWidthSum            = WidthSum;
    _LastLayoutActiveIndex         = ActiveIndex;
    _LastLayoutVisibilitySignature = Signature;

    const auto LocalActiveIndex = Participating.IndexOfByKey(ActiveIndex);
    const auto Computed = FCkDebug_UnderlineTabLayout::Compute(
        InAvailableWidth,
        Widths,
        _OverflowButtonWidth,
        LocalActiveIndex);

    auto OnTheLine = TArray<bool>{};
    OnTheLine.Init(true, TabCount);

    auto NewLayout = FCkDebug_UnderlineTabLayout{};

    for (const auto LocalIndex : Computed.OverflowIndices)
    {
        if (NOT Participating.IsValidIndex(LocalIndex))
        { continue; }

        OnTheLine[Participating[LocalIndex]] = false;
    }

    for (auto Index = 0; Index < TabCount; ++Index)
    {
        if (OnTheLine[Index])
        { NewLayout.VisibleIndices.Add(Index); }
        else
        { NewLayout.OverflowIndices.Add(Index); }
    }

    if (NewLayout.Get_IsEquivalentTo(_Layout))
    { return; }

    _Layout = MoveTemp(NewLayout);

    Rebuild_Row();
    Invalidate(EInvalidateWidgetReason::Layout);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_UnderlineTabs::
    Rebuild_Row()
    -> void
{
    if (NOT _Row.IsValid())
    { return; }

    _Row->ClearChildren();

    for (const auto TabIndex : _Layout.VisibleIndices)
    {
        if (NOT _TabWidgets.IsValidIndex(TabIndex) || NOT _TabWidgets[TabIndex].IsValid())
        { continue; }

        _Row->AddSlot()
            .AutoWidth()
            [
                _TabWidgets[TabIndex].ToSharedRef()
            ];
    }

    if (_OverflowButton.IsValid())
    {
        _Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                _OverflowButton.ToSharedRef()
            ];
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_UnderlineTabs::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    Refresh_Measurements();
    Refresh_Layout(static_cast<float>(InAllottedGeometry.GetLocalSize().X));
}

// ====================================================================================================================
