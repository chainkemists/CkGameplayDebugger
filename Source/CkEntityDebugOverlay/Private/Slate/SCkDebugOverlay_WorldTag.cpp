// Implements SCkDebugOverlay_WorldTag — a tiny one-line world-anchored label.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Text/STextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

auto
    SCkDebugOverlay_WorldTag::
    Construct(const FArguments& InArgs)
    -> void
{
    SetVisibility(EVisibility::HitTestInvisible);

    SAssignNew(_TextBlock, STextBlock)
        .Text(InArgs._Text)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
        .ColorAndOpacity(CkDebugStyle::TextStrong());

    ChildSlot
    [
        _TextBlock.ToSharedRef()
    ];
}

// ====================================================================================================================

auto
    SCkDebugOverlay_WorldTag::
    Set_Text(const FText& InText)
    -> void
{
    if (_TextBlock.IsValid())
    {
        _TextBlock->SetText(InText);
    }
}

// ====================================================================================================================
