#include "SCkDebug_NameDepthCycler.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
    SCkDebug_NameDepthCycler::
    Construct(const FArguments& InArgs)
    -> void
{
    _Depth          = InArgs._Depth;
    _MaxDepth       = InArgs._MaxDepth;
    _OnDepthChanged = InArgs._OnDepthChanged;

    ChildSlot
    [
        SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceS, 0.0f})
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Name")))
                        .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                        .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                ]

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("\x25C0")))   // ◀
                        .ToolTipText(FText::FromString(TEXT("Shorter display name (fewer segments)")))
                        .OnClicked(FOnClicked::CreateSP(this, &SCkDebug_NameDepthCycler::DoCycle, -1))
                ]

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text_Lambda([this]() -> FText
                        {
                            const auto D = _Depth.Get(1);
                            return FText::FromString(D == 0 ? TEXT("Full") : FString::Printf(TEXT("%d"), D));
                        })
                        .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                        .Justification(ETextJustify::Center)
                        .MinDesiredWidth(28.0f)
                        .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                ]

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("\x25B6")))   // ▶
                        .ToolTipText(FText::FromString(TEXT("Longer display name (more segments)")))
                        .OnClicked(FOnClicked::CreateSP(this, &SCkDebug_NameDepthCycler::DoCycle, 1))
                ]
    ];
}

auto
    SCkDebug_NameDepthCycler::
    DoCycle(int32 InDirection)
    -> FReply
{
    const auto Depth    = _Depth.Get(1);
    const auto MaxDepth = FMath::Max(1, _MaxDepth.Get(1));

    const auto NewDepth = InDirection < 0
        ? (Depth == 0 ? MaxDepth : Depth - 1)          // Full(0) → Max → … → 1 → Full(0)
        : (Depth >= MaxDepth ? 0 : Depth + 1);         // Full(0) → 1 → … → Max → Full(0)

    if (_OnDepthChanged.IsBound())
    { _OnDepthChanged.Execute(NewDepth); }

    return FReply::Handled();
}

// ====================================================================================================================
