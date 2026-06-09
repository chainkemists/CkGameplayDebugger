// Implements SCkDebugOverlay_WorldTag — a tiny one-line world-anchored label.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Layout/SBorder.h"
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

    // Dark rounded pill behind the text so it reads against any world background.
    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(CkDebugStyle::GetRoundedBrush())
            .BorderBackgroundColor(CkDebugStyle::OverlayOf(CkDebugStyle::BgRoot(), 0.78f))
            .Padding(FMargin{ CkDebugStyle::SpaceS, CkDebugStyle::SpaceXS })
            [
                _TextBlock.ToSharedRef()
            ]
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
