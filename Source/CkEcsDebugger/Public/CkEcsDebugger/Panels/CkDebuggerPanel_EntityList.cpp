#include "CkDebuggerPanel_EntityList.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#include "CkEcsDebugger/Widgets/CkDebuggerWidget_SearchBar.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_EntityTree.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"

auto SCkDebuggerPanel_EntityList::Construct(
    const FArguments& InArgs,
    TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
    TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel) -> void
{
    SelectionModel = InSelectionModel;
    WorldModel = InWorldModel;

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            Build_Toolbar()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 0.0f)
        [
            SAssignNew(SearchBar, SCkDebuggerWidget_SearchBar)
            .OnSearchTextChanged(this, &SCkDebuggerPanel_EntityList::OnSearchTextChanged)
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(4.0f, 4.0f, 4.0f, 0.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
            .Padding(0.0f)
            [
                SAssignNew(EntityTree, SCkDebuggerWidget_EntityTree, SelectionModel, WorldModel)
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            Build_StatusBar()
        ]
    ];
}

auto SCkDebuggerPanel_EntityList::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
}

auto SCkDebuggerPanel_EntityList::Build_Toolbar() -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, 4.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .OnClicked(this, &SCkDebuggerPanel_EntityList::OnRefreshClicked)
            .ToolTipText(FText::FromString(TEXT("Refresh entity list")))
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush("Icons.Refresh"))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, 4.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .OnClicked(this, &SCkDebuggerPanel_EntityList::OnExpandAllClicked)
            .ToolTipText(FText::FromString(TEXT("Expand all nodes")))
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush("Icons.ChevronDown"))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .OnClicked(this, &SCkDebuggerPanel_EntityList::OnCollapseAllClicked)
            .ToolTipText(FText::FromString(TEXT("Collapse all nodes")))
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush("Icons.ChevronUp"))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNew(SBox)
        ];
}

auto SCkDebuggerPanel_EntityList::Build_StatusBar() -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(STextBlock)
            .Text(this, &SCkDebuggerPanel_EntityList::Get_EntityCountText)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(16.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(this, &SCkDebuggerPanel_EntityList::Get_SelectionCountText)
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