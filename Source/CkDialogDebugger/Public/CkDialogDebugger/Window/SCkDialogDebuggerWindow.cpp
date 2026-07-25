#include "CkDialogDebugger/Window/SCkDialogDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkDialogDebuggerWindow::WindowId = FName(TEXT("DialogDebugger"));

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    ChildSlot
    [
        SNew(SVerticalBox)
        // Save / Load toolbar — convenience for the Ck_Save / Ck_Load console commands.
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 4.0f, 4.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Save")))
                .ToolTipText(FText::FromString(TEXT("Runs the Ck_Save console command in the active PIE session")))
                .OnClicked_Lambda([this]() { DoExecCommand(TEXT("Ck_Save")); return FReply::Handled(); })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Load")))
                .ToolTipText(FText::FromString(TEXT("Runs the Ck_Load console command in the active PIE session")))
                .OnClicked_Lambda([this]() { DoExecCommand(TEXT("Ck_Load")); return FReply::Handled(); })
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(SCkDebug_DualSearchBar)
            .FilterHintText(FText::FromString(TEXT("Filter lines/emitters…")))
            .OnFilterTextChanged_Lambda([this](const FString& InText) { _FilterString = InText; })
            .OnHighlightTextChanged_Lambda([this](const FString& InText) { _HighlightString = InText; })
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            .Padding(8.0f, 4.0f)
            [
                SAssignNew(_CooldownBox, SVerticalBox)
            ]
            + SScrollBox::Slot()
            .Padding(8.0f, 4.0f)
            [
                SAssignNew(_ContentText, STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                .AutoWrapText(true)
                .Text(FText::FromString(TEXT("(waiting for a PIE session…)")))
            ]
        ]
    ];

    Register_WithGate();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _Collector.Collect(DoGet_PieWorld());
    DoRebuildCooldowns();
    DoRebuildContent();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoGet_PieWorld() const
    -> UWorld*
{
    if (ck::Is_NOT_Valid(GEditor))
    { return nullptr; }

    for (const auto& Context : GEditor->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE && ck::IsValid(Context.World()))
        { return Context.World(); }
    }

    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoExecCommand(
        const FString& InCommand)
    -> void
{
    auto* World = DoGet_PieWorld();
    if (ck::Is_NOT_Valid(World))
    { return; }

    // Prefer the local PlayerController so AS UFUNCTION(Exec) commands (PC-routed) are reached — this is what typing
    // the command in the PIE console does. Fall back to the engine exec chain if there is no controller yet.
    if (auto* PlayerController = World->GetFirstPlayerController(); ck::IsValid(PlayerController))
    { PlayerController->ConsoleCommand(InCommand, true); }
    else if (ck::IsValid(GEngine))
    { GEngine->Exec(World, *InCommand, *GLog); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoPassesFilter(
        const FString& InText) const
    -> bool
{
    if (_FilterString.IsEmpty())
    { return true; }

    return InText.Contains(_FilterString, ESearchCase::IgnoreCase);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoRebuildContent()
    -> void
{
    if (NOT _ContentText.IsValid())
    { return; }

    const auto& Snapshot = _Collector.Get_Snapshot();

    auto Out = FString{};

    Out += FString::Printf(TEXT("DIALOG REGISTRY    ready=%s    %d lines    %d banks\n"),
        Snapshot.IsReady ? TEXT("YES") : TEXT("no"),
        Snapshot.Lines.Num(),
        Snapshot.NumBanks);
    Out += TEXT("--------------------------------------------------------------------\n\n");

    // ---- Lines ----------------------------------------------------------------------------------------------------
    Out += FString::Printf(TEXT("LINES (%d)\n"), Snapshot.Lines.Num());
    for (const auto& Line : Snapshot.Lines)
    {
        if (NOT DoPassesFilter(Line.LineID.ToString()))
        { continue; }

        Out += FString::Printf(TEXT("  %-28s  enter=%s  exit=%s  conds=%d\n"),
            *Line.LineID.ToString(),
            *Line.EventTag.ToString(),
            Line.LinkedEventTag.IsValid() ? *Line.LinkedEventTag.ToString() : TEXT("(none)"),
            Line.NumConditions);
    }

    // ---- Emitters -------------------------------------------------------------------------------------------------
    Out += FString::Printf(TEXT("\nEMITTERS (%d)\n"), Snapshot.Emitters.Num());
    for (const auto& Emitter : Snapshot.Emitters)
    {
        if (NOT DoPassesFilter(Emitter.DebugName) && NOT DoPassesFilter(Emitter.EmitterTags.ToStringSimple()))
        { continue; }

        Out += FString::Printf(TEXT("\n  %s\n"), *Emitter.DebugName);
        Out += FString::Printf(TEXT("    tags: %s\n"),
            Emitter.EmitterTags.IsEmpty() ? TEXT("(global — no tags)") : *Emitter.EmitterTags.ToStringSimple());

        // Cooldowns are NOT printed here — they live in the metered section above, which is the only place that can
        // show progress rather than a bare number.
        if (Emitter.QueryHistory.Num() > 0)
        {
            const auto& Last = Emitter.QueryHistory.Last();
            Out += FString::Printf(TEXT("    last query: %s  ->  %d pass / %d fail(line) / %d fail(cooldown)\n"),
                *Last.EventTag.ToString(),
                Last.NumPassed, Last.NumFailLine, Last.NumFailEmitter);
        }
    }

    if (ck::Is_NOT_Valid(DoGet_PieWorld()))
    { Out = TEXT("(no active PIE session — start Play In Editor to inspect the Dialog registry)"); }

    _ContentText->SetText(FText::FromString(Out));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoMake_CooldownRow(
        const FCkDialogDebugger_CooldownInfo& InCooldown)
    -> TSharedRef<SWidget>
{
    // Remaining / total, so a full bar reads "just spent" and an empty one "about to come back". A cooldown started
    // with a zero (or negative) duration has no meaningful progress — show it full rather than dividing by zero.
    const auto Fraction = InCooldown.IsForever || InCooldown.TotalSeconds <= 0.0f
        ? 1.0f
        : FMath::Clamp(InCooldown.RemainingSeconds / InCooldown.TotalSeconds, 0.0f, 1.0f);

    // Warn while it is still withholding the line, Ok once it is nearly back. Forever is an Err: a line that never
    // returns is almost always a mistake in the caller, and the colour is what makes that jump out.
    const auto FillColor = InCooldown.IsForever
        ? CkStyle::Err()
        : (Fraction > 0.25f ? CkStyle::Warn() : CkStyle::Ok());

    const auto RemainingText = InCooldown.IsForever
        ? FString(TEXT("forever"))
        : FString::Printf(TEXT("%.2fs / %.2fs"), InCooldown.RemainingSeconds, InCooldown.TotalSeconds);

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(260.0f)
            [
                SNew(STextBlock)
                .Font(CkStyle::MonoFont(9))
                .ColorAndOpacity(CkStyle::Text())
                .Text(FText::FromName(InCooldown.LineID))
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SCkDebug_MeterBar)
            .Fraction(Fraction)
            .FillColor(FillColor)
            .DesiredSize(FVector2D(160.0f, 5.0f))
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Font(CkStyle::MonoFont(9))
            .ColorAndOpacity(CkStyle::TextDim())
            .Text(FText::FromString(RemainingText))
        ];
}

auto
    SCkDialogDebuggerWindow::
    DoRebuildCooldowns()
    -> void
{
    if (NOT _CooldownBox.IsValid())
    { return; }

    _CooldownBox->ClearChildren();

    auto NumCooling = 0;
    for (const auto& Emitter : _Collector.Get_Snapshot().Emitters)
    { NumCooling += Emitter.Cooldowns.Num(); }

    _CooldownBox->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("COOLDOWNS")))
            .CountText(FText::AsNumber(NumCooling))
            .SubText(FText::FromString(TEXT("lines currently withheld from their speaker")))
            .Underline(true)
        ];

    if (NumCooling == 0)
    {
        _CooldownBox->AddSlot()
            .AutoHeight()
            .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                .Font(CkStyle::MonoFont(9))
                .ColorAndOpacity(CkStyle::TextMute())
                .Text(FText::FromString(TEXT("(nothing cooling)")))
            ];
        return;
    }

    for (const auto& Emitter : _Collector.Get_Snapshot().Emitters)
    {
        if (Emitter.Cooldowns.IsEmpty())
        { continue; }

        // Filtered on the EMITTER, matching the text dump below — a filter that hid individual rows would leave an
        // emitter heading with no rows under it.
        if (NOT DoPassesFilter(Emitter.DebugName) && NOT DoPassesFilter(Emitter.EmitterTags.ToStringSimple()))
        { continue; }

        _CooldownBox->AddSlot()
            .AutoHeight()
            .Padding(CkStyle::SpaceM, CkStyle::SpaceM, 0.0f, CkStyle::SpaceXS)
            [
                SNew(STextBlock)
                .Font(CkStyle::BoldFont(9))
                .ColorAndOpacity(CkStyle::TextStrong())
                .Text(FText::FromString(Emitter.DebugName))
            ];

        for (const auto& Cooldown : Emitter.Cooldowns)
        {
            _CooldownBox->AddSlot()
                .AutoHeight()
                .Padding(CkStyle::SpaceXL, CkStyle::SpaceXS)
                [
                    DoMake_CooldownRow(Cooldown)
                ];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
