#include "SCkDebug_Stepper.h"

#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
    SCkDebug_Stepper::
    Construct(const FArguments& InArgs)
    -> void
{
    _OnDelta = InArgs._OnDelta;

    ChildSlot
    [
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            DoMakeButton(FText::FromString(TEXT("−")), -1, InArgs._ButtonSize)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(3.0f, 0.0f, 0.0f, 0.0f)
        [
            DoMakeButton(FText::FromString(TEXT("+")), +1, InArgs._ButtonSize)
        ]
    ];
}

auto
    SCkDebug_Stepper::
    DoMakeButton(const FText& InGlyph, int32 InDelta, float InSize)
    -> TSharedRef<SWidget>
{
    const auto OnDelta = _OnDelta;

    auto Border = SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush_Small())
        .Padding(FMargin{1.0f});
    const auto BorderWeak = TWeakPtr<SBorder>{Border};

    Border->SetBorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([BorderWeak]
    {
        const auto Pinned = BorderWeak.Pin();
        return FSlateColor{Pinned.IsValid() && Pinned->IsHovered() ? CkStyle::Accent() : CkStyle::Border()};
    }));

    Border->SetContent(
        SNew(SBox)
        .WidthOverride(InSize)
        .HeightOverride(InSize)
        [
            SNew(SButton)
            .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
            .ContentPadding(FMargin{0.0f})
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .OnClicked_Lambda([OnDelta, InDelta]
            {
                OnDelta.ExecuteIfBound(InDelta);
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                .Text(InGlyph)
                .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity_Lambda([BorderWeak]
                {
                    const auto Pinned = BorderWeak.Pin();
                    return FSlateColor{Pinned.IsValid() && Pinned->IsHovered() ? CkStyle::Accent() : CkStyle::TextDim()};
                })
            ]
        ]);

    return Border;
}

// ====================================================================================================================
