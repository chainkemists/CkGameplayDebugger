#include "CkObjectPoolingDebugger/Window/SCkObjectPoolingDebuggerWindow.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "Engine/World.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Local style constants (mirrors the other CK debugger windows)
// --------------------------------------------------------------------------------------------------------------------

namespace style
{
    static constexpr auto Pad_S = 4.0f;
    static constexpr auto Pad_M = 8.0f;

    static const auto Bg_Dark   = FLinearColor(0.01f, 0.01f, 0.01f);
    static const auto Bg_Medium = FLinearColor(0.025f, 0.025f, 0.025f);

    static const auto Text_Primary   = FLinearColor(0.85f, 0.85f, 0.85f);
    static const auto Text_Secondary = FLinearColor(0.6f, 0.6f, 0.6f);
    static const auto Text_Muted     = FLinearColor(0.35f, 0.35f, 0.35f);

    static const auto Accent_Cyan    = FLinearColor(0.51f, 0.69f, 1.0f);
    static const auto Accent_Warning = FLinearColor(1.0f, 0.8f, 0.01f);

    static auto Normal(int32 InSize = 9) -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Regular", InSize); }
    static auto Bold(int32 InSize = 9)   -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Bold", InSize); }

    // column widths — class is flexible, numeric columns fixed
    static constexpr auto Col_Class     = 0.34f;
    static constexpr auto Col_Archetype = 0.22f;
    static constexpr auto Col_Num       = 0.06f; // each numeric column
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_object_pooling_debugger_window
{
    // one flexible-width text cell
    static auto MakeCell(const FString& InText, float InFill, const FSlateFontInfo& InFont, const FLinearColor& InColor)
        -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(InFill).VAlign(VAlign_Center).Padding(style::Pad_S, 2.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(InText))
                    .Font(InFont)
                    .ColorAndOpacity(InColor)
            ];
    }
}

// ====================================================================================================================
// Construct
// ====================================================================================================================

const FName SCkObjectPoolingDebuggerWindow::WindowId = FName(TEXT("ObjectPoolingDebugger"));

auto
    SCkObjectPoolingDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

    _SummaryText = SNew(STextBlock)
        .Font(style::Bold(10))
        .ColorAndOpacity(style::Text_Primary);

    _TableBox = SNew(SVerticalBox);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(style::Bg_Medium)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()
                [ BuildToolbar() ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
                [ _SummaryText.ToSharedRef() ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_M, 0.0f)
                [ SNew(SSeparator).Thickness(1.0f) ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_S, style::Pad_S)
                [
                    SNew(SCkDebug_SectionHeader)
                        .Label(FText::FromString(TEXT("Pools (per class + archetype)")))
                ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_S, 0.0f)
                [ BuildHeaderRow() ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot().Padding(style::Pad_S)
                        [ _TableBox.ToSharedRef() ]
                ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------
// Toolbar
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(style::Bg_Dark)
        .Padding(FMargin(style::Pad_S))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_S, 0.0f)
                [
                    SNew(SCkDebug_WorldSelector, _WorldModel)
                        .ShowHeaderLabel(false)
                ]

            + SHorizontalBox::Slot().FillWidth(1.0f)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_M, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebugger_RefreshControls)
                        .WindowId(SCkObjectPoolingDebuggerWindow::WindowId)
                ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Header + rows
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    BuildHeaderRow()
    -> TSharedRef<SWidget>
{
    using ck_object_pooling_debugger_window::MakeCell;
    const auto& F = style::Bold(9);
    const auto& C = style::Text_Secondary;

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(style::Bg_Dark)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(style::Col_Class)     [ MakeCell(TEXT("Class"),     1.0f, F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Archetype) [ MakeCell(TEXT("Archetype"), 1.0f, F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(TEXT("Free"),      1.0f, F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(TEXT("InUse"),     1.0f, F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(TEXT("Live"),      1.0f, F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(TEXT("High"),      1.0f, F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(TEXT("Hits"),      1.0f, F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(TEXT("Miss"),      1.0f, F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(TEXT("Prewarm"),   1.0f, F, C) ]
        ];
}

auto
    SCkObjectPoolingDebuggerWindow::
    BuildPoolRow(
        const FCkObjectPoolingDebugger_PoolRow& InRow)
    -> TSharedRef<SWidget>
{
    using ck_object_pooling_debugger_window::MakeCell;
    const auto& F = style::Normal(9);
    const auto& Prim = style::Text_Primary;

    // a pool that has ever missed is worth a glance — tint the miss cell
    const auto MissColor = InRow.NumMisses > 0 ? style::Accent_Warning : style::Text_Muted;

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(style::Col_Class)     [ MakeCell(InRow.ClassName,                          1.0f, F, style::Accent_Cyan) ]
        + SHorizontalBox::Slot().FillWidth(style::Col_Archetype) [ MakeCell(InRow.ArchetypeName,                      1.0f, F, style::Text_Secondary) ]
        + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(FString::FromInt(InRow.NumFree),          1.0f, F, Prim) ]
        + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(FString::FromInt(InRow.NumInUse),         1.0f, F, Prim) ]
        + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(FString::FromInt(InRow.NumLiveInstances), 1.0f, F, Prim) ]
        + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(FString::FromInt(InRow.HighWaterMark),    1.0f, F, Prim) ]
        + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(FString::FromInt(InRow.NumHits),          1.0f, F, Prim) ]
        + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(FString::FromInt(InRow.NumMisses),        1.0f, F, MissColor) ]
        + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ MakeCell(FString::FromInt(InRow.NumPrewarmRemaining), 1.0f, F, Prim) ];
}

auto
    SCkObjectPoolingDebuggerWindow::
    RebuildTable(
        const FCkObjectPoolingDebugger_Snapshot& InSnapshot)
    -> void
{
    _TableBox->ClearChildren();

    if (NOT InSnapshot.HasSubsystem)
    {
        _TableBox->AddSlot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
            [
                SNew(STextBlock)
                    .Font(style::Normal())
                    .Text(FText::FromString(TEXT("No ObjectPooling subsystem for the selected world. Start PIE.")))
                    .ColorAndOpacity(style::Text_Muted)
            ];
        return;
    }

    if (InSnapshot.Pools.IsEmpty())
    {
        _TableBox->AddSlot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
            [
                SNew(STextBlock)
                    .Font(style::Normal())
                    .Text(FText::FromString(TEXT("No recycle pools created yet (nothing has been pooled).")))
                    .ColorAndOpacity(style::Text_Muted)
            ];
        return;
    }

    for (const auto& Row : InSnapshot.Pools)
    {
        _TableBox->AddSlot().AutoHeight()
            [ BuildPoolRow(Row) ];
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Tick
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _WorldModel->Ensure_AutoSelect();
    auto* World = _WorldModel->Get_SelectedWorld();

    const auto Snapshot = FCkObjectPoolingDebugger_Snapshot::Gather(World);

    RebuildTable(Snapshot);

    _SummaryText->SetText(FText::FromString(Snapshot.HasSubsystem
        ? ck::Format_UE(TEXT("{} pool(s)  ·  {} pinned-unique (DestroyOnRelease)"),
            Snapshot.Pools.Num(), Snapshot.NumPinnedUnique)
        : FString(TEXT("No active world."))));
}

// ====================================================================================================================
