#pragma once

#include "CkDebugTools/CkDebugTools_Base_Discovery.h"

#include "CkEcs/Handle/CkHandle.h"

#include <Widgets/SCompoundWidget.h>
#include <Widgets/Views/SListView.h>
#include <Widgets/Input/SComboBox.h>
#include <Widgets/Input/SSearchBox.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SBorder.h>

// --------------------------------------------------------------------------------------------------------------------

namespace CkDebugToolsConstants
{
    constexpr float PanelPadding = 8.0f;
    constexpr float SectionSpacing = 8.0f;
    constexpr float ButtonWidth = 70.0f;
    constexpr float SearchBoxHeight = 24.0f;
}

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGTOOLS_API SCkDebugTool_Base : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebugTool_Base) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Virtual methods for derived tools to implement
    virtual auto DoCreateContentPanel() -> TSharedRef<SWidget> = 0;
    virtual auto DoGetToolName() -> FText = 0;
    virtual auto DoGetToolIcon() -> FSlateIcon = 0;
    virtual auto DoRefreshToolData() -> void {}

protected:
    // Common panels all tools can use
    auto DoCreateEntitySelectionPanel() -> TSharedRef<SWidget>;
    auto DoCreateValidationPanel() -> TSharedRef<SWidget>;
    auto DoCreateSearchPanel() -> TSharedRef<SWidget>;
    auto DoCreateActionPanel() -> TSharedRef<SWidget>;

    // Integration with existing ECS subsystem
    auto DoUpdateFromEntitySelection() -> void;
    auto DoRefreshData() -> void;
    auto Get_SelectedEntities() const -> TArray<FCk_Handle>;
    auto Get_PrimarySelectedEntity() const -> FCk_Handle;

    // Style helpers
    auto Get_EntityColor() const -> FLinearColor;
    auto Get_VectorColor() const -> FLinearColor;
    auto Get_NumberColor() const -> FLinearColor;
    auto Get_EnumColor() const -> FLinearColor;
    auto Get_UnknownColor() const -> FLinearColor;
    auto Get_NoneColor() const -> FLinearColor;

    // Common UI elements
    auto CreateColoredText(const FString& InText, const FLinearColor& InColor) -> TSharedRef<STextBlock>;
    auto CreateTableRow(const char* InLabel, const FString& InValue, const FLinearColor& InColor) -> void;

    // Validation display
    auto DoUpdateValidationDisplay() -> void;

private:
    auto DoOnSearchTextChanged(const FText& InText) -> void;
    auto DoOnRefreshClicked() -> FReply;

    auto DoTick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

protected:
    TSharedPtr<SSearchBox> _SearchBox;
    TSharedPtr<STextBlock> _ValidationText;
    TSharedPtr<SVerticalBox> _ContentContainer;

    TArray<FCk_Handle> _CachedSelectedEntities;
    FString _CurrentSearchText;
    float _LastRefreshTime = 0.0f;

    // Data discovery - can be overridden by derived classes
    TSharedPtr<FCkDebugToolsDiscovery_Base> _DataDiscovery;
};

// --------------------------------------------------------------------------------------------------------------------