#include "CkSchedulerDebuggerPage_Combined.h"

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"
#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Timeline.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_ProcessorTree.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Inspector.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Combined::
    Get_PageName() const
    -> FText
{
    return FText::FromString(TEXT("Combined"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Combined::
    Build_Content(
        TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel)
    -> TSharedRef<SWidget>
{
    _ViewModel = InViewModel;

    auto Content = SNew(SHorizontalBox)

        // ---- Main area (toolbar + splitter)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNew(SVerticalBox)

            // ---- Toolbar
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                DoBuildToolbar()
            ]

            // ---- Main vertical splitter (tree / timeline)
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(_MainSplitter, SSplitter)
                .Orientation(Orient_Vertical)

                // ---- Top: Simplified tree view with detail graph placeholder
                + SSplitter::Slot()
                .Value(_TopRatio)
                [
                    SNew(SSplitter)
                    .Orientation(Orient_Horizontal)

                    // ---- Left: Processor tree
                    + SSplitter::Slot()
                    .Value(0.5f)
                    [
                        SNew(SCkSchedulerDebugger_ProcessorTree)
                        .ViewModel(_ViewModel)
                    ]

                    // ---- Right: Detail graph placeholder
                    + SSplitter::Slot()
                    .Value(0.5f)
                    [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FCkSchedulerDebuggerStyle::Color_Background_Medium)
                        .Padding(FCkSchedulerDebuggerStyle::Padding_Large)
                        [
                            SNew(SBox)
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Detail Graph")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
                                .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
                            ]
                        ]
                    ]
                ]

                // ---- Bottom: Timeline
                + SSplitter::Slot()
                .Value(1.0f - _TopRatio)
                [
                    SNew(SCkSchedulerDebugger_Timeline)
                    .ViewModel(_ViewModel)
                ]
            ]
        ]

        // ---- Right sidebar: Inspector (spanning full height)
        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SBox)
            .WidthOverride(280.0f)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FCkSchedulerDebuggerStyle::Color_Background_Medium)
                [
                    SNew(SCkSchedulerDebugger_Inspector)
                    .ViewModel(_ViewModel)
                ]
            ]
        ];

    return Content;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Combined::
    Tick(
        float InDeltaTime)
    -> void
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Combined::
    OnSelectionChanged(
        int32 InProcessorIndex)
    -> void
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Combined::
    DoBuildToolbar()
    -> TSharedRef<SWidget>
{
    const auto ButtonFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

    auto MakeLayoutButton = [this, &ButtonFont](const FString& InLabel, ELayoutPreset InPreset, float InTopRatio) -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonColorAndOpacity_Lambda([this, InPreset]() -> FLinearColor
            {
                return (_ActivePreset == InPreset)
                    ? FCkSchedulerDebuggerStyle::Color_Selection
                    : FCkSchedulerDebuggerStyle::Color_Background_Light;
            })
            .OnClicked_Lambda([this, InPreset, InTopRatio]() -> FReply
            {
                _ActivePreset = InPreset;
                DoOnLayoutChanged(InTopRatio);
                return FReply::Handled();
            })
            .ContentPadding(FMargin{8.0f, 4.0f})
            [
                SNew(STextBlock)
                .Text(FText::FromString(InLabel))
                .Font(ButtonFont)
                .ColorAndOpacity_Lambda([this, InPreset]() -> FSlateColor
                {
                    return (_ActivePreset == InPreset)
                        ? FSlateColor{FCkSchedulerDebuggerStyle::Color_Text_Highlight}
                        : FSlateColor{FCkSchedulerDebuggerStyle::Color_Text_Secondary};
                })
            ];
    };

    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FCkSchedulerDebuggerStyle::Color_Background_Medium)
        .Padding(FMargin{FCkSchedulerDebuggerStyle::Padding_Small})
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Layout:")))
                .Font(ButtonFont)
                .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
            [
                MakeLayoutButton(TEXT("50 / 50"), ELayoutPreset::Split5050, 0.5f)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
            [
                MakeLayoutButton(TEXT("Tree 70 / 30"), ELayoutPreset::TreeFocus, 0.7f)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                MakeLayoutButton(TEXT("Timeline 30 / 70"), ELayoutPreset::TimelineFocus, 0.3f)
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Combined::
    DoOnLayoutChanged(
        float InTopRatio)
    -> void
{
    _TopRatio = InTopRatio;

    if (NOT _MainSplitter.IsValid()) { return; }

    auto& TopSlot = _MainSplitter->SlotAt(0);
    auto& BottomSlot = _MainSplitter->SlotAt(1);

    TopSlot.SetSizeValue(InTopRatio);
    BottomSlot.SetSizeValue(1.0f - InTopRatio);
}

// --------------------------------------------------------------------------------------------------------------------
