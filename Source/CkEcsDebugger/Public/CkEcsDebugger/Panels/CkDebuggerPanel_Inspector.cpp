#include "CkDebuggerPanel_Inspector.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Inspectors/CkInspector_EntityInfo.h"
#include "CkEcsDebugger/Inspectors/CkInspector_Transform.h"
#include "CkEcsDebugger/Inspectors/CkInspector_Network.h"
#include "CkEcsDebugger/Inspectors/CkInspector_Relationships.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"

auto SCkDebuggerPanel_Inspector::Construct(const FArguments& InArgs, TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel) -> void
{
    SelectionModel = InSelectionModel;

    RegisterDefaultInspectors();

    if (SelectionModel.IsValid())
    {
        SelectionModel->OnSelectionChanged.AddSP(this, &SCkDebuggerPanel_Inspector::OnSelectionChanged);
    }

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(new FSlateColorBrush(FCkDebuggerStyle::Color_Background_Dark))
        .Padding(0.0f)
        [
            SAssignNew(ScrollBox, SScrollBox)
            .Orientation(Orient_Vertical)
            .ScrollBarAlwaysVisible(false)
        ]
    ];

    RebuildInspectors();
}

auto SCkDebuggerPanel_Inspector::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    // Tick all inspectors that can inspect the current entity
    if (ck::IsValid(CurrentInspectedEntity))
    {
        for (const auto& Inspector : Inspectors)
        {
            if (Inspector.IsValid() && Inspector->CanInspect(CurrentInspectedEntity))
            {
                Inspector->Tick(CurrentInspectedEntity, InDeltaTime);
            }
        }
    }
}

auto SCkDebuggerPanel_Inspector::RebuildInspectors() -> void
{
    if (NOT ScrollBox.IsValid())
    { return; }

    ScrollBox->ClearChildren();

    if (NOT SelectionModel.IsValid())
    {
        ScrollBox->AddSlot()
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                Build_NoSelectionWidget()
            ];
        return;
    }

    const auto& SelectedEntities = SelectionModel->Get_SelectedEntities();

    if (SelectedEntities.Num() == 0)
    {
        CurrentInspectedEntity = FCk_Handle{};
        ScrollBox->AddSlot()
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                Build_NoSelectionWidget()
            ];
        return;
    }

    if (SelectedEntities.Num() > 1)
    {
        CurrentInspectedEntity = FCk_Handle{};
        ScrollBox->AddSlot()
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                Build_MultiSelectionWidget(SelectedEntities.Num())
            ];
        return;
    }

    const auto& Entity = SelectedEntities[0];
    CurrentInspectedEntity = Entity;

    if (ck::Is_NOT_Valid(Entity))
    {
        ScrollBox->AddSlot()
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold"))
                .Text(FText::FromString(TEXT("Invalid Entity")))
                .ColorAndOpacity(FCkDebuggerStyle::Color_Error)
            ];
        return;
    }

    ScrollBox->AddSlot()
        .Padding(FCkDebuggerStyle::Padding_Small)
        [
            Build_SingleEntityInspector(Entity)
        ];
}

auto SCkDebuggerPanel_Inspector::Build_NoSelectionWidget() -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(FCkDebuggerStyle::Padding_Large)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.LargeHeader"))
                .Text(FText::FromString(TEXT("No Entity Selected")))
                .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Muted)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(FText::FromString(TEXT("Select an entity from the list to inspect")))
                .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
            ]
        ];
}

auto SCkDebuggerPanel_Inspector::Build_MultiSelectionWidget(int32 Count) -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(FCkDebuggerStyle::Padding_Large)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.LargeHeader"))
                .Text(FText::FromString(ck::Format_UE(TEXT("Multiple Entities Selected ({})"), Count)))
                .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Muted)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(FText::FromString(TEXT("Multi-entity inspection coming soon")))
                .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
            ]
        ];
}

auto SCkDebuggerPanel_Inspector::Build_SingleEntityInspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto VerticalBox = SNew(SVerticalBox);

    Inspectors.Sort([](const TSharedPtr<ICkDebuggerComponentInspector_Base>& A, const TSharedPtr<ICkDebuggerComponentInspector_Base>& B)
    {
        return A->Get_SortPriority() < B->Get_SortPriority();
    });

    bool FirstInspector = true;

    for (const auto& Inspector : Inspectors)
    {
        if (NOT Inspector.IsValid())
        { continue; }

        if (NOT Inspector->CanInspect(Entity))
        { continue; }

        if (NOT FirstInspector)
        {
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SSeparator)
                    .Orientation(Orient_Horizontal)
                    .SeparatorImage(new FSlateColorBrush(FCkDebuggerStyle::Color_Border))
                    .Thickness(1.0f)
                ];
        }

        FirstInspector = false;

        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(SExpandableArea)
                .InitiallyCollapsed(false)
                .BorderBackgroundColor(FCkDebuggerStyle::Color_Background_Dark)
                .BorderImage(new FSlateRoundedBoxBrush(
                    FCkDebuggerStyle::Color_Border,
                    2.0f,
                    FCkDebuggerStyle::Color_Background_Dark,
                    1.0f
                ))
                .HeaderPadding(FMargin(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small))
                .HeaderContent()
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Header"))
                    .Text(Inspector->Get_ComponentName())
                    .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Highlight)
                ]
                .BodyContent()
                [
                    SNew(SBox)
                    .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
                    [
                        Inspector->Build_Inspector(Entity)
                    ]
                ]
            ];
    }

    return VerticalBox;
}

auto SCkDebuggerPanel_Inspector::RegisterDefaultInspectors() -> void
{
    Inspectors.Empty();

    Inspectors.Add(MakeShared<FCkInspector_EntityInfo>());
    Inspectors.Add(MakeShared<FCkInspector_Transform>());
    Inspectors.Add(MakeShared<FCkInspector_Network>());
    Inspectors.Add(MakeShared<FCkInspector_Relationships>());
}

auto SCkDebuggerPanel_Inspector::OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void
{
    RebuildInspectors();
}