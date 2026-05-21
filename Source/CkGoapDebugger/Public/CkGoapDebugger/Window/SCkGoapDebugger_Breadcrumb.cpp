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
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_goap_debugger_breadcrumb_internal
{
    // Locate the catalog entry matching a chain handle in the selected
    // ActionSet. nullptr when not present.
    auto FindAction(
        const FCkGoapDebugger_ActionSetInfo* InAs,
        const FCk_Handle_Goap_Action& InHandle) -> const FCkGoapDebugger_ActionInfo*
    {
        if (InAs == nullptr) { return nullptr; }
        return InAs->Catalog.FindByPredicate(
            [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == InHandle; });
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
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
            [
                SNew(SHorizontalBox)

                    // "Chain:" label
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Chain:")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                        ]

                    // Segments box (rebuilt on change)
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(_SegmentsBox, SHorizontalBox)
                        ]

                    // Meta + Reset chain link
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FCkGoapDebuggerStyle::Padding_Medium, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                        [
                            SAssignNew(_MetaText, STextBlock)
                                .Text(FText::FromString(TEXT("")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Reset chain")))
                                .ToolTipText(FText::FromString(TEXT("Stub — wiring to utils_goap_action_set::Request_ResetActiveChain is a follow-up")))
                                .OnClicked(this, &SCkGoapDebugger_Breadcrumb::OnResetChainClicked)
                        ]
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

    using namespace ck_goap_debugger_breadcrumb_internal;

    const auto* AsInfo = _ViewModel->GetSelectedActionSetInfo();
    const auto  SelAction = _ViewModel->GetSelectedAction();

    // Compute a structural hash — only rebuild when the chain or selection
    // changes. Per-frame status text on the segments doesn't need a rebuild
    // (we render only the (name + tag) text per segment, both class-derived).
    auto NewHash = uint32{0};
    if (AsInfo != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(AsInfo->Handle)));
        NewHash = HashCombine(NewHash, ::GetTypeHash(AsInfo->ActiveChainHandles.Num()));
        NewHash = HashCombine(NewHash, ::GetTypeHash(AsInfo->Catalog.Num()));
        for (const auto& H : AsInfo->ActiveChainHandles)
        { NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(H))); }
    }
    NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(SelAction)));

    // Meta string is cheap to refresh every time.
    if (_MetaText.IsValid())
    {
        if (AsInfo != nullptr)
        {
            _MetaText->SetText(FText::FromString(FString::Printf(
                TEXT("Catalog: %d actions · Active: %d ·"),
                AsInfo->Catalog.Num(), AsInfo->ActiveChainHandles.Num())));
        }
        else
        {
            _MetaText->SetText(FText::FromString(TEXT("(no ActionSet selected) ·")));
        }
    }

    if (_HasMaterialized && NewHash == _LastSegmentsHash) { return; }
    _LastSegmentsHash = NewHash;
    _HasMaterialized  = true;

    RebuildSegments();
}

// ====================================================================================================================
// REBUILD
// ====================================================================================================================

auto
    SCkGoapDebugger_Breadcrumb::
    RebuildSegments()
    -> void
{
    using namespace ck_goap_debugger_breadcrumb_internal;

    if (NOT _SegmentsBox.IsValid() || NOT _ViewModel.IsValid()) { return; }

    _SegmentsBox->ClearChildren();

    const auto* AsInfo = _ViewModel->GetSelectedActionSetInfo();
    const auto  SelAction = _ViewModel->GetSelectedAction();

    if (AsInfo == nullptr)
    {
        _SegmentsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(no chain — select an ActionSet)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
        return;
    }

    if (AsInfo->ActiveChainHandles.Num() == 0)
    {
        _SegmentsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(no active chain)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
        return;
    }

    const auto WeakVM = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel);

    for (auto Idx = 0; Idx < AsInfo->ActiveChainHandles.Num(); ++Idx)
    {
        const auto Handle = AsInfo->ActiveChainHandles[Idx];
        const auto* Action = FindAction(AsInfo, Handle);

        const auto ClassName = (Action != nullptr && NOT Action->ClassName.IsEmpty())
            ? Action->ClassName
            : FString(TEXT("(unknown)"));
        const auto TagText   = (Action != nullptr && Action->ActionTag.IsValid())
            ? Action->ActionTag.ToString()
            : FString{};

        const auto IsSelected = (Handle == SelAction);

        const auto BrushName  = IsSelected
            ? FName(TEXT("CkGoap.Crumb.Selected"))
            : FName(TEXT("CkGoap.Crumb.Default"));

        const auto NameColor  = IsSelected
            ? FCkGoapDebuggerStyle::Color_Status_Selected
            : FCkGoapDebuggerStyle::Color_Text_Primary;

        const auto SegHandle = Handle;

        // Segment button
        _SegmentsBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(FCkGoapDebuggerStyle::Padding_Small, FCkGoapDebuggerStyle::Padding_XSmall))
                    .ToolTipText(FText::FromString(FString::Printf(
                        TEXT("Click to inspect %s"), *ClassName)))
                    .OnClicked_Lambda([WeakVM, SegHandle]() -> FReply
                    {
                        if (const auto VM = WeakVM.Pin())
                        { VM->SetSelectedAction(SegHandle); }
                        return FReply::Handled();
                    })
                    [
                        SNew(SBorder)
                            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(BrushName))
                            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(ClassName))
                                                .Font(IsSelected
                                                    ? FCoreStyle::GetDefaultFontStyle("Bold", 10)
                                                    : FCoreStyle::GetDefaultFontStyle("Regular", 10))
                                                .ColorAndOpacity(FSlateColor(NameColor))
                                        ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TagText))
                                                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 7))
                                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                                        ]
                            ]
                    ]
            ];

        // Arrow separator (skip after last segment)
        if (Idx + 1 < AsInfo->ActiveChainHandles.Num())
        {
            _SegmentsBox->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("▸")))   // ▸
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Ghost))
                ];
        }
    }
}

// ====================================================================================================================
// COMMANDS
// ====================================================================================================================

auto
    SCkGoapDebugger_Breadcrumb::
    OnResetChainClicked()
    -> FReply
{
    // D3 stub. Wiring this to utils_goap_action_set::Request_ResetActiveChain
    // requires an editor-safe path through the request layer; deferred.
    UE_LOG(LogTemp, Warning,
        TEXT("[CkGoapDebugger] Reset chain clicked — wiring deferred (see D3)."));
    return FReply::Handled();
}

// ====================================================================================================================
