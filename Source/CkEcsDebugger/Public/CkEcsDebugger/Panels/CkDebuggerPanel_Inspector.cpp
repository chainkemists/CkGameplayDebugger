#include "CkEcsDebugger/Panels/CkDebuggerPanel_Inspector.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Inspectors/CkInspector_EntityInfo.h"
#include "CkEcsDebugger/Inspectors/CkInspector_Transform.h"
#include "CkEcsDebugger/Inspectors/CkInspector_Network.h"
#include "CkEcsDebugger/Inspectors/CkInspector_Relationships.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
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
        SAssignNew(ScrollBox, SScrollBox)
        .Orientation(Orient_Vertical)
    ];

    RebuildInspectors();
}

auto SCkDebuggerPanel_Inspector::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (NeedsRebuild)
    {
        RebuildInspectors();
        NeedsRebuild = false;
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
            .Padding(8.0f)
            [
                Build_NoSelectionWidget()
            ];
        return;
    }

    const auto& SelectedEntities = SelectionModel->Get_SelectedEntities();

    if (SelectedEntities.Num() == 0)
    {
        ScrollBox->AddSlot()
            .Padding(8.0f)
            [
                Build_NoSelectionWidget()
            ];
        return;
    }

    if (SelectedEntities.Num() > 1)
    {
        ScrollBox->AddSlot()
            .Padding(8.0f)
            [
                Build_MultiSelectionWidget(SelectedEntities.Num())
            ];
        return;
    }

    const auto& Entity = SelectedEntities[0];

    if (ck::Is_NOT_Valid(Entity))
    {
        ScrollBox->AddSlot()
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Invalid Entity")))
                .ColorAndOpacity(FSlateColor(FLinearColor::Red))
            ];
        return;
    }

    ScrollBox->AddSlot()
        .Padding(4.0f)
        [
            Build_SingleEntityInspector(Entity)
        ];
}

auto SCkDebuggerPanel_Inspector::Build_NoSelectionWidget() -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("No Entity Selected")))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
        ];
}

auto SCkDebuggerPanel_Inspector::Build_MultiSelectionWidget(int32 Count) -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(ck::Format_UE(TEXT("Multiple Entities Selected ({})"), Count)))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
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
                .Padding(0.0f, 8.0f, 0.0f, 8.0f)
                [
                    SNew(SSeparator)
                    .Orientation(Orient_Horizontal)
                ];
        }

        FirstInspector = false;

        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(SExpandableArea)
                .InitiallyCollapsed(false)
                .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f))
                .HeaderContent()
                [
                    SNew(STextBlock)
                    .Text(Inspector->Get_ComponentName())
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
                .BodyContent()
                [
                    SNew(SBox)
                    .Padding(FMargin(8.0f, 4.0f))
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
    NeedsRebuild = true;
}