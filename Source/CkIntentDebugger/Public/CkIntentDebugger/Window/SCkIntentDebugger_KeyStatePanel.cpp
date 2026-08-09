#include "CkIntentDebugger/Window/SCkIntentDebugger_KeyStatePanel.h"

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"
#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_OctantDial.h"

#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_keystate
{
    // The record's held set is bounded by how many buttons a player can physically hold; a pool this size has
    // never been exhausted by a real profile and costs nothing when unused.
    constexpr auto ButtonSlotCount = 24;

    constexpr auto PadS = 4.0f;
    constexpr auto PadM = 8.0f;

    auto
        Get_EdgeLabel(
            const FCkIntentDebugger_ButtonState& InButton)
        -> FString
    {
        if (InButton.WentDown)
        { return TEXT("press"); }

        if (InButton.WentUp)
        { return TEXT("release"); }

        return TEXT("held");
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_KeyStatePanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    ChildSlot
    [
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().Padding(ck_intent_debugger_keystate::PadM)
        [
            Build_Rosette()
        ]

        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(ck_intent_debugger_keystate::PadM)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()
                [ Build_Readouts() ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, ck_intent_debugger_keystate::PadM, 0.0f, 0.0f)
                [ Build_ButtonPool() ]
            ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_KeyStatePanel::
    Build_Rosette()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkIntentDebugger_KeyStatePanel>(SharedThis(this));

    return SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(TEXT("Direction")))
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, ck_intent_debugger_keystate::PadS)
        [
            SNew(SBox)
                .WidthOverride(148.0f)
                .HeightOverride(148.0f)
            [
                SNew(SCkIntentDebugger_OctantDial)
                    .Octant_Lambda([WeakPanel]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                        { return ECk_Intent_Octant::Neutral; }

                        const auto* Frame = Panel->_ViewModel->TryGet_DisplayedFrame();
                        return Frame != nullptr ? Frame->Octant : ECk_Intent_Octant::Neutral;
                    })
                    .AxisValue_Lambda([WeakPanel]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                        { return FVector2D::ZeroVector; }

                        const auto* Frame = Panel->_ViewModel->TryGet_DisplayedFrame();
                        return Frame != nullptr ? FVector2D{Frame->AxisX, Frame->AxisY} : FVector2D::ZeroVector;
                    })
                    .NeutralRadius_Lambda([WeakPanel]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                        { return 0.0f; }

                        const auto* Source = Panel->_ViewModel->TryGet_SelectedSource();
                        return Source != nullptr ? Source->OctantNeutralRadius : 0.0f;
                    })
            ]
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
                .Font(CkStyle::BoldFont(CkStyle::FontSizeBody()))
                .ColorAndOpacity(CkStyle::Accent())
                .Justification(ETextJustify::Center)
                .Text_Lambda([WeakPanel]()
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                    { return FText::GetEmpty(); }

                    const auto* Frame = Panel->_ViewModel->TryGet_DisplayedFrame();
                    if (Frame == nullptr)
                    { return FText::FromString(TEXT("no record")); }

                    return FText::FromString(ck::intent_debugger::Get_Label(Frame->Octant));
                })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_KeyStatePanel::
    Build_Readouts()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkIntentDebugger_KeyStatePanel>(SharedThis(this));

    const auto MakeRow = [WeakPanel](
        const FString& InKey,
        TFunction<FString(const FCkIntentDebugger_FrameRow&)> InRead) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .ValueText_Lambda([WeakPanel, InRead]()
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                { return FText::GetEmpty(); }

                const auto* Frame = Panel->_ViewModel->TryGet_DisplayedFrame();
                if (Frame == nullptr)
                { return FText::FromString(TEXT("—")); }

                return FText::FromString(InRead(*Frame));
            });
    };

    return SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(TEXT("Frame")))
        ]

        + SVerticalBox::Slot().AutoHeight()
        [ MakeRow(TEXT("Frame index"), [](const FCkIntentDebugger_FrameRow& InRow)
          { return ck::Format_UE(TEXT("{}"), InRow.FrameIndex); }) ]

        + SVerticalBox::Slot().AutoHeight()
        [ MakeRow(TEXT("Conditioned axis"), [](const FCkIntentDebugger_FrameRow& InRow)
          { return ck::Format_UE(TEXT("X {}  Y {}"),
                FString::SanitizeFloat(InRow.AxisX), FString::SanitizeFloat(InRow.AxisY)); }) ]

        + SVerticalBox::Slot().AutoHeight()
        [ MakeRow(TEXT("Octant"), [](const FCkIntentDebugger_FrameRow& InRow)
          { return ck::intent_debugger::Get_Label(InRow.Octant); }) ]

        + SVerticalBox::Slot().AutoHeight()
        [ MakeRow(TEXT("SOCD horizontal"), [](const FCkIntentDebugger_FrameRow& InRow)
          { return ck::intent_debugger::Get_Label(InRow.CleanedHorizontal); }) ]

        + SVerticalBox::Slot().AutoHeight()
        [ MakeRow(TEXT("SOCD vertical"), [](const FCkIntentDebugger_FrameRow& InRow)
          { return ck::intent_debugger::Get_Label(InRow.CleanedVertical); }) ]

        + SVerticalBox::Slot().AutoHeight()
        [ MakeRow(TEXT("Routed this frame"), [](const FCkIntentDebugger_FrameRow& InRow)
          { return ck::Format_UE(TEXT("{} total · {} consumed · {} passed through · {} dropped"),
                InRow.NumRoutedEvents, InRow.NumConsumed, InRow.NumPassedThrough, InRow.NumDropped); }) ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_KeyStatePanel::
    Build_ButtonPool()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkIntentDebugger_KeyStatePanel>(SharedThis(this));

    auto Column = SNew(SVerticalBox);

    Column->AddSlot().AutoHeight()
    [
        SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Held buttons")))
            .SubText(FText::FromString(TEXT("empty when the source carries no button map")))
    ];

    for (auto SlotIndex = 0; SlotIndex < ck_intent_debugger_keystate::ButtonSlotCount; ++SlotIndex)
    {
        Column->AddSlot().AutoHeight()
        [
            Build_ButtonSlot(SlotIndex)
        ];
    }

    Column->AddSlot().AutoHeight().Padding(0.0f, ck_intent_debugger_keystate::PadS)
    [
        SNew(STextBlock)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(CkStyle::TextMute())
            .Visibility_Lambda([WeakPanel]()
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid())
                { return EVisibility::Collapsed; }

                return Panel->Get_ButtonAt(0) == nullptr ? EVisibility::Visible : EVisibility::Collapsed;
            })
            .Text(FText::FromString(TEXT("Nothing held on this row.")))
    ];

    return Column;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_KeyStatePanel::
    Build_ButtonSlot(
        int32 InSlotIndex)
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkIntentDebugger_KeyStatePanel>(SharedThis(this));

    return SNew(SHorizontalBox)
        .Visibility_Lambda([WeakPanel, InSlotIndex]()
        {
            const auto Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid())
            { return EVisibility::Collapsed; }

            return Panel->Get_ButtonAt(InSlotIndex) != nullptr ? EVisibility::Visible : EVisibility::Collapsed;
        })

        + SHorizontalBox::Slot().FillWidth(0.4f).Padding(ck_intent_debugger_keystate::PadS, 1.0f)
        [
            SNew(STextBlock)
                .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(CkStyle::Text())
                .Text_Lambda([WeakPanel, InSlotIndex]()
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid())
                    { return FText::GetEmpty(); }

                    const auto* Button = Panel->Get_ButtonAt(InSlotIndex);
                    return Button != nullptr ? FText::FromString(Button->Label) : FText::GetEmpty();
                })
        ]

        + SHorizontalBox::Slot().FillWidth(0.4f).Padding(ck_intent_debugger_keystate::PadS, 1.0f)
        [
            SNew(STextBlock)
                .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(CkStyle::TextDim())
                .Text_Lambda([WeakPanel, InSlotIndex]()
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid())
                    { return FText::GetEmpty(); }

                    const auto* Button = Panel->Get_ButtonAt(InSlotIndex);
                    if (Button == nullptr)
                    { return FText::GetEmpty(); }

                    return Button->Key.IsValid()
                        ? FText::FromString(Button->Key.ToString())
                        : FText::FromString(TEXT("<unbound>"));
                })
        ]

        + SHorizontalBox::Slot().FillWidth(0.2f).Padding(ck_intent_debugger_keystate::PadS, 1.0f)
        [
            SNew(STextBlock)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .Text_Lambda([WeakPanel, InSlotIndex]()
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid())
                    { return FText::GetEmpty(); }

                    const auto* Button = Panel->Get_ButtonAt(InSlotIndex);
                    return Button != nullptr
                        ? FText::FromString(ck_intent_debugger_keystate::Get_EdgeLabel(*Button))
                        : FText::GetEmpty();
                })
                .ColorAndOpacity_Lambda([WeakPanel, InSlotIndex]()
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid())
                    { return FSlateColor{CkStyle::TextMute()}; }

                    const auto* Button = Panel->Get_ButtonAt(InSlotIndex);
                    if (Button == nullptr)
                    { return FSlateColor{CkStyle::TextMute()}; }

                    if (Button->WentDown)
                    { return FSlateColor{CkStyle::Ok()}; }

                    if (Button->WentUp)
                    { return FSlateColor{CkStyle::Warn()}; }

                    return FSlateColor{CkStyle::TextMute()};
                })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_KeyStatePanel::
    Get_ButtonAt(
        int32 InSlotIndex) const
    -> const FCkIntentDebugger_ButtonState*
{
    if (NOT _ViewModel.IsValid())
    { return nullptr; }

    const auto* Frame = _ViewModel->TryGet_DisplayedFrame();
    if (Frame == nullptr || NOT Frame->Held.IsValidIndex(InSlotIndex))
    { return nullptr; }

    return &Frame->Held[InSlotIndex];
}

// --------------------------------------------------------------------------------------------------------------------
