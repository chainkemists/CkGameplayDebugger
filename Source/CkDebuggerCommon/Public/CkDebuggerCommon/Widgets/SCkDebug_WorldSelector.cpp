#include "SCkDebug_WorldSelector.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

// ====================================================================================================================

auto SCkDebug_WorldSelector::Construct(
    const FArguments& InArgs,
    TSharedPtr<FCkDebuggerModel_WorldSelector> InModel) -> void
{
    _Model = InModel;
    _OnWorldChanged = InArgs._OnWorldChanged;
    _ShowHeaderLabel = InArgs._ShowHeaderLabel;

    // Seed so the very first Tick processes the world list immediately rather than
    // after a full WorldCheckInterval of dead air (snappier auto-select on open).
    _TimeSinceWorldCheck = WorldCheckInterval;

    auto StripContainer = SNew(SBox)
    [
        Build_ButtonStrip()
    ];
    _StripContainer = StripContainer;

    if (InArgs._ShowHeaderLabel)
    {
        ChildSlot
        [
            SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
            .Padding(FMargin(CkStyle::SpaceS))
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("World Selection")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    StripContainer
                ]
            ]
        ];
    }
    else
    {
        ChildSlot
        [
            StripContainer
        ];
    }
}

auto SCkDebug_WorldSelector::Build_ButtonStrip() -> TSharedRef<SWidget>
{
    auto ButtonRow = SNew(SHorizontalBox);

    if (NOT _Model.IsValid())
    { return ButtonRow; }

    const auto AvailableWorlds = _Model->Get_AvailableWorlds();

    for (auto Index = 0; Index < AvailableWorlds.Num(); ++Index)
    {
        auto* World = AvailableWorlds[Index];
        if (ck::Is_NOT_Valid(World))
        { continue; }

        const TWeakObjectPtr<UWorld> WorldWeak(World);
        const auto WorldLabel = ck::Format_UE(TEXT("{}"), World->GetNetMode());

        // Sidebar form fills the container width evenly; compact toolbar form gives
        // each button more padding so it reads as a real button next to the other
        // controls (and the AutoWidth slot below sizes it to its full label).
        const auto ContentPadding = _ShowHeaderLabel
            ? FMargin(CkStyle::SpaceS, 2.0f)
            : FMargin(CkStyle::SpaceM, 3.0f);

        auto ButtonWidget = SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor_Lambda([this, WorldWeak]()
            {
                const auto bIsSelected = _Model.IsValid() && _Model->Get_SelectedWorld() == WorldWeak.Get();
                auto C = bIsSelected ? CkStyle::Selection() : CkStyle::Border();
                C.A = bIsSelected ? 0.50f : 0.25f;
                return FSlateColor(C);
            })
            .Padding(FMargin(1.0f))
            // Right-click → "Copy Text". SButton ignores non-left-mouse buttons,
            // so right-click bubbles up to this SBorder handler.
            .OnMouseButtonDown(this, &SCkDebug_WorldSelector::OnWorldButtonRightClicked, WorldLabel)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .OnClicked(this, &SCkDebug_WorldSelector::OnWorldButtonClicked, WorldWeak)
                .ContentPadding(ContentPadding)
                .HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(WorldLabel))
                    .ColorAndOpacity_Lambda([this, WorldWeak]()
                    {
                        const auto bIsSelected = _Model.IsValid() && _Model->Get_SelectedWorld() == WorldWeak.Get();
                        return FSlateColor(bIsSelected ? CkStyle::Selection() : CkStyle::Text());
                    })
                    .Justification(ETextJustify::Center)
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]
            ];

        const auto SlotPadding = FMargin(Index > 0 ? CkStyle::SpaceXS : 0.0f, 0.0f, 0.0f, 0.0f);

        if (_ShowHeaderLabel)
        {
            // Sidebar: split the container width evenly between worlds.
            ButtonRow->AddSlot().FillWidth(1.0f).Padding(SlotPadding) [ ButtonWidget ];
        }
        else
        {
            // Toolbar: size each button to its label so long NetMode names fit.
            ButtonRow->AddSlot().AutoWidth().Padding(SlotPadding) [ ButtonWidget ];
        }
    }

    return ButtonRow;
}

auto SCkDebug_WorldSelector::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    _TimeSinceWorldCheck += InDeltaTime;
    if (_TimeSinceWorldCheck < WorldCheckInterval || NOT _Model.IsValid())
    { return; }

    _TimeSinceWorldCheck = 0.0f;

    // Detect changes by IDENTITY, not just count. A quick PIE stop→restart can
    // produce the same count with a different UWorld* — count-only gating would
    // miss it and leave a stale TWeakObjectPtr selected.
    const auto AvailableWorlds = _Model->Get_AvailableWorlds();

    auto WorldsChanged = AvailableWorlds.Num() != _LastKnownWorlds.Num();
    if (NOT WorldsChanged)
    {
        for (auto Index = 0; Index < AvailableWorlds.Num(); ++Index)
        {
            if (_LastKnownWorlds[Index].Get() != AvailableWorlds[Index])
            {
                WorldsChanged = true;
                break;
            }
        }
    }

    if (NOT WorldsChanged)
    { return; }

    _LastKnownWorlds.Reset(AvailableWorlds.Num());
    for (auto* World : AvailableWorlds)
    {
        _LastKnownWorlds.Emplace(World);
    }

    // Re-resolve the selection: pick the first world if none is selected or the
    // previous one was destroyed (its weak ptr now reads null).
    _Model->Ensure_AutoSelect();

    // Worlds-list change is a context change — rebuild the strip (structure only
    // changes here, never per-click) and let the host refresh its dependent views.
    if (_StripContainer.IsValid())
    {
        _StripContainer->SetContent(Build_ButtonStrip());
    }

    _OnWorldChanged.ExecuteIfBound();
}

auto SCkDebug_WorldSelector::OnWorldButtonClicked(TWeakObjectPtr<UWorld> InWorldWeak) -> FReply
{
    auto* InWorld = InWorldWeak.Get();
    if (NOT _Model.IsValid() || ck::Is_NOT_Valid(InWorld))
    { return FReply::Handled(); }

    const auto* PreviousWorld = _Model->Get_SelectedWorld();
    _Model->Set_SelectedWorld(InWorld);

    if (PreviousWorld != InWorld)
    {
        _OnWorldChanged.ExecuteIfBound();
    }

    return FReply::Handled();
}

auto SCkDebug_WorldSelector::OnWorldButtonRightClicked(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent,
    FString InLabel) -> FReply
{
    return ck::DebugCopyMenu::Handle_RightClickToCopy(AsShared(), InMouseEvent, InLabel);
}

// ====================================================================================================================
