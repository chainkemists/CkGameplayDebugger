#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;
class SCkDebuggerWidget_SearchBar;

enum class ECkInspectorDisplayMode : uint8
{
    GroupByInspector,
    GroupByEntity
};

class SCkDebuggerPanel_Inspector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerPanel_Inspector) {}
    SLATE_END_ARGS()

    ~SCkDebuggerPanel_Inspector();

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel) -> void;
    auto Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto Get_CurrentInspectedEntity() const -> FCk_Handle;

private:
    auto DeactivateAllInspectors() -> void;
    auto RebuildInspectors() -> void;
    auto Build_NoSelectionWidget() -> TSharedRef<SWidget>;
    auto Build_SingleEntityInspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    auto Build_InspectorSection(const FCk_Handle& Entity, const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector, int32 InspectorIndex) -> TSharedRef<SWidget>;

    auto Build_ModeToggle() -> TSharedRef<SWidget>;
    auto Build_MultiEntityInspector_GroupByInspector(const TArray<FCk_Handle>& Entities) -> TSharedRef<SWidget>;
    auto Build_MultiEntityInspector_GroupByEntity(const TArray<FCk_Handle>& Entities) -> TSharedRef<SWidget>;
    auto Build_EntitySubSection(const FCk_Handle& Entity, const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector, int32 OuterIndex, int32 InnerIndex) -> TSharedRef<SWidget>;
    auto OnDisplayModeChanged(ECkInspectorDisplayMode NewMode) -> void;
    auto Format_EntityDisplayName(const FCk_Handle& Entity) const -> FText;

    auto RegisterDefaultInspectors() -> void;
    auto OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void;
    auto OnInspectorFilterChanged(int32 InspectorIndex, const FString& InFilterText) -> void;

    TSharedPtr<SScrollBox> ScrollBox;
    TSharedPtr<SBox> _ModeToggleContainer;
    TArray<TSharedPtr<ICkDebuggerComponentInspector_Base>> Inspectors;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;

    ECkInspectorDisplayMode _DisplayMode = ECkInspectorDisplayMode::GroupByInspector;
    TMap<int32, FString> InspectorFilters;
    TMap<TPair<int32, int32>, TSharedPtr<SBox>> _InspectorContentContainers;
    TArray<FCk_Handle> _CurrentInspectedEntities;
};
