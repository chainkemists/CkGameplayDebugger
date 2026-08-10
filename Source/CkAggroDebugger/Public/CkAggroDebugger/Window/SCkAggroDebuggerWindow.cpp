#include "CkAggroDebugger/Window/SCkAggroDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkAggroDebuggerWindow::WindowId = FName(TEXT("AggroDebugger"));

// --------------------------------------------------------------------------------------------------------------------

namespace ck_aggro_debugger_window
{
    constexpr auto k_NameColumnWidth   = 190.0f;
    constexpr auto k_ThreatMeterWidth  = 150.0f;
    constexpr auto k_ScoreMeterWidth   = 110.0f;
    constexpr auto k_ValueColumnWidth  = 86.0f;
    constexpr auto k_StateColumnWidth  = 150.0f;
    constexpr auto k_MeterHeight       = 7.0f;

    // RowDensity applies as a DELTA on this surface's own base padding: only the offset between
    // density options belongs to the axis, and Comfortable (the default) leaves the threat rows
    // exactly where they shipped. Clamped so Compact cannot produce negative margins.
    auto Apply_RowDensity(
        const FMargin& InBase)
        -> FMargin
    {
        const auto Baseline = ck::debug_axes::Get_RowPadding(FCkDebuggerStyleSelection{});
        const auto Current  = ck::debug_axes::Get_RowPadding(UCkDebuggerStyleSettings::Get_Selection());

        const auto DeltaX = Current.Left - Baseline.Left;
        const auto DeltaY = Current.Top  - Baseline.Top;

        return FMargin
        {
            FMath::Max(0.0f, InBase.Left   + DeltaX),
            FMath::Max(0.0f, InBase.Top    + DeltaY),
            FMath::Max(0.0f, InBase.Right  + DeltaX),
            FMath::Max(0.0f, InBase.Bottom + DeltaY)
        };
    }

    // Bars are normalised against the OWNER's strongest target, not an absolute ceiling. The threat clamp defaults to
    // 10000, so an absolute scale renders every real fight as a flat row of empty bars. Relative scaling makes the
    // top contender read full and every other bar a direct visual ratio against it — which is exactly the comparison
    // selection performs.
    auto Get_Fraction(
        float InValue,
        float InMax)
        -> float
    {
        if (InMax <= KINDA_SMALL_NUMBER)
        { return 0.0f; }

        return FMath::Clamp(InValue / InMax, 0.0f, 1.0f);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAggroDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
            .WindowId(WindowId)
            .ToolTabId(TEXT("CkAggroDebugger"))
            .DisplayName(FText::FromString(TEXT("CK Aggro Debugger")))
            .MenuActionsContent()
            [
                SNew(SCkDebug_IconToggle)
                .IconId(TEXT("Target"))
                .Label(FText::FromString(TEXT("Engaged owners only")))
                .ToolTip(FText::FromString(TEXT("Show only owners with an active tracked target.")))
                .IsOn_Lambda([this]() { return _ShowEngagedOwnersOnly; })
                .OnStateChanged_Lambda([this](const bool InEngagedOnly)
                {
                    if (_ShowEngagedOwnersOnly == InEngagedOnly) { return; }
                    _ShowEngagedOwnersOnly = InEngagedOnly;
                    _LastSignature.Reset();
                })
            ]
            .Content()
            [
                SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceXS)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceXL, 0.0f)
            [
                SAssignNew(_StatOwners, SCkDebug_StatPair)
                .Label(FText::FromString(TEXT("OWNERS")))
                .Value(FText::FromString(TEXT("0")))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceXL, 0.0f)
            [
                SAssignNew(_StatEngaged, SCkDebug_StatPair)
                .Label(FText::FromString(TEXT("ENGAGED")))
                .Value(FText::FromString(TEXT("0")))
                .ValueColor(FSlateColor(CkStyle::Err()))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(_StatTargets, SCkDebug_StatPair)
                .Label(FText::FromString(TEXT("TRACKED")))
                .Value(FText::FromString(TEXT("0")))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, CkStyle::SpaceXS)
        [
            SAssignNew(_StatusText, STextBlock)
            .Font(CkStyle::MonoFont(9))
            .ColorAndOpacity(CkStyle::TextMute())
            .Text(FText::FromString(TEXT("(waiting for a PIE session…)")))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceS)
        [
            SNew(SCkDebug_DualSearchBar)
            .FilterHintText(FText::FromString(TEXT("Filter owners/targets…")))
            .OnFilterTextChanged_Lambda([this](const FString& InText) { _FilterString = InText; })
            .OnHighlightTextChanged_Lambda([this](const FString& InText) { _HighlightString = InText; })
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SAssignNew(_OwnerBox, SVerticalBox)
            ]
        ]
            ]
    ];

    Register_WithGate();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAggroDebuggerWindow::
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

    if (const auto Signature = DoBuild_Signature();
        Signature != _LastSignature)
    {
        _LastSignature = Signature;
        DoRebuild_Structure();
    }

    DoUpdate_LiveValues();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAggroDebuggerWindow::
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
    SCkAggroDebuggerWindow::
    DoPassesFilter(
        const FString& InText) const
    -> bool
{
    if (_FilterString.IsEmpty())
    { return true; }

    return InText.Contains(_FilterString, ESearchCase::IgnoreCase);
}

auto
    SCkAggroDebuggerWindow::
    DoPassesFilter(
        const FCkAggroDebugger_OwnerInfo& InOwner) const
    -> bool
{
    if (_FilterString.IsEmpty())
    { return true; }

    // Match on the owner OR any of its targets, then show the whole table. Filtering individual rows away would
    // leave an owner heading with a partial table, and a threat table only means anything read whole — every bar is
    // scaled relative to its siblings.
    if (DoPassesFilter(InOwner.OwnerName))
    { return true; }

    for (const auto& Target : InOwner.Targets)
    {
        if (DoPassesFilter(Target.TrackedName))
        { return true; }
    }

    return false;
}

auto SCkAggroDebuggerWindow::DoPassesEngagedFilter(const FCkAggroDebugger_OwnerInfo& InOwner) const -> bool
{
    return NOT _ShowEngagedOwnersOnly || ck::IsValid(InOwner.ActiveTrackedEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAggroDebuggerWindow::
    DoGet_ThreatColor(
        const FCkAggroDebugger_TargetInfo& InTarget)
    -> FLinearColor
{
    // Colour encodes WHY the bar is moving the way it is, which is the question the numbers cannot answer at a
    // glance: red is the one being attacked, amber is a live contender refreshing its perception, muted is decaying
    // toward being forgotten.
    if (InTarget.IsPendingForget)
    { return CkStyle::TextMute(); }

    if (InTarget.IsActive)
    { return CkStyle::Err(); }

    if (InTarget.IsPerceived)
    { return CkStyle::Warn(); }

    return CkStyle::TextDim();
}

auto
    SCkAggroDebuggerWindow::
    DoBuild_StateText(
        const FCkAggroDebugger_TargetInfo& InTarget)
    -> FString
{
    auto State = FString{};

    if (InTarget.IsActive)          { State += TEXT("ACTIVE ");  }
    if (InTarget.IsPerceived)       { State += TEXT("SEEN ");    }
    if (InTarget.IsWithinRetention) { State += TEXT("RANGE ");   }
    if (InTarget.IsPendingForget)   { State += TEXT("FORGET ");  }
    if (InTarget.CannotBecomeActive){ State += TEXT("NOACTIVE ");}
    if (InTarget.CannotBeForgotten) { State += TEXT("PINNED ");  }

    return State.IsEmpty() ? FString(TEXT("--")) : State.TrimEnd();
}

auto
    SCkAggroDebuggerWindow::
    DoBuild_DetailText(
        const FCkAggroDebugger_TargetInfo& InTarget)
    -> FString
{
    const auto Rate = InTarget.Get_EffectiveDecayRate();

    // Time-to-forget is the single most useful derived number here: it answers "is this NPC about to stand down?",
    // which neither the threat value nor the decay rate answers alone.
    auto Forget = FString(TEXT("stable"));
    if (Rate > KINDA_SMALL_NUMBER)
    {
        const auto Seconds = InTarget.Get_SecondsToForget();
        Forget = Seconds <= 0.0f
            ? FString(TEXT("forgetting"))
            : FString::Printf(TEXT("~%.1fs left"), Seconds);
    }

    return FString::Printf(TEXT("d=%.0f/%.0f  -%.2f/s  %s  seen %.1fs ago"),
        InTarget.Distance,
        InTarget.RetentionDistance,
        Rate,
        *Forget,
        InTarget.SecondsSincePerceived);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAggroDebuggerWindow::
    DoBuild_Signature() const
    -> FString
{
    // Owner + tracked-target identity only. Threat, score and distance are deliberately absent: they change every
    // frame, and including them would rebuild the tree every frame — the exact failure this split exists to avoid.
    auto Signature = FString{};
    Signature.Append(_FilterString);
    Signature.Append(_ShowEngagedOwnersOnly ? TEXT("|engaged") : TEXT("|all"));

    for (const auto& Owner : _Collector.Get_Snapshot().Owners)
    {
        if (NOT DoPassesFilter(Owner) || NOT DoPassesEngagedFilter(Owner))
        { continue; }

        Signature.Appendf(TEXT("|%s>"), *Owner.OwnerName);
        for (const auto& Target : Owner.Targets)
        { Signature.Appendf(TEXT("%s,"), *Target.TrackedName); }
    }

    return Signature;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAggroDebuggerWindow::
    DoMake_TargetRow(
        const FCkAggroDebugger_TargetInfo& InTarget,
        FCkAggroDebugger_TargetSlot& OutSlot) const
    -> TSharedRef<SWidget>
{
    OutSlot.ThreatFraction = MakeShared<float>(0.0f);
    OutSlot.ScoreFraction  = MakeShared<float>(0.0f);
    OutSlot.ThreatColor    = MakeShared<FLinearColor>(DoGet_ThreatColor(InTarget));
    OutSlot.ScoreColor     = MakeShared<FLinearColor>(CkStyle::Accent());

    return SNew(SHorizontalBox)
        // Tracked entity — static for the row's lifetime (a change to it changes the signature and rebuilds).
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(ck_aggro_debugger_window::k_NameColumnWidth)
            [
                SNew(STextBlock)
                .Font(CkStyle::MonoFont(9))
                .ColorAndOpacity(InTarget.IsActive ? CkStyle::TextStrong() : CkStyle::Text())
                .Text(FText::FromString(InTarget.TrackedName))
                .ToolTipText(FText::FromString(InTarget.TrackedName))
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_MeterBar)
            .Fraction_Lambda([Cell = OutSlot.ThreatFraction]() { return *Cell; })
            .FillColor_Lambda([Cell = OutSlot.ThreatColor]() { return *Cell; })
            .DesiredSize(FVector2D(ck_aggro_debugger_window::k_ThreatMeterWidth, ck_aggro_debugger_window::k_MeterHeight))
            .ToolTipText(FText::FromString(TEXT("Threat, relative to this owner's strongest target")))
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(ck_aggro_debugger_window::k_ValueColumnWidth)
            [
                SAssignNew(OutSlot.ThreatText, STextBlock)
                .Font(CkStyle::MonoFont(9))
                .ColorAndOpacity(CkStyle::Text())
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_MeterBar)
            .Fraction_Lambda([Cell = OutSlot.ScoreFraction]() { return *Cell; })
            .FillColor_Lambda([Cell = OutSlot.ScoreColor]() { return *Cell; })
            .DesiredSize(FVector2D(ck_aggro_debugger_window::k_ScoreMeterWidth, ck_aggro_debugger_window::k_MeterHeight))
            .ToolTipText(FText::FromString(TEXT("Selection score — the quantity the argmax actually compares")))
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(ck_aggro_debugger_window::k_ValueColumnWidth)
            [
                SAssignNew(OutSlot.ScoreText, STextBlock)
                .Font(CkStyle::MonoFont(9))
                .ColorAndOpacity(CkStyle::TextDim())
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(ck_aggro_debugger_window::k_StateColumnWidth)
            [
                SAssignNew(OutSlot.StateText, STextBlock)
                .Font(CkStyle::MonoFont(9))
            ]
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SAssignNew(OutSlot.DetailText, STextBlock)
            .Font(CkStyle::MonoFont(9))
            .ColorAndOpacity(CkStyle::TextMute())
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAggroDebuggerWindow::
    DoRebuild_Structure()
    -> void
{
    if (NOT _OwnerBox.IsValid())
    { return; }

    _OwnerBox->ClearChildren();
    _TargetSlots.Reset();
    _OwnerSlots.Reset();

    auto AnyOwners = false;

    for (const auto& Owner : _Collector.Get_Snapshot().Owners)
    {
        if (NOT DoPassesFilter(Owner) || NOT DoPassesEngagedFilter(Owner))
        { continue; }

        AnyOwners = true;

        auto OwnerSlot = FCkAggroDebugger_OwnerSlot{};

        _OwnerBox->AddSlot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceXS)
            [
                SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(Owner.OwnerName))
                .Underline(true)
                .RightContent()
                [
                    SAssignNew(OwnerSlot.ActiveText, STextBlock)
                    .Font(CkStyle::MonoFont(9))
                    .ColorAndOpacity(CkStyle::Err())
                ]
            ];

        _OwnerBox->AddSlot()
            .AutoHeight()
            .Padding(ck_aggro_debugger_window::Apply_RowDensity(
                FMargin{CkStyle::SpaceS, 0.0f, 0.0f, CkStyle::SpaceXS}))
            [
                SAssignNew(OwnerSlot.MetaText, STextBlock)
                .Font(CkStyle::MonoFont(8))
                .ColorAndOpacity(CkStyle::TextMute())
            ];

        _OwnerSlots.Emplace(OwnerSlot);

        if (Owner.Targets.IsEmpty())
        {
            _OwnerBox->AddSlot()
                .AutoHeight()
                .Padding(ck_aggro_debugger_window::Apply_RowDensity(FMargin{CkStyle::SpaceXL, CkStyle::SpaceXS}))
                [
                    SNew(STextBlock)
                    .Font(CkStyle::MonoFont(9))
                    .ColorAndOpacity(CkStyle::TextMute())
                    .Text(FText::FromString(TEXT("(no tracked targets — idle)")))
                ];
            continue;
        }

        for (const auto& Target : Owner.Targets)
        {
            auto Slot = FCkAggroDebugger_TargetSlot{};
            const auto Row = DoMake_TargetRow(Target, Slot);
            _TargetSlots.Emplace(Slot);

            _OwnerBox->AddSlot()
                .AutoHeight()
                .Padding(ck_aggro_debugger_window::Apply_RowDensity(FMargin{CkStyle::SpaceXL, 1.0f}))
                [
                    Row
                ];
        }
    }

    if (AnyOwners)
    { return; }

    _OwnerBox->AddSlot()
        .AutoHeight()
        .Padding(ck_aggro_debugger_window::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS}))
        [
            SNew(STextBlock)
            .Font(CkStyle::MonoFont(9))
            .ColorAndOpacity(CkStyle::TextMute())
            .Text(FText::FromString(TEXT("(no Aggro owners in this world)")))
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAggroDebuggerWindow::
    DoUpdate_LiveValues()
    -> void
{
    const auto& Snapshot = _Collector.Get_Snapshot();

    if (_StatOwners.IsValid())  { _StatOwners->SetValue(FText::AsNumber(Snapshot.NumOwners)); }
    if (_StatEngaged.IsValid()) { _StatEngaged->SetValue(FText::AsNumber(Snapshot.NumEngaged)); }
    if (_StatTargets.IsValid()) { _StatTargets->SetValue(FText::AsNumber(Snapshot.NumTargets)); }

    if (_StatusText.IsValid())
    {
        // Aggro composes authority-side only, so "no owners" on a client is correct behaviour rather than a fault.
        // Saying so here stops the empty window from reading as a broken one.
        const auto Status = NOT Snapshot.HasWorld
            ? FString(TEXT("(no active PIE session — start Play In Editor to inspect the threat model)"))
            : (Snapshot.NumOwners == 0
                ? FString(TEXT("(no Aggro owners — note Aggro is authority-only, so a client PIE window shows none)"))
                : FString::Printf(TEXT("%d owner(s) · %d engaged · %d tracked target(s)"),
                    Snapshot.NumOwners, Snapshot.NumEngaged, Snapshot.NumTargets));

        _StatusText->SetText(FText::FromString(Status));
    }

    // Walks owners and targets in the same order the structure pass did, so slot N belongs to row N.
    auto TargetSlotIndex = 0;
    auto OwnerSlotIndex  = 0;

    for (const auto& Owner : Snapshot.Owners)
    {
        if (NOT DoPassesFilter(Owner) || NOT DoPassesEngagedFilter(Owner))
        { continue; }

        if (_OwnerSlots.IsValidIndex(OwnerSlotIndex))
        {
            const auto& OwnerSlot = _OwnerSlots[OwnerSlotIndex];

            if (OwnerSlot.ActiveText.IsValid())
            {
                OwnerSlot.ActiveText->SetText(FText::FromString(ck::IsValid(Owner.ActiveTrackedEntity)
                    ? FString::Printf(TEXT("▶ %s  (%.1fs)"), *Owner.ActiveTrackedName, Owner.SecondsActiveTargetHeld)
                    : FString(TEXT("idle"))));

                OwnerSlot.ActiveText->SetColorAndOpacity(ck::IsValid(Owner.ActiveTrackedEntity)
                    ? CkStyle::Err()
                    : CkStyle::TextMute());
            }

            if (OwnerSlot.MetaText.IsValid())
            {
                // The switch bar is the number that explains a switch that did NOT happen — a challenger must clear
                // the incumbent's score times bias times threshold. Printing it beside the score meters turns
                // "why is it still on that target?" into a direct comparison.
                const auto SwitchBar = Owner.Get_SwitchBarScore();
                auto Meta = FString::Printf(
                    TEXT("switch bar %.2f (bias %.2fx · thresh %.2fx)  ·  min score %.2f  ·  held %.1fs / min %.1fs  ·  since switch %.1fs / cd %.1fs  ·  evals %lld"),
                    SwitchBar,
                    Owner.CurrentTargetBias,
                    Owner.SwitchThreshold,
                    Owner.MinimumTargetScore,
                    Owner.SecondsActiveTargetHeld,
                    Owner.MinimumAggroDuration,
                    Owner.SecondsSinceSwitch,
                    Owner.SwitchCooldown,
                    Owner.EvaluationCount);

                if (Owner.MaxTrackedTargets > 0)
                { Meta += FString::Printf(TEXT("  ·  cap %d/%d"), Owner.Targets.Num(), Owner.MaxTrackedTargets); }

                if (Owner.IsDisabled)
                { Meta += TEXT("  ·  DISABLED"); }

                if (Owner.IsSelectionPending)
                { Meta += TEXT("  ·  selection pending"); }

                OwnerSlot.MetaText->SetText(FText::FromString(Meta));
            }
        }
        ++OwnerSlotIndex;

        const auto MaxThreat = Owner.Get_MaxThreat();
        const auto MaxScore  = Owner.Get_MaxScore();

        for (const auto& Target : Owner.Targets)
        {
            if (NOT _TargetSlots.IsValidIndex(TargetSlotIndex))
            { break; }

            const auto& Slot = _TargetSlots[TargetSlotIndex];
            ++TargetSlotIndex;

            // Writing the bound cells IS the update — the meters read them through their attributes on the next
            // paint, so nothing is invalidated or rebuilt.
            if (Slot.ThreatFraction.IsValid())
            { *Slot.ThreatFraction = ck_aggro_debugger_window::Get_Fraction(Target.Threat, MaxThreat); }

            if (Slot.ScoreFraction.IsValid())
            { *Slot.ScoreFraction = ck_aggro_debugger_window::Get_Fraction(Target.Score, MaxScore); }

            if (Slot.ThreatColor.IsValid())
            { *Slot.ThreatColor = DoGet_ThreatColor(Target); }

            if (Slot.ThreatText.IsValid())
            {
                Slot.ThreatText->SetText(FText::FromString(
                    FString::Printf(TEXT("%.1f / %.1f"), Target.Threat, Target.MinimumTrackedThreat)));
            }

            if (Slot.ScoreText.IsValid())
            { Slot.ScoreText->SetText(FText::FromString(FString::Printf(TEXT("s %.2f"), Target.Score))); }

            if (Slot.StateText.IsValid())
            {
                Slot.StateText->SetText(FText::FromString(DoBuild_StateText(Target)));
                Slot.StateText->SetColorAndOpacity(DoGet_ThreatColor(Target));
            }

            if (Slot.DetailText.IsValid())
            { Slot.DetailText->SetText(FText::FromString(DoBuild_DetailText(Target))); }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
