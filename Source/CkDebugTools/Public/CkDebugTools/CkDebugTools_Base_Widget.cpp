#include "CkDebugTools/CkDebugTools_Base_Widget.h"

#include "CkDebugTools/CkDebugTools_Style.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"

#include <Engine/World.h>
#include <Widgets/Layout/SSeparator.h>
#include <Framework/Application/SlateApplication.h>

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebugTool_Base::Construct(const FArguments& InArgs) -> void
{
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(&FCkDebugToolsStyle::Get().GetWidgetStyle<FTableRowStyle>("CkDebugTools.TableRow.Normal").EvenRowBackgroundBrush)
        .Padding(CkDebugToolsConstants::PanelPadding)
        [
            SNew(SHorizontalBox)

            // Left side: Entity Selection Panel (now takes up significant space)
            + SHorizontalBox::Slot()
            .FillWidth(0.4f) // 40% of width for entity selection
            .Padding(0, 0, CkDebugToolsConstants::SectionSpacing, 0)
            [
                DoCreateEntitySelectionPanel()
            ]

            // Right side: Tool Content
            + SHorizontalBox::Slot()
            .FillWidth(0.6f) // 60% of width for tool content
            [
                SNew(SScrollBox)
                .Orientation(Orient_Vertical)

                + SScrollBox::Slot()
                [
                    SAssignNew(_ContentContainer, SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0, 0, 0, CkDebugToolsConstants::SectionSpacing)
                    [
                        DoCreateSearchPanel()
                    ]

                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        DoCreateContentPanel()
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0, CkDebugToolsConstants::SectionSpacing, 0, 0)
                    [
                        DoCreateValidationPanel()
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0, CkDebugToolsConstants::SectionSpacing, 0, 0)
                    [
                        DoCreateActionPanel()
                    ]
                ]
            ]
        ]
    ];

    // Initial data load
    DoRefreshData();
}

auto SCkDebugTool_Base::DoCreateEntitySelectionPanel() -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(&FCkDebugToolsStyle::Get().GetWidgetStyle<FTableRowStyle>("CkDebugTools.TableRow.Normal").EvenRowBackgroundBrush)
        .Padding(CkDebugToolsConstants::PanelPadding)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 4)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Entity Selection")))
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Bold"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Primary"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                // FIXED: Use SCkSimpleEntitySelector instead of SCkEntitySelector
                SAssignNew(_EntitySelector, SCkSimpleEntitySelector)
                .OnSelectionChanged_Lambda([this]()
                {
                    // When entity selection changes, refresh our tool data
                    DoUpdateFromEntitySelection();
                })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 4, 0, 0)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    const auto SelectedEntities = Get_SelectedEntities();
                    if (SelectedEntities.Num() == 0)
                    {
                        return FText::FromString(TEXT("No entities selected - select an entity above to inspect"));
                    }
                    else if (SelectedEntities.Num() == 1)
                    {
                        const auto DebugName = SelectedEntities[0].ToString();
                        return FText::FromString(FString::Printf(TEXT("Inspecting: %s"), *DebugName));
                    }
                    else
                    {
                        return FText::FromString(FString::Printf(TEXT("Inspecting %d entities"), SelectedEntities.Num()));
                    }
                })
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Secondary"))
            ]
        ];
}

auto SCkDebugTool_Base::DoCreateValidationPanel() -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(&FCkDebugToolsStyle::Get().GetWidgetStyle<FTableRowStyle>("CkDebugTools.TableRow.Normal").EvenRowBackgroundBrush)
        .Padding(CkDebugToolsConstants::PanelPadding)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Validation Status")))
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Bold"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Warning"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 4, 0, 0)
            [
                SAssignNew(_ValidationText, STextBlock)
                .Text(FText::FromString(TEXT("Ready")))
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Secondary"))
                .AutoWrapText(true)
            ]
        ];
}

auto SCkDebugTool_Base::DoCreateSearchPanel() -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SAssignNew(_SearchBox, SSearchBox)
            .HintText(FText::FromString(TEXT("Search...")))
            .OnTextChanged(this, &SCkDebugTool_Base::DoOnSearchTextChanged)
        ];
}

auto SCkDebugTool_Base::DoCreateActionPanel() -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0, 0, 4, 0)
        [
            SNew(SBox)
            .WidthOverride(CkDebugToolsConstants::ButtonWidth)
            [
                SNew(SButton)
                .ButtonStyle(&FCkDebugToolsStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugTools.Button.Primary"))
                .Text(FText::FromString(TEXT("Refresh")))
                .OnClicked(this, &SCkDebugTool_Base::DoOnRefreshClicked)
            ]
        ];
}

auto SCkDebugTool_Base::DoRefreshEntitySelector() -> void
{
    if (_EntitySelector.IsValid())
    {
        _EntitySelector->Request_RefreshEntityList();
    }
}

auto SCkDebugTool_Base::DoUpdateFromEntitySelection() -> void
{
    const auto SelectedEntities = Get_SelectedEntities();

    // Only refresh if selection actually changed
    if (_CachedSelectedEntities != SelectedEntities)
    {
        _CachedSelectedEntities = SelectedEntities;

        if (_DataDiscovery.IsValid())
        {
            _DataDiscovery->Request_RefreshFromEntities(SelectedEntities);
        }

        DoRefreshToolData();
        DoUpdateValidationDisplay();
    }
}

auto SCkDebugTool_Base::DoRefreshData() -> void
{
    DoRefreshEntitySelector();
    DoUpdateFromEntitySelection();
}

auto SCkDebugTool_Base::Get_SelectedEntities() const -> TArray<FCk_Handle>
{
    if (_EntitySelector.IsValid())
    {
        return _EntitySelector->Get_SelectedEntities();
    }

    // Fallback to subsystem method
    UWorld* CurrentWorld = nullptr;
    for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
    {
        if (WorldContext.WorldType == EWorldType::PIE || WorldContext.WorldType == EWorldType::Game)
        {
            CurrentWorld = WorldContext.World();
            break;
        }
    }

    if (NOT IsValid(CurrentWorld))
    {
        return {};
    }

    const auto DebuggerSubsystem = CurrentWorld->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
    if (NOT IsValid(DebuggerSubsystem))
    {
        return {};
    }

    return DebuggerSubsystem->Get_SelectionEntities();
}

auto SCkDebugTool_Base::Get_PrimarySelectedEntity() const -> FCk_Handle
{
    const auto SelectedEntities = Get_SelectedEntities();
    return SelectedEntities.Num() > 0 ? SelectedEntities[0] : FCk_Handle{};
}

auto SCkDebugTool_Base::Get_EntityColor() const -> FLinearColor
{
    return FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Entity");
}

auto SCkDebugTool_Base::Get_VectorColor() const -> FLinearColor
{
    return FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Vector");
}

auto SCkDebugTool_Base::Get_NumberColor() const -> FLinearColor
{
    return FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Number");
}

auto SCkDebugTool_Base::Get_EnumColor() const -> FLinearColor
{
    return FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Enum");
}

auto SCkDebugTool_Base::Get_UnknownColor() const -> FLinearColor
{
    return FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Unknown");
}

auto SCkDebugTool_Base::Get_NoneColor() const -> FLinearColor
{
    return FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.None");
}

auto SCkDebugTool_Base::CreateColoredText(const FString& InText, const FLinearColor& InColor) -> TSharedRef<STextBlock>
{
    return SNew(STextBlock)
        .Text(FText::FromString(InText))
        .ColorAndOpacity(InColor)
        .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"));
}

auto SCkDebugTool_Base::DoUpdateValidationDisplay() -> void
{
    if (NOT _ValidationText.IsValid())
    {
        return;
    }

    if (_DataDiscovery.IsValid())
    {
        const auto [Total, Errors, Warnings] = _DataDiscovery->Get_ValidationStats();

        FString ValidationMessage = FString::Printf(TEXT("✓ %d items processed"), Total);

        if (Warnings > 0)
        {
            ValidationMessage += FString::Printf(TEXT("\n⚠ %d warnings"), Warnings);
        }

        if (Errors > 0)
        {
            ValidationMessage += FString::Printf(TEXT("\n❌ %d errors"), Errors);
        }

        _ValidationText->SetText(FText::FromString(ValidationMessage));
    }
    else
    {
        const auto SelectedEntities = Get_SelectedEntities();
        FString Status = FString::Printf(TEXT("✓ %d entities selected"), SelectedEntities.Num());
        _ValidationText->SetText(FText::FromString(Status));
    }
}

auto SCkDebugTool_Base::DoOnSearchTextChanged(const FText& InText) -> void
{
    _CurrentSearchText = InText.ToString();

    if (_DataDiscovery.IsValid())
    {
        _DataDiscovery->Request_ApplySearchFilter(_CurrentSearchText);
    }
}

auto SCkDebugTool_Base::DoOnRefreshClicked() -> FReply
{
    DoRefreshData();
    return FReply::Handled();
}

auto SCkDebugTool_Base::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    // Update from entity selection periodically
    static float LastUpdateTime = 0.0f;
    const float CurrentTime = static_cast<float>(InCurrentTime);

    if (CurrentTime - LastUpdateTime > 0.1f) // Update 10 times per second
    {
        LastUpdateTime = CurrentTime;
        DoUpdateFromEntitySelection();
    }
}

// --------------------------------------------------------------------------------------------------------------------
