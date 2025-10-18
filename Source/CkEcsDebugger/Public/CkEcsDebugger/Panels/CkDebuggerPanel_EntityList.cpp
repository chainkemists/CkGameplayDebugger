#include "CkDebuggerPanel_EntityList.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
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

auto SCkDebuggerPanel_EntityList::Construct(
    const FArguments& InArgs,
    TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
    TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel) -> void
{
    SelectionModel = InSelectionModel;
    WorldModel = InWorldModel;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(new FSlateColorBrush(FCkDebuggerStyle::Color_Background_Dark))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                Build_WorldSelector()
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
                .BorderImage(new FSlateRoundedBoxBrush(
                    FCkDebuggerStyle::Color_Border,
                    2.0f,
                    FCkDebuggerStyle::Color_Background_Dark,
                    1.0f
                ))
                .Padding(0.0f)
                [
                    SAssignNew(EntityTree, SCkDebuggerWidget_EntityTree, SelectionModel, WorldModel)
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
}

auto SCkDebuggerPanel_EntityList::Build_WorldSelector() -> TSharedRef<SWidget>
{
    auto WorldSelectorContent = SNew(SHorizontalBox);

    if (WorldModel.IsValid())
    {
        const auto AvailableWorlds = WorldModel->Get_AvailableWorlds();

        for (auto Index = 0; Index < AvailableWorlds.Num(); ++Index)
        {
            const auto& World = AvailableWorlds[Index];

            WorldSelectorContent->AddSlot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "Button")
                .OnClicked(this, &SCkDebuggerPanel_EntityList::OnWorldButtonClicked, World)
                .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(ck::Format_UE(TEXT("{}"), World->GetNetMode())))
                    .ColorAndOpacity(this, &SCkDebuggerPanel_EntityList::Get_WorldButtonColor, World)
                    .Justification(ETextJustify::Center)
                ]
            ];
        }
    }

    return SNew(SBox)
        .Padding(FMargin(0.0f))
        [
            SNew(SBorder)
            .BorderImage(new FSlateColorBrush(FCkDebuggerStyle::Color_Background_Medium))
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
                    .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    WorldSelectorContent
                ]
            ]
        ];
}

auto SCkDebuggerPanel_EntityList::Build_Toolbar() -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(FMargin(0.0f))
        [
            SNew(SBorder)
            .BorderImage(new FSlateColorBrush(FCkDebuggerStyle::Color_Background_Dark))
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
                        .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
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
                        .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
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
                        .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
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
            .BorderImage(new FSlateColorBrush(FCkDebuggerStyle::Color_Background_Dark))
            .Padding(FMargin(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(this, &SCkDebuggerPanel_EntityList::Get_EntityCountText)
                    .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FCkDebuggerStyle::Padding_Large, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(this, &SCkDebuggerPanel_EntityList::Get_SelectionCountText)
                    .ColorAndOpacity(FCkDebuggerStyle::Color_Selection)
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

auto SCkDebuggerPanel_EntityList::OnWorldButtonClicked(UWorld* InWorld) -> FReply
{
    if (WorldModel.IsValid() && ck::IsValid(InWorld))
    {
        WorldModel->Set_SelectedWorld(InWorld);

        if (EntityTree.IsValid())
        {
            EntityTree->RefreshTree();
        }
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

auto SCkDebuggerPanel_EntityList::Get_WorldButtonColor(UWorld* InWorld) const -> FSlateColor
{
    if (NOT WorldModel.IsValid())
    { return FCkDebuggerStyle::Color_Text_Secondary; }

    const auto SelectedWorld = WorldModel->Get_SelectedWorld();

    if (SelectedWorld == InWorld)
    { return FCkDebuggerStyle::Color_Selection; }

    return FCkDebuggerStyle::Color_Text_Primary;
}