#include "CkDebuggerPanel_Inspector.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_SearchBar.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"

SCkDebuggerPanel_Inspector::~SCkDebuggerPanel_Inspector()
{
    DeactivateAllInspectors();
}

auto SCkDebuggerPanel_Inspector::DeactivateAllInspectors() -> void
{
    for (const auto& Inspector : Inspectors)
    {
        if (Inspector.IsValid())
        {
            Inspector->OnDeactivated();
        }
    }
}

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
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(0.0f)
        [
            SAssignNew(ScrollBox, SScrollBox)
            .Orientation(Orient_Vertical)
            .ScrollBarAlwaysVisible(true)
            .ScrollBarVisibility(EVisibility::Visible)
        ]
    ];

    RebuildInspectors();
}

auto SCkDebuggerPanel_Inspector::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (ck::IsValid(CurrentInspectedEntity))
    {
        for (int32 Index = 0; Index < Inspectors.Num(); ++Index)
        {
            const auto& Inspector = Inspectors[Index];
            if (NOT Inspector.IsValid() || NOT Inspector->CanInspect(CurrentInspectedEntity))
            { continue; }

            Inspector->Tick(CurrentInspectedEntity, InDeltaTime);

            if (Inspector->NeedsRebuild())
            {
                Inspector->ClearRebuildFlag();

                const auto Filter = InspectorFilters.FindRef(Index);
                if (const auto Container = InspectorContentContainers.Find(Index))
                {
                    if (Container->IsValid())
                    {
                        (*Container)->SetContent(
                            Inspector->IsFilterable()
                                ? Inspector->Build_Inspector(CurrentInspectedEntity, Filter)
                                : Inspector->Build_Inspector(CurrentInspectedEntity)
                        );
                    }
                }
            }
        }
    }
}

auto SCkDebuggerPanel_Inspector::RebuildInspectors() -> void
{
    if (NOT ScrollBox.IsValid())
    { return; }

    // Notify all inspectors that they are being deactivated before rebuilding.
    // This allows inspectors to clean up per-entity state (debug draw, etc.).
    DeactivateAllInspectors();

    ScrollBox->ClearChildren();
    InspectorContentContainers.Empty();

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

    auto FirstSection = true;

    auto AddSeparator = [&]()
    {
        if (NOT FirstSection)
        {
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SSeparator)
                    .Orientation(Orient_Horizontal)
                    .SeparatorImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Separator"))
                    .Thickness(1.0f)
                ];
        }
        FirstSection = false;
    };

    auto AddSection = [&](const FText& InName, const TSharedRef<SWidget>& InContent)
    {
        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(SExpandableArea)
                .InitiallyCollapsed(false)
                .BorderBackgroundColor(FCkDebuggerStyle::Color_Background_Dark)
                .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border"))
                .HeaderPadding(FMargin(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small))
                .HeaderContent()
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Header"))
                    .Text(InName)
                    .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Highlight)
                ]
                .BodyContent()
                [
                    SNew(SBox)
                    .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
                    [
                        InContent
                    ]
                ]
            ];
    };

    for (int32 Index = 0; Index < Inspectors.Num(); ++Index)
    {
        const auto& Inspector = Inspectors[Index];

        if (NOT Inspector.IsValid())
        { continue; }

        if (NOT Inspector->CanInspect(Entity))
        { continue; }

        if (Inspector->IsMultiSection())
        {
            for (const auto& Section : Inspector->Get_InspectorSections(Entity))
            {
                AddSeparator();
                AddSection(Section.Name, Section.Widget);
            }
        }
        else
        {
            AddSeparator();
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    Build_InspectorSection(Entity, Inspector, Index)
                ];
        }
    }

    return VerticalBox;
}

auto SCkDebuggerPanel_Inspector::Build_InspectorSection(
    const FCk_Handle& Entity,
    const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector,
    int32 InspectorIndex) -> TSharedRef<SWidget>
{
    const auto Filter = InspectorFilters.FindRef(InspectorIndex);

    auto BodyContent = SNew(SVerticalBox);

    if (Inspector->IsFilterable())
    {
        BodyContent->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebuggerWidget_SearchBar)
                .OnSearchTextChanged_Lambda([this, InspectorIndex](const FString& InText)
                {
                    OnInspectorFilterChanged(InspectorIndex, InText);
                })
            ];
    }

    TSharedPtr<SBox> ContentContainer;

    BodyContent->AddSlot()
        .AutoHeight()
        [
            SAssignNew(ContentContainer, SBox)
            .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
            [
                Inspector->IsFilterable()
                    ? Inspector->Build_Inspector(Entity, Filter)
                    : Inspector->Build_Inspector(Entity)
            ]
        ];

    InspectorContentContainers.Add(InspectorIndex, ContentContainer);

    return SNew(SExpandableArea)
        .InitiallyCollapsed(false)
        .BorderBackgroundColor(FCkDebuggerStyle::Color_Background_Dark)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border"))
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
            BodyContent
        ];
}

auto SCkDebuggerPanel_Inspector::RegisterDefaultInspectors() -> void
{
    Inspectors = FCkDebuggerInspectorRegistry::Get().CreateAll();

    for (const auto& Inspector : Inspectors)
    {
        if (Inspector.IsValid())
        {
            Inspector->Set_SelectionModel(SelectionModel);
        }
    }
}

auto SCkDebuggerPanel_Inspector::OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void
{
    RebuildInspectors();
}

auto SCkDebuggerPanel_Inspector::OnInspectorFilterChanged(int32 InspectorIndex, const FString& InFilterText) -> void
{
    InspectorFilters.Add(InspectorIndex, InFilterText);

    if (NOT ck::IsValid(CurrentInspectedEntity))
    { return; }

    if (NOT Inspectors.IsValidIndex(InspectorIndex))
    { return; }

    const auto& Inspector = Inspectors[InspectorIndex];
    if (NOT Inspector.IsValid() || NOT Inspector->IsFilterable())
    { return; }

    if (const auto Container = InspectorContentContainers.Find(InspectorIndex))
    {
        if (Container->IsValid())
        {
            (*Container)->SetContent(
                SNew(SBox)
                .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
                [
                    Inspector->Build_Inspector(CurrentInspectedEntity, InFilterText)
                ]
            );
        }
    }
}
