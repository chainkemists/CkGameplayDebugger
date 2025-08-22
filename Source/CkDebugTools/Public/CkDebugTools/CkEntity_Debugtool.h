#pragma once

#include "CkDebugTools/CkDebugTools_Base_Widget.h"
#include "CkDebugTools/CkEntity_Discovery.h"

#include <Widgets/Views/STreeView.h>

// --------------------------------------------------------------------------------------------------------------------

struct FCkEntityDebugInfo
{
    enum class EInfoType
    {
        Section,
        KeyValue,
        Transform,
        Network,
        Relationship
    };

    FString Key;
    FString Value;
    FLinearColor Color = FLinearColor::White;
    EInfoType Type = EInfoType::KeyValue;
    TArray<TSharedPtr<FCkEntityDebugInfo>> Children;

    FCkEntityDebugInfo() = default;
    FCkEntityDebugInfo(const FString& InKey, const FString& InValue, FLinearColor InColor = FLinearColor::White, EInfoType InType = EInfoType::KeyValue)
        : Key(InKey), Value(InValue), Color(InColor), Type(InType) {}

    static TSharedPtr<FCkEntityDebugInfo> CreateSection(const FString& InSectionName)
    {
        return MakeShareable(new FCkEntityDebugInfo(InSectionName, TEXT(""), FLinearColor::White, EInfoType::Section));
    }
};

// --------------------------------------------------------------------------------------------------------------------

class SCkEntityDebugInfoRow : public SMultiColumnTableRow<TSharedPtr<FCkEntityDebugInfo>>
{
public:
    SLATE_BEGIN_ARGS(SCkEntityDebugInfoRow) {}
        SLATE_ARGUMENT(TSharedPtr<FCkEntityDebugInfo>, InfoItem)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView) -> void;
    auto GenerateWidgetForColumn(const FName& ColumnName) -> TSharedRef<SWidget> override;

private:
    TSharedPtr<FCkEntityDebugInfo> _InfoItem;
};

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGTOOLS_API SCkEntityDebugTool : public SCkDebugTool_Base
{
public:
    SLATE_BEGIN_ARGS(SCkEntityDebugTool) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // SCkDebugTool_Base interface
    auto DoCreateContentPanel() -> TSharedRef<SWidget> override;
    auto DoGetToolName() -> FText override { return FText::FromString(TEXT("Entity Inspector")); }
    auto DoGetToolIcon() -> FSlateIcon override { return FSlateIcon("CkDebugToolsStyle", "CkDebugTools.Entity.Icon"); }
    auto DoRefreshToolData() -> void override;

private:
    auto DoCreateEntityInfoList() -> TSharedRef<SWidget>;
    auto DoRefreshEntityInfoList() -> void;
    auto DoCollectEntityInfo(const FCk_Handle& InEntity) -> void;
    auto DoAddBasicInfo(const FCk_Handle& InEntity) -> void;
    auto DoAddTransformInfo(const FCk_Handle& InEntity) -> void;
    auto DoAddNetworkInfo(const FCk_Handle& InEntity) -> void;
    auto DoAddRelationshipInfo(const FCk_Handle& InEntity) -> void;
    auto
        Tick(
            const FGeometry& AllottedGeometry,
            double InCurrentTime,
            float InDeltaTime)
            -> void override;

private:
    TSharedPtr<STreeView<TSharedPtr<FCkEntityDebugInfo>>> _EntityInfoTreeView;
    TArray<TSharedPtr<FCkEntityDebugInfo>> _EntityInfoItems;
    TSharedPtr<FCkEntityDiscovery> _EntityDiscovery;
};

// --------------------------------------------------------------------------------------------------------------------