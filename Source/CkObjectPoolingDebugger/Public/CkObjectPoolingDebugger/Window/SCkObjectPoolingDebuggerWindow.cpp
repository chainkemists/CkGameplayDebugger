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
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

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

    static constexpr auto Col_Class     = 0.34f;
    static constexpr auto Col_Archetype = 0.22f;
    static constexpr auto Col_Num       = 0.06f;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_object_pooling_debugger_window
{
    static auto Cell(const FString& InText, const FSlateFontInfo& InFont, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(FText::FromString(InText))
            .Font(InFont)
            .ColorAndOpacity(InColor)
            .Margin(FMargin(style::Pad_S, 2.0f));
    }
}

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

    _SummaryText = SNew(STextBlock).Font(style::Bold(10)).ColorAndOpacity(style::Text_Primary);

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

            + SVerticalBox::Slot().FillHeight(1.0f).Padding(style::Pad_S, 0.0f)
                [
                    SAssignNew(_ListView, SListView<ItemPtr>)
                        .ListItemsSource(&_Items)
                        .OnGenerateRow(this, &SCkObjectPoolingDebuggerWindow::OnGenerateRow)
                        .SelectionMode(ESelectionMode::None)
                ]
        ]
    ];
}

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
                [ SNew(SCkDebug_WorldSelector, _WorldModel).ShowHeaderLabel(false) ]

            + SHorizontalBox::Slot().FillWidth(1.0f)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_M, 0.0f, 0.0f, 0.0f)
                [ SNew(SCkDebugger_RefreshControls).WindowId(SCkObjectPoolingDebuggerWindow::WindowId) ]
        ];
}

auto
    SCkObjectPoolingDebuggerWindow::
    BuildHeaderRow()
    -> TSharedRef<SWidget>
{
    using ck_object_pooling_debugger_window::Cell;
    const auto& F = style::Bold(9);
    const auto& C = style::Text_Secondary;

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(style::Bg_Dark)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(style::Col_Class)     [ Cell(TEXT("Class"),     F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Archetype) [ Cell(TEXT("Archetype"), F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(TEXT("Free"),      F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(TEXT("InUse"),     F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(TEXT("Live"),      F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(TEXT("High"),      F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(TEXT("Hits"),      F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(TEXT("Miss"),      F, C) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(TEXT("Prewarm"),   F, C) ]
        ];
}

auto
    SCkObjectPoolingDebuggerWindow::
    OnGenerateRow(
        ItemPtr InItem,
        const TSharedRef<STableViewBase>& InTable)
    -> TSharedRef<ITableRow>
{
    using ck_object_pooling_debugger_window::Cell;
    const auto& F = style::Normal(9);
    const auto& Prim = style::Text_Primary;

    const auto& Row = *InItem;
    const auto MissColor = Row.NumMisses > 0 ? style::Accent_Warning : style::Text_Muted;

    return SNew(STableRow<ItemPtr>, InTable)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(style::Col_Class)     [ Cell(Row.ClassName,                          F, style::Accent_Cyan) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Archetype) [ Cell(Row.ArchetypeName,                      F, style::Text_Secondary) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(FString::FromInt(Row.NumFree),          F, Prim) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(FString::FromInt(Row.NumInUse),         F, Prim) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(FString::FromInt(Row.NumLiveInstances), F, Prim) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(FString::FromInt(Row.HighWaterMark),    F, Prim) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(FString::FromInt(Row.NumHits),          F, Prim) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(FString::FromInt(Row.NumMisses),        F, MissColor) ]
            + SHorizontalBox::Slot().FillWidth(style::Col_Num)       [ Cell(FString::FromInt(Row.NumPrewarmRemaining), F, Prim) ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    MakeSignature(
        const FCkObjectPoolingDebugger_Snapshot& InSnapshot)
    -> FString
{
    // pool SET identity only (class+archetype) — NOT stats, so scroll survives a stats-only refresh
    auto Sig = FString{};
    Sig.Reserve(InSnapshot.Pools.Num() * 32);
    for (const auto& Pool : InSnapshot.Pools)
    {
        Sig += Pool.ClassName;
        Sig += TEXT("|");
        Sig += Pool.ArchetypeName;
        Sig += TEXT(";");
    }
    return Sig;
}

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
    const auto Snapshot = FCkObjectPoolingDebugger_Snapshot::Gather(_WorldModel->Get_SelectedWorld());

    if (const auto Signature = MakeSignature(Snapshot);
        Signature != _LastSignature)
    {
        // pool set changed — rebuild the item objects
        _LastSignature = Signature;
        _Items.Reset(Snapshot.Pools.Num());
        for (const auto& Pool : Snapshot.Pools)
        { _Items.Emplace(MakeShared<FCkObjectPoolingDebugger_PoolRow>(Pool)); }
        _ListView->RequestListRefresh();
    }
    else
    {
        // same pools — update stats in place (both are sorted, so indices align) and redraw
        const auto Num = FMath::Min(_Items.Num(), Snapshot.Pools.Num());
        for (auto Index = 0; Index < Num; ++Index)
        { *_Items[Index] = Snapshot.Pools[Index]; }
        _ListView->RequestListRefresh();
    }

    _SummaryText->SetText(FText::FromString(Snapshot.HasSubsystem
        ? ck::Format_UE(TEXT("{} pool(s)  ·  {} pinned-unique (DestroyOnRelease)"),
            Snapshot.Pools.Num(), Snapshot.NumPinnedUnique)
        : FString(TEXT("No ObjectPooling subsystem for the selected world. Start PIE."))));
}

// ====================================================================================================================
