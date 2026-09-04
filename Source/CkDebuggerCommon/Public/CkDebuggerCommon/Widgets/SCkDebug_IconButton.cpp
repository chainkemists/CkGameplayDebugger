#include "SCkDebug_IconButton.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"

#include "CkEditorTools/Style/CkIconStyle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_IconButton::Construct(const FArguments& InArgs) -> void
{
    const auto* IconBrush = FCkIconStyle::Get_Brush(InArgs._IconId, ECk_Icon_BrushSize::Size_16x16);
    const auto IconIsValid = InArgs._IconId != ECk_Icon::None && IconBrush != nullptr;
    CK_ENSURE_IF_NOT(IconIsValid, TEXT("Missing common debugger icon [{}] for icon button"), InArgs._IconId)
    {
        ChildSlot[SNullWidget::NullWidget];
        return;
    }

    ChildSlot
    .HAlign(HAlign_Left)
    .VAlign(VAlign_Center)
    [
        SNew(SButton)
        .ContentPadding(FMargin{CkStyle::SpaceS, 1.0f})
        .ToolTipText(InArgs._Label)
        .OnClicked(InArgs._OnClicked)
        .IsEnabled(InArgs._IsEnabled)
        [
            SNew(SCkDebug_Icon)
            .Brush(IconBrush)
            .Meaning(InArgs._Label)
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------
