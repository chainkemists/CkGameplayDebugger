#include "CkGoapDebugger/Window/SCkGoapDebugger_PlanStrip.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

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

namespace ck_goap_debugger_planstrip_internal
{
    // Look up a plan entry (by class name) in the ActionSet's catalog. Returns
    // nullptr when no match found.
    auto FindCatalogEntryByName(
        const FCkGoapDebugger_ActionSetInfo* InAs,
        const FString& InClassName) -> const FCkGoapDebugger_ActionInfo*
    {
        if (InAs == nullptr || InClassName.IsEmpty()) { return nullptr; }
        return InAs->Catalog.FindByPredicate(
            [&](const FCkGoapDebugger_ActionInfo& In) { return In.ClassName == InClassName; });
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

    const auto* Action = _ViewModel->GetSelectedActionInfo();
    const auto  SelAction = _ViewModel->GetSelectedAction();

    auto NewHash = uint32{0};
    if (Action != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(Action->Handle)));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Action->PlanClassNames.Num()));
        for (const auto& Name : Action->PlanClassNames)
        { NewHash = HashCombine(NewHash, GetTypeHash(Name)); }
    }
    NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(SelAction)));

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
    using namespace ck_goap_debugger_planstrip_internal;

    if (NOT _CardsBox.IsValid() || NOT _ViewModel.IsValid()) { return; }

    _CardsBox->ClearChildren();

    const auto* AsInfo = _ViewModel->GetSelectedActionSetInfo();
    const auto* Action = _ViewModel->GetSelectedActionInfo();
    const auto  SelAction = _ViewModel->GetSelectedAction();

    if (Action == nullptr)
    {
        _CardsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small))
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(no action selected)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
        return;
    }

    if (Action->PlanClassNames.Num() == 0)
    {
        _CardsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small))
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(empty plan — goal satisfied)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
        return;
    }

    const auto WeakVM = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel);

    // Cache the (planEntryName -> catalog match) lookup once per render.
    auto Matched = TArray<const FCkGoapDebugger_ActionInfo*>{};
    Matched.Reserve(Action->PlanClassNames.Num());
    for (const auto& Name : Action->PlanClassNames)
    { Matched.Add(FindCatalogEntryByName(AsInfo, Name)); }

    for (auto Idx = 0; Idx < Action->PlanClassNames.Num(); ++Idx)
    {
        const auto& ClassName = Action->PlanClassNames[Idx];
        const auto* Match = Matched[Idx];

        const auto IsComposite = (Match != nullptr && Match->ChildActionHandles.Num() > 0);
        const auto MatchHandle = (Match != nullptr) ? Match->Handle : FCk_Handle_Goap_Action{};
        const auto IsSelected  = ck::IsValid(MatchHandle) && (MatchHandle == SelAction);
        const auto Cost        = (Match != nullptr) ? Match->Cost : 0.0f;

        const auto CardColor = IsComposite
            ? FCkGoapDebuggerStyle::Color_Status_Composite
            : FCkGoapDebuggerStyle::Color_Status_PlanningBdr;
        const auto BorderColor = IsSelected
            ? FCkGoapDebuggerStyle::Color_Status_Selected
            : CardColor;

        auto LeafChildName = FString{};
        if (IsComposite && Match != nullptr && Match->ChildActionHandles.Num() > 0 && AsInfo != nullptr)
        {
            const auto FirstChild = Match->ChildActionHandles[0];
            if (const auto* ChildInfo = AsInfo->Catalog.FindByPredicate(
                    [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == FirstChild; }))
            {
                LeafChildName = FString(TEXT("▸ ")) + ChildInfo->ClassName;
            }
        }

        auto CardContent = SNew(SVerticalBox)
            // Step number (small badge text)
            + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("%d"), Idx + 1)))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        .ColorAndOpacity(FSlateColor(CardColor))
                ]
            // Class name
            + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(FMargin(0.0f, 2.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(ClassName))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                ]
            // Cost
            + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("$%d"), FMath::RoundToInt(Cost))))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                ];

        if (IsComposite && NOT LeafChildName.IsEmpty())
        {
            CardContent->AddSlot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(LeafChildName))
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 7))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Composite))
                ];
        }

        _CardsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                    .WidthOverride(140.0f)
                    [
                        SNew(SButton)
                            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                            .ContentPadding(FMargin(0.0f))
                            .IsEnabled(Match != nullptr)
                            .ToolTipText(FText::FromString(Match != nullptr
                                ? FString::Printf(TEXT("Click to inspect %s"), *ClassName)
                                : FString::Printf(TEXT("%s — not in current catalog"), *ClassName)))
                            .OnClicked_Lambda([WeakVM, MatchHandle]() -> FReply
                            {
                                if (const auto VM = WeakVM.Pin())
                                {
                                    if (ck::IsValid(MatchHandle))
                                    { VM->SetSelectedAction(MatchHandle); }
                                }
                                return FReply::Handled();
                            })
                            [
                                SNew(SBorder)
                                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                    .BorderBackgroundColor(BorderColor)
                                    .Padding(FMargin(FCkGoapDebuggerStyle::Border_Standard))
                                    [
                                        SNew(SBorder)
                                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                            .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Surface)
                                            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                                             FCkGoapDebuggerStyle::Padding_Small))
                                            [
                                                CardContent
                                            ]
                                    ]
                            ]
                    ]
            ];

        // Arrow separator
        if (Idx + 1 < Action->PlanClassNames.Num())
        {
            _CardsBox->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("▸")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Ghost))
                ];
        }
    }
}

// ====================================================================================================================
