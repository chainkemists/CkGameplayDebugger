#include "SCkDebug_DualSearchBar.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_DualSearchBar::Construct(const FArguments& InArgs) -> void
{
    _OnFilterChanged    = InArgs._OnFilterTextChanged;
    _OnHighlightChanged = InArgs._OnHighlightTextChanged;

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(_FilterBox, SSearchBox)
            .HintText(InArgs._FilterHintText)
            .OnTextChanged_Lambda([this](const FText& InText)
            {
                _OnFilterChanged.ExecuteIfBound(InText.ToString());
            })
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SAssignNew(_HighlightBox, SSearchBox)
            .HintText(InArgs._HighlightHintText)
            .OnTextChanged_Lambda([this](const FText& InText)
            {
                _OnHighlightChanged.ExecuteIfBound(InText.ToString());
            })
        ]
    ];
}

auto SCkDebug_DualSearchBar::Get_FilterText() const -> FString
{
    return _FilterBox.IsValid() ? _FilterBox->GetText().ToString() : FString{};
}

auto SCkDebug_DualSearchBar::Get_HighlightText() const -> FString
{
    return _HighlightBox.IsValid() ? _HighlightBox->GetText().ToString() : FString{};
}
