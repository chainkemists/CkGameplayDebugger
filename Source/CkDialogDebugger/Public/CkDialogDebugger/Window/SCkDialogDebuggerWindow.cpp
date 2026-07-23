#include "CkDialogDebugger/Window/SCkDialogDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
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

        for (const auto& Cooldown : Emitter.Cooldowns)
        {
            Out += FString::Printf(TEXT("    cooldown: %-24s %s\n"),
                *Cooldown.LineID.ToString(),
                Cooldown.IsForever ? TEXT("forever") : *FString::Printf(TEXT("%.2fs remaining"), Cooldown.RemainingSeconds));
        }

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
