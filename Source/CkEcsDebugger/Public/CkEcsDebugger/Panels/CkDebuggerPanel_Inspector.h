#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;
class SCkDebuggerWidget_SearchBar;

class SCkDebuggerPanel_Inspector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerPanel_Inspector) {}
    SLATE_END_ARGS()

    ~SCkDebuggerPanel_Inspector();

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel) -> void;
    auto Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto Get_CurrentInspectedEntity() const -> FCk_Handle { return CurrentInspectedEntity; }

private:
    auto DeactivateAllInspectors() -> void;
    auto RebuildInspectors() -> void;
    auto Build_NoSelectionWidget() -> TSharedRef<SWidget>;
    auto Build_MultiSelectionWidget(int32 Count) -> TSharedRef<SWidget>;
    auto Build_SingleEntityInspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    auto Build_InspectorSection(const FCk_Handle& Entity, const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector, int32 InspectorIndex) -> TSharedRef<SWidget>;

    auto RegisterDefaultInspectors() -> void;
    auto OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void;
    auto OnInspectorFilterChanged(int32 InspectorIndex, const FString& InFilterText) -> void;

    TSharedPtr<SScrollBox> ScrollBox;
    TArray<TSharedPtr<ICkDebuggerComponentInspector_Base>> Inspectors;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;

    TMap<int32, FString> InspectorFilters;
    TMap<int32, TSharedPtr<SBox>> InspectorContentContainers;
    FCk_Handle CurrentInspectedEntity;
};
