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
    // Locate an ActionInfo by handle within the selected top-level Planner's
    // legacy ActionSetInfo Catalog. Returns nullptr if not present.
    auto FindActionInCatalog_Breadcrumb(
        const FCkGoapDebugger_ActionSetInfo* InAs,
        const FCk_Handle_Goap_Action& InHandle) -> const FCkGoapDebugger_ActionInfo*
    {
        if (InAs == nullptr) { return nullptr; }
        return InAs->Catalog.FindByPredicate(
            [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == InHandle; });
    }

    // Compute the per-step depth label (T0, T1, T2 ...).
    auto TierLabel_Breadcrumb(int32 InTier) -> FString
    {
        return FString::Printf(TEXT("T%d"), InTier);
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

    // Structural hash. Captures every chain entry across every top-level
    // Planner + the selected planner handle (drives the highlight pip).
    auto NewHash = uint32{0};
    if (Snap != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(Snap->EntityHandle));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Snap->ActionSets.Num()));
        for (const auto& As : Snap->ActionSets)
        {
            NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(As.Handle)));
            NewHash = HashCombine(NewHash, ::GetTypeHash(As.ActiveChainHandles.Num()));
            for (const auto& H : As.ActiveChainHandles)
            { NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(H))); }
        }
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

    if (Snap == nullptr || Snap->ActionSets.Num() == 0)
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
    for (auto AsIdx = 0; AsIdx < Snap->ActionSets.Num(); ++AsIdx)
    {
        const auto& As = Snap->ActionSets[AsIdx];

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
        // the runtime active chain. Get_ActiveChain (legacy ActiveChainHandles)
        // already yields the Plan[0] walk starting from the root Action —
        // which IS the top-level Planner's underlying entity. So index 0 is
        // Tier 0; we don't synthesize an extra entry for the Planner itself.
        struct FCrumb
        {
            int32                   Tier         = 0;
            FString                 Name;
            FCk_Handle_Goap_Planner PlannerHandle;     // valid → clickable
            bool                    IsSelected   = false;
        };
        auto Crumbs = TArray<FCrumb>{};

        for (auto Step = 0; Step < As.ActiveChainHandles.Num(); ++Step)
        {
            const auto& StepHandle = As.ActiveChainHandles[Step];
            if (NOT ck::IsValid(StepHandle)) { continue; }

            const auto* StepInfo = FindActionInCatalog_Breadcrumb(&As, StepHandle);

            auto C = FCrumb{};
            C.Tier = Step;

            // Tier 0 — the top-level Planner. Prefer the Planner's DebugName
            // (which is the Planner's display name) over the raw class name
            // from the Catalog (the root Action's class is an implementation
            // detail at this level).
            if (Step == 0)
            {
                C.Name = As.DebugName.IsEmpty()
                    ? (StepInfo != nullptr ? StepInfo->ClassName : As.ActionSetTag.ToString())
                    : As.DebugName;
                C.PlannerHandle = As.Handle;
                C.IsSelected    = (As.Handle == SelPlanner);
            }
            else
            {
                C.Name = (StepInfo != nullptr && NOT StepInfo->ClassName.IsEmpty())
                    ? StepInfo->ClassName
                    : FString(TEXT("(unknown)"));

                // Dual-role steps are themselves Planners; build the typesafe
                // Planner handle from the shared FCk_Handle.
                if (StepInfo != nullptr && StepInfo->IsPlannerRole)
                {
                    const auto AsPlanner = ck::StaticCast<FCk_Handle_Goap_Planner>(static_cast<FCk_Handle>(StepHandle));
                    C.PlannerHandle = AsPlanner;
                    C.IsSelected    = (AsPlanner == SelPlanner);
                }
            }
            Crumbs.Add(MoveTemp(C));
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
