#include "SCkDebug_SelectableLabel.h"

#include "Widgets/Input/SEditableText.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_SelectableLabel::Construct(const FArguments& InArgs) -> void
{
    ChildSlot
    [
        SAssignNew(_Editable, SEditableText)
        .Text(InArgs._Text)
        .Font(InArgs._Font)
        .ColorAndOpacity(InArgs._ColorAndOpacity)
        .IsReadOnly(true)
    ];
}

auto SCkDebug_SelectableLabel::SetText(const FText& InText) -> void
{
    if (_Editable.IsValid())
    {
        _Editable->SetText(InText);
    }
}
