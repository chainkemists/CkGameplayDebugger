#include "CkGoapDebugger/Window/SCkGoapDebugger_PlanStrip.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"  // FCkGoapDebugger_NameParams
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================
// Internal helpers — file-prefixed (`_PlanStrip`) so Adaptive-Unity can't
// merge them with same-named helpers in sibling .cpp files.
// ====================================================================================================================

namespace
{
    // Locate an ActionInfo in the selected top-level Planner's legacy Catalog
    // by handle. Returns nullptr when absent. The legacy Catalog still carries
    // the per-Action class name + role flags we need to label and click into
    // plan-step cards while the dispatch hasn't migrated DataCollector yet.
    auto FindCatalogEntry_PlanStrip(
        const FCkGoapDebugger_ActionSetInfo* InAs,
        const FCk_Handle_Goap_Action& InHandle) -> const FCkGoapDebugger_ActionInfo*
    {
        if (InAs == nullptr || NOT ck::IsValid(InHandle)) { return nullptr; }
        return InAs->Catalog.FindByPredicate(
            [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == InHandle; });
    }

    // Find the top-level ActionSetInfo whose Catalog contains the given
    // Planner-or-Action handle. Used because the selected Planner may be a
    // sub-Planner whose Catalog isn't surfaced directly — falls back to the
    // top-level Planner's Catalog.
    auto FindOwningActionSet_PlanStrip(
        const FCkGoapDebugger_EntitySnapshot* InSnap,
        const FCk_Handle_Goap_Planner& InHandle) -> const FCkGoapDebugger_ActionSetInfo*
    {
        if (InSnap == nullptr || NOT ck::IsValid(InHandle)) { return nullptr; }
        for (const auto& As : InSnap->ActionSets)
        {
            if (As.Handle == InHandle) { return &As; }
            const auto AsAction = ck::StaticCast<FCk_Handle_Goap_Action>(static_cast<FCk_Handle>(InHandle));
            if (As.Catalog.ContainsByPredicate(
                [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == AsAction; }))
            { return &As; }
        }
        return nullptr;
    }
}

// ====================================================================================================================
// CONSTRUCT / DESTRUCT
// ====================================================================================================================

auto
    SCkGoapDebugger_PlanStrip::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    ChildSlot
    [
        SAssignNew(_ScrollBox, SScrollBox)
            .Orientation(Orient_Horizontal)
            + SScrollBox::Slot()
            [
                SAssignNew(_CardsBox, SHorizontalBox)
            ]
    ];

    RefreshFromViewModel();
}

SCkGoapDebugger_PlanStrip::~SCkGoapDebugger_PlanStrip() = default;

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_PlanStrip::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const auto* Planner = _ViewModel->GetSelectedPlannerInfo();

    // Structural hash — Plan content + selected Planner handle. Plan[0] gets
    // visually distinct treatment, so reorderings need a rebuild.
    auto NewHash = uint32{0};
    if (Planner != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(Planner->PlannerHandle)));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->PlanHandles.Num()));
        for (const auto& H : Planner->PlanHandles)
        { NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(H))); }
        for (const auto& Name : Planner->PlanClassNames)
        { NewHash = HashCombine(NewHash, GetTypeHash(Name)); }
        NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<uint8>(Planner->PlanStatus)));
    }
    // Re-render when name-depth verbosity changes — every card's label is
    // derived from the shared depth.
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth()));

    if (_HasMaterialized && NewHash == _LastStripHash) { return; }
    _LastStripHash = NewHash;
    _HasMaterialized = true;

    RebuildCards();
}

// ====================================================================================================================
// REBUILD
// ====================================================================================================================

auto
    SCkGoapDebugger_PlanStrip::
    RebuildCards()
    -> void
{
    if (NOT _CardsBox.IsValid() || NOT _ViewModel.IsValid()) { return; }

    _CardsBox->ClearChildren();

    const auto* Planner = _ViewModel->GetSelectedPlannerInfo();
    const auto* Snap    = _ViewModel->GetCurrentEntitySnapshot();

    if (Planner == nullptr)
    {
        _CardsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkGoapDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(TEXT("(no planner selected)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
        return;
    }

    if (Planner->PlanHandles.Num() == 0)
    {
        const auto IsFailure = Planner->PlanStatus == ECk_GoapPlanStatus::PlanFailed
                            || Planner->PlanStatus == ECk_GoapPlanStatus::CostThresholdReached;
        const auto Msg   = IsFailure
            ? FString(TEXT("✗ No path to goal from current WorldState."))
            : FString(TEXT("✓ Goal already satisfied; plan empty."));
        const auto Color = IsFailure
            ? FCkGoapDebuggerStyle::Color_Status_Failed
            : FCkGoapDebuggerStyle::Color_Status_PlanFound;

        _CardsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkGoapDebuggerStyle::Padding_Small)
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Surface)
                    .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(Msg))
                            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
                            .ColorAndOpacity(FSlateColor(Color))
                    ]
            ];
        return;
    }

    const auto* OwningAs = FindOwningActionSet_PlanStrip(Snap, Planner->PlannerHandle);
    const auto WeakVM   = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel);

    const auto StepCount = Planner->PlanHandles.Num();
    for (auto Idx = 0; Idx < StepCount; ++Idx)
    {
        const auto& StepHandle = Planner->PlanHandles[Idx];
        const auto  RawName    = Planner->PlanClassNames.IsValidIndex(Idx)
            ? Planner->PlanClassNames[Idx]
            : FString(TEXT("(unknown)"));
        const auto  Name       = FCkGoapDebugger_NameParams::ComputeDisplayName(
            RawName, _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1);

        const auto* StepInfo = FindCatalogEntry_PlanStrip(OwningAs, StepHandle);

        const auto IsActive       = (Idx == 0);
        const auto IsPlannerStep  = (StepInfo != nullptr && StepInfo->IsPlannerRole);
        const auto IsDualRole     = (StepInfo != nullptr && StepInfo->IsActionRole && StepInfo->IsPlannerRole);
        const auto Cost           = (StepInfo != nullptr) ? StepInfo->Cost : 0.0f;

        // Visual: composite (purple) for Planner-role steps, atomic (blue)
        // for action-only. Plan[0] gets a green "active" overlay border.
        const auto CardAccent = IsPlannerStep
            ? FCkGoapDebuggerStyle::Color_Status_Composite
            : FCkGoapDebuggerStyle::Color_Status_PlanningBdr;
        const auto BorderColor = IsActive
            ? FCkGoapDebuggerStyle::Color_Status_PlanFound
            : CardAccent;

        // Click handler: drill into the Planner if the step is one.
        const auto AsPlanner = IsPlannerStep
            ? ck::StaticCast<FCk_Handle_Goap_Planner>(static_cast<FCk_Handle>(StepHandle))
            : FCk_Handle_Goap_Planner{};

        // ---- Card content ------------------------------------------------------
        // Header row: step number + role badges.
        auto BadgeBox = SNew(SHorizontalBox);
        if (IsPlannerStep)
        {
            BadgeBox->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f))
                [
                    SNew(SCkDebug_StatusPill)
                        .Text(FText::FromString(TEXT("PLANNER")))
                        .Tone(ECkDebug_Tone::Accent)
                        .ShowDot(false)
                ];
        }
        if (StepInfo != nullptr && StepInfo->IsActionRole && (NOT IsPlannerStep || IsDualRole))
        {
            BadgeBox->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_StatusPill)
                        .Text(FText::FromString(TEXT("ACTION")))
                        .Tone(ECkDebug_Tone::Info)
                        .ShowDot(false)
                ];
        }

        // Optional ACTIVE pill — added to the top row only on Plan[0].
        auto TopRow = SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f))
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(FString::Printf(TEXT("%d"), Idx + 1)))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FSlateColor(CardAccent))
                ]
            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    BadgeBox
                ];

        // Note: no separate "ACTIVE" pill — the thick green border below
        // (BorderColor = Color_Status_PlanFound when IsActive) plus the bold
        // step name + green-tinted index "1" already signal "this is Plan[0]"
        // unambiguously, and squeezing a third pill onto the badge row
        // overflows narrow cards (see Mockup B screenshot feedback).

        auto CardBody = SNew(SVerticalBox)

            // Top row — step number + role badges + (optional) ACTIVE pill.
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    TopRow
                ]

            // Name
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall))
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(Name))
                        .Font(IsActive
                            ? FCoreStyle::GetDefaultFontStyle("Bold", 11)
                            : FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                ]

            // Kind + cost meta
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(FString::Printf(
                            TEXT("%s · cost $%d"),
                            IsPlannerStep ? TEXT("composite · sub-planner") : TEXT("atomic"),
                            FMath::RoundToInt(Cost))))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                ];

        // Two-layer border to render a tinted edge + dark surface.
        auto Card = SNew(SBox)
            .WidthOverride(IsActive ? 160.0f : 140.0f)
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(0.0f))
                    .IsEnabled(IsPlannerStep)
                    .ToolTipText(FText::FromString(IsPlannerStep
                        ? FString::Printf(TEXT("Click to inspect sub-Planner %s"), *Name)
                        : FString::Printf(TEXT("%s — atomic step (no drilldown)"), *Name)))
                    .OnClicked_Lambda([WeakVM, AsPlanner]() -> FReply
                    {
                        if (const auto VM = WeakVM.Pin())
                        {
                            if (ck::IsValid(AsPlanner))
                            { VM->SetSelectedActionSet(AsPlanner); }
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(BorderColor)
                            .Padding(FMargin(IsActive ? FCkGoapDebuggerStyle::Border_Strong
                                                      : FCkGoapDebuggerStyle::Border_Standard))
                            [
                                SNew(SBorder)
                                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                    .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Surface)
                                    .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                                     FCkGoapDebuggerStyle::Padding_Small))
                                    [
                                        CardBody
                                    ]
                            ]
                    ]
            ];

        _CardsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                Card
            ];

        // Arrow separator between cards.
        if (Idx + 1 < StepCount)
        {
            _CardsBox->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT(">")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Ghost))
                ];
        }
    }
}

// ====================================================================================================================
