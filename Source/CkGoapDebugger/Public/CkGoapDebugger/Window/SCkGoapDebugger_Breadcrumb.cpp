#include "CkGoapDebugger/Window/SCkGoapDebugger_Breadcrumb.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================
// Internal helpers — file-prefixed (`_Breadcrumb`) so the Adaptive-Unity build
// doesn't collide them with same-named helpers in other widget .cpp files.
// ====================================================================================================================

namespace
{
    // Compute the per-step depth label (T0, T1, T2 ...).
    auto TierLabel_Breadcrumb(int32 InTier) -> FString
    {
        return FString::Printf(TEXT("T%d"), InTier);
    }

    // Find the active child Planner under a PlannerInfo (the one tagged
    // IsInActiveChain = true). Returns nullptr if no active child.
    auto FindActiveChildPlanner_Breadcrumb(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
        -> const FCkGoapDebugger_PlannerInfo*
    {
        for (const auto& Child : InPlanner.ChildPlanners)
        {
            if (Child.IsInActiveChain) { return &Child; }
        }
        return nullptr;
    }

    // Recursive structural hash of a PlannerInfo subtree's active-chain trail.
    auto HashActiveTrail_Breadcrumb(
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        uint32& InOutHash) -> void
    {
        InOutHash = HashCombine(InOutHash, ::GetTypeHash(static_cast<FCk_Handle>(InPlanner.PlannerHandle)));
        InOutHash = HashCombine(InOutHash, ::GetTypeHash(InPlanner.PlanHandles.Num()));
        if (InPlanner.PlanHandles.Num() > 0)
        {
            InOutHash = HashCombine(InOutHash,
                ::GetTypeHash(static_cast<FCk_Handle>(InPlanner.PlanHandles[0])));
        }
        if (const auto* ActiveChild = FindActiveChildPlanner_Breadcrumb(InPlanner))
        { HashActiveTrail_Breadcrumb(*ActiveChild, InOutHash); }
    }
}

// ====================================================================================================================
// CONSTRUCT / DESTRUCT
// ====================================================================================================================

auto
    SCkGoapDebugger_Breadcrumb::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large, FCkGoapDebuggerStyle::Padding_Small))
            [
                SAssignNew(_RowsBox, SVerticalBox)
            ]
    ];

    RefreshFromViewModel();
}

SCkGoapDebugger_Breadcrumb::~SCkGoapDebugger_Breadcrumb() = default;

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_Breadcrumb::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const auto* Snap = _ViewModel->GetCurrentEntitySnapshot();
    const auto  SelPlanner = _ViewModel->GetSelectedActionSet();

    // Structural hash. Captures every active-chain entry across every
    // top-level Planner + the selected planner handle (drives the highlight pip).
    auto NewHash = uint32{0};
    if (Snap != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(Snap->EntityHandle));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Snap->TopLevelPlanners.Num()));
        for (const auto& Top : Snap->TopLevelPlanners)
        { HashActiveTrail_Breadcrumb(Top, NewHash); }
    }
    NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(SelPlanner)));

    if (_HasMaterialized && NewHash == _LastHash) { return; }
    _LastHash        = NewHash;
    _HasMaterialized = true;

    RebuildRows();
}

// ====================================================================================================================
// REBUILD
// ====================================================================================================================

auto
    SCkGoapDebugger_Breadcrumb::
    RebuildRows()
    -> void
{
    if (NOT _RowsBox.IsValid() || NOT _ViewModel.IsValid()) { return; }

    _RowsBox->ClearChildren();

    const auto* Snap = _ViewModel->GetCurrentEntitySnapshot();
    const auto  SelPlanner = _ViewModel->GetSelectedActionSet();

    if (Snap == nullptr || Snap->TopLevelPlanners.Num() == 0)
    {
        _RowsBox->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(no entity selected)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
        return;
    }

    const auto WeakVM = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel);

    // One row per top-level Planner on the entity.
    for (auto AsIdx = 0; AsIdx < Snap->TopLevelPlanners.Num(); ++AsIdx)
    {
        const auto& TopPlanner = Snap->TopLevelPlanners[AsIdx];

        auto Row = SNew(SHorizontalBox);

        // Per-row label — only on the first row, to mimic the mockup's
        // "ACTIVE CHAIN ·" lead-in. Subsequent rows get a thin spacer.
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(AsIdx == 0
                        ? FString(TEXT("ACTIVE CHAIN ·"))
                        : FString(TEXT("            ·"))))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
            ];

        // Build a flat list of (Tier, DisplayName, Handle) crumbs by walking
        // the active-chain trail. Mirrors the legacy ActiveChainHandles walk,
        // but sourced from the per-spec PlannerInfo forest:
        //   - Tier 0 = top-level Planner itself.
        //   - Each subsequent tier = the child Planner with IsInActiveChain.
        //   - Final tier (if PlanHandles[0] is a non-Planner atomic Action) =
        //     a non-clickable atomic leaf.
        struct FCrumb
        {
            int32                   Tier         = 0;
            FString                 Name;
            FCk_Handle_Goap_Planner PlannerHandle;     // valid → clickable
            bool                    IsSelected   = false;
        };
        auto Crumbs = TArray<FCrumb>{};

        // Walk the active-chain Planner trail.
        const FCkGoapDebugger_PlannerInfo* Cursor = &TopPlanner;
        auto Tier = 0;
        while (Cursor != nullptr)
        {
            auto C = FCrumb{};
            C.Tier          = Tier;
            C.Name          = Cursor->DisplayName.IsEmpty()
                ? Cursor->PlannerTag.ToString()
                : Cursor->DisplayName;
            C.PlannerHandle = Cursor->PlannerHandle;
            C.IsSelected    = (Cursor->PlannerHandle == SelPlanner);
            Crumbs.Add(MoveTemp(C));

            const auto* ActiveChild = FindActiveChildPlanner_Breadcrumb(*Cursor);
            if (ActiveChild != nullptr)
            {
                Cursor = ActiveChild;
                ++Tier;
                continue;
            }

            // No active child Planner — if the leaf Planner's current plan
            // step (PlanHandles[0]) resolves to a registered ChildAction that
            // is itself NOT a Planner (atomic), emit a final leaf crumb.
            // Dual-role children would have been picked up as ActiveChild
            // above, so this branch is atomic-only.
            if (Cursor->PlanHandles.Num() > 0)
            {
                const auto& LeafStep = Cursor->PlanHandles[0];
                if (ck::IsValid(LeafStep))
                {
                    const auto* LeafActionInfo = Cursor->ChildActions.FindByPredicate(
                        [&LeafStep](const FCkGoapDebugger_ActionInfo& In)
                        { return In.Handle == LeafStep; });

                    if (LeafActionInfo != nullptr && NOT LeafActionInfo->IsPlannerRole)
                    {
                        auto Leaf = FCrumb{};
                        Leaf.Tier = Tier + 1;
                        Leaf.Name = LeafActionInfo->ClassName.IsEmpty()
                            ? FString(TEXT("(unknown)"))
                            : LeafActionInfo->ClassName;
                        // No PlannerHandle → not clickable (atomic step).
                        Crumbs.Add(MoveTemp(Leaf));
                    }
                    else if (LeafActionInfo == nullptr)
                    {
                        // Plan step doesn't live in ChildActions (rare —
                        // grandchild Action under a sub-Planner). Show as
                        // unknown atomic so the trail isn't silently truncated.
                        auto Leaf = FCrumb{};
                        Leaf.Tier = Tier + 1;
                        Leaf.Name = FString(TEXT("(unknown)"));
                        Crumbs.Add(MoveTemp(Leaf));
                    }
                }
            }

            Cursor = nullptr;
        }

        // Emit pills + separators.
        for (auto CrumbIdx = 0; CrumbIdx < Crumbs.Num(); ++CrumbIdx)
        {
            const auto& Crumb = Crumbs[CrumbIdx];

            const auto CanSelect   = ck::IsValid(Crumb.PlannerHandle);
            const auto NameColor   = Crumb.IsSelected
                ? FCkGoapDebuggerStyle::Color_Status_Selected
                : (CanSelect ? FCkGoapDebuggerStyle::Color_Text_Primary
                             : FCkGoapDebuggerStyle::Color_Text_Secondary);
            const auto BorderBrush = Crumb.IsSelected
                ? FName(TEXT("CkGoap.Crumb.Selected"))
                : FName(TEXT("CkGoap.Crumb.Default"));

            const auto SegHandle = Crumb.PlannerHandle;

            auto Pill = SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(FCkGoapDebuggerStyle::Padding_Small, FCkGoapDebuggerStyle::Padding_XSmall))
                .IsEnabled(CanSelect)
                .ToolTipText(FText::FromString(CanSelect
                    ? FString::Printf(TEXT("Click to inspect %s"), *Crumb.Name)
                    : FString::Printf(TEXT("%s — atomic step"), *Crumb.Name)))
                .OnClicked_Lambda([WeakVM, SegHandle]() -> FReply
                {
                    if (const auto VM = WeakVM.Pin())
                    {
                        if (ck::IsValid(SegHandle))
                        { VM->SetSelectedActionSet(SegHandle); }
                    }
                    return FReply::Handled();
                })
                [
                    SNew(SBorder)
                        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(BorderBrush))
                        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_XSmall))
                        [
                            SNew(SHorizontalBox)

                                // Tier badge (T0 / T1 / ...)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                                    [
                                        SNew(SBorder)
                                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                            .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Surface)
                                            .Padding(FMargin(3.0f, 1.0f))
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TierLabel_Breadcrumb(Crumb.Tier)))
                                                    .Font(FCoreStyle::GetDefaultFontStyle("Mono", 7))
                                                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                                            ]
                                    ]

                                // Display name
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(Crumb.Name))
                                            .Font(Crumb.IsSelected
                                                ? FCoreStyle::GetDefaultFontStyle("Bold", 10)
                                                : FCoreStyle::GetDefaultFontStyle("Regular", 10))
                                            .ColorAndOpacity(FSlateColor(NameColor))
                                    ]
                        ]
                ];

            Row->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    Pill
                ];

            // Arrow separator between pills inside a row.
            if (CrumbIdx + 1 < Crumbs.Num())
            {
                Row->AddSlot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("▸")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Ghost))
                    ];
            }
        }

        _RowsBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_XSmall))
            [
                Row
            ];
    }
}

// ====================================================================================================================
