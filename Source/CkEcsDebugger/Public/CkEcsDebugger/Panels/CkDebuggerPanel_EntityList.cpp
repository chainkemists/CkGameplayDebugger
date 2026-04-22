#include "CkDebuggerPanel_EntityList.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_SearchBar.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_EntityTree.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
auto SCkDebuggerPanel_EntityList::Construct(
    const FArguments& InArgs,
    TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
    TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel,
    TSharedPtr<FCkDebuggerModel_InspectorFilter> InFilterModel) -> void
{
    SelectionModel = InSelectionModel;
    WorldModel = InWorldModel;

    SetClipping(EWidgetClipping::ClipToBounds);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SAssignNew(WorldSelectorContainer, SBox)
                [
                    Build_WorldSelector()
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                Build_Toolbar()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SAssignNew(SearchBar, SCkDebuggerWidget_SearchBar)
                .OnSearchTextChanged(this, &SCkDebuggerPanel_EntityList::OnSearchTextChanged)
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SBorder)
                .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border"))
                .Padding(0.0f)
                [
                    SAssignNew(EntityTree, SCkDebuggerWidget_EntityTree, SelectionModel, WorldModel, InFilterModel)
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                Build_StatusBar()
            ]
        ]
    ];
}

auto SCkDebuggerPanel_EntityList::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    // Honour the user's refresh settings — skip world-count polling while the
    // tab is hidden (OnlyWhenVisible mode) or faster than the rate cap allows.
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(SCkDebuggerWindow_Main::WindowId))
    { return; }

    TimeSinceWorldCheck += InDeltaTime;
    if (TimeSinceWorldCheck < WorldCheckInterval || NOT WorldModel.IsValid())
    { return; }

    TimeSinceWorldCheck = 0.0f;

    // Detect changes by IDENTITY, not just count. A quick PIE stop→restart can
    // produce the same count with a different UWorld* — count-only gating would
    // miss it and leave a stale TWeakObjectPtr selected.
    const auto AvailableWorlds = WorldModel->Get_AvailableWorlds();

    auto WorldsChanged = AvailableWorlds.Num() != LastKnownWorlds.Num();
    if (NOT WorldsChanged)
    {
        for (auto Index = 0; Index < AvailableWorlds.Num(); ++Index)
        {
            if (LastKnownWorlds[Index].Get() != AvailableWorlds[Index])
            {
                WorldsChanged = true;
                break;
            }
        }
    }

    if (NOT WorldsChanged)
    { return; }

    LastKnownWorlds.Reset(AvailableWorlds.Num());
    for (auto* World : AvailableWorlds)
    {
        LastKnownWorlds.Emplace(World);
    }

    // If the previously-selected world was destroyed, the weak ptr now returns null.
    // Clear any stale entity selection before auto-selecting a replacement.
    if (NOT WorldModel->Get_SelectedWorld() && SelectionModel.IsValid())
    {
        SelectionModel->Clear_Selection();
        WorldModel->Set_SelectedWorld(nullptr);
    }

    if (AvailableWorlds.Num() > 0 && NOT WorldModel->Get_SelectedWorld())
    {
        WorldModel->Set_SelectedWorld(AvailableWorlds[0]);
        if (EntityTree.IsValid())
        {
            EntityTree->RefreshTree();
        }
    }

    // Rebuild world selector AFTER auto-select so the active state is correct
    if (WorldSelectorContainer.IsValid())
    {
        WorldSelectorContainer->SetContent(Build_WorldSelector());
    }
}

auto SCkDebuggerPanel_EntityList::Build_WorldSelector() -> TSharedRef<SWidget>
{
    auto ButtonRow = SNew(SHorizontalBox);

    if (WorldModel.IsValid())
    {
        const auto AvailableWorlds = WorldModel->Get_AvailableWorlds();

        for (auto Index = 0; Index < AvailableWorlds.Num(); ++Index)
        {
            const auto& World = AvailableWorlds[Index];

            if (Index > 0)
            {
                ButtonRow->AddSlot()
                .AutoWidth()
                .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                [
                    SNullWidget::NullWidget
                ];
            }

            const auto bIsSelected = WorldModel->Get_SelectedWorld() == World;
            const TWeakObjectPtr<UWorld> WorldWeak(World);

            ButtonRow->AddSlot()
            .FillWidth(1.0f)
            [
                SNew(SBorder)
                .BorderImage(bIsSelected
                    ? FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border")
                    : FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
                .Padding(FMargin(2.0f))
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "Button")
                    .OnClicked(this, &SCkDebuggerPanel_EntityList::OnWorldButtonClicked, WorldWeak)
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small))
                    .HAlign(HAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(ck::Format_UE(TEXT("{}"), World->GetNetMode())))
                        .ColorAndOpacity(bIsSelected
                            ? CkDebugStyle::Selection()
                            : CkDebugStyle::Text())
                        .Justification(ETextJustify::Center)
                        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]
                ]
            ];
        }
    }

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Medium"))
        .Padding(FMargin(FCkDebuggerStyle::Padding_Small))
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold"))
                .Text(FText::FromString(TEXT("World Selection")))
                .ColorAndOpacity(CkDebugStyle::TextDim())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ButtonRow
            ]
        ];
}

auto SCkDebuggerPanel_EntityList::Build_Toolbar() -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(FMargin(0.0f))
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
            .Padding(FMargin(FCkDebuggerStyle::Padding_Small))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .OnClicked(this, &SCkDebuggerPanel_EntityList::OnRefreshClicked)
                    .ToolTipText(FText::FromString(TEXT("Refresh entity list")))
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Refresh"))
                        .ColorAndOpacity(CkDebugStyle::TextDim())
                        .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .OnClicked(this, &SCkDebuggerPanel_EntityList::OnExpandAllClicked)
                    .ToolTipText(FText::FromString(TEXT("Expand all nodes")))
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.ChevronDown"))
                        .ColorAndOpacity(CkDebugStyle::TextDim())
                        .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .OnClicked(this, &SCkDebuggerPanel_EntityList::OnCollapseAllClicked)
                    .ToolTipText(FText::FromString(TEXT("Collapse all nodes")))
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.ChevronUp"))
                        .ColorAndOpacity(CkDebugStyle::TextDim())
                        .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                    ]
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SBox)
                ]
            ]
        ];
}

auto SCkDebuggerPanel_EntityList::Build_StatusBar() -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(FMargin(0.0f))
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
            .Padding(FMargin(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(this, &SCkDebuggerPanel_EntityList::Get_EntityCountText)
                    .ColorAndOpacity(CkDebugStyle::TextDim())
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FCkDebuggerStyle::Padding_Large, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(this, &SCkDebuggerPanel_EntityList::Get_SelectionCountText)
                    .ColorAndOpacity(CkDebugStyle::Selection())
                ]
            ]
        ];
}

auto SCkDebuggerPanel_EntityList::OnSearchTextChanged(const FString& InSearchText) -> void
{
    if (EntityTree.IsValid())
    {
        EntityTree->ApplyFilter(InSearchText);
    }
}

auto SCkDebuggerPanel_EntityList::OnRefreshClicked() -> FReply
{
    if (WorldModel.IsValid())
    {
        WorldModel->MarkCacheDirty();
    }

    if (EntityTree.IsValid())
    {
        EntityTree->RefreshTree();
    }

    return FReply::Handled();
}

auto SCkDebuggerPanel_EntityList::OnExpandAllClicked() -> FReply
{
    if (EntityTree.IsValid())
    {
        EntityTree->ExpandAll();
    }

    return FReply::Handled();
}

auto SCkDebuggerPanel_EntityList::OnCollapseAllClicked() -> FReply
{
    if (EntityTree.IsValid())
    {
        EntityTree->CollapseAll();
    }

    return FReply::Handled();
}

auto SCkDebuggerPanel_EntityList::OnWorldButtonClicked(TWeakObjectPtr<UWorld> InWorldWeak) -> FReply
{
    auto* InWorld = InWorldWeak.Get();
    if (NOT WorldModel.IsValid() || NOT InWorld)
    { return FReply::Handled(); }

    const auto PreviousWorld = WorldModel->Get_SelectedWorld();
    WorldModel->Set_SelectedWorld(InWorld);

    // Clear entity selection when switching worlds — previous entities are stale
    if (PreviousWorld != InWorld && SelectionModel.IsValid())
    {
        SelectionModel->Clear_Selection();
    }

    // Rebuild the world selector to reflect the new active button
    if (WorldSelectorContainer.IsValid())
    {
        WorldSelectorContainer->SetContent(Build_WorldSelector());
    }

    if (EntityTree.IsValid())
    {
        EntityTree->RefreshTree();
    }

    return FReply::Handled();
}

auto SCkDebuggerPanel_EntityList::Get_EntityCountText() const -> FText
{
    if (NOT WorldModel.IsValid())
    { return FText::FromString(TEXT("Entities: 0")); }

    const auto EntityCount = WorldModel->Get_CachedEntities().Num();
    return FText::FromString(ck::Format_UE(TEXT("Entities: {}"), EntityCount));
}

auto SCkDebuggerPanel_EntityList::Get_SelectionCountText() const -> FText
{
    if (NOT SelectionModel.IsValid())
    { return FText::FromString(TEXT("Selected: 0")); }

    const auto SelectionCount = SelectionModel->Get_SelectionCount();
    return FText::FromString(ck::Format_UE(TEXT("Selected: {}"), SelectionCount));
}

