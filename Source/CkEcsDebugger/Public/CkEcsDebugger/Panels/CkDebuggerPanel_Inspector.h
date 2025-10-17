#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "Widgets/SCompoundWidget.h"

class SScrollBox;

class SCkDebuggerPanel_Inspector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerPanel_Inspector) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel) -> void;
    auto Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

private:
    auto RebuildInspectors() -> void;
    auto Build_NoSelectionWidget() -> TSharedRef<SWidget>;
    auto Build_MultiSelectionWidget(int32 Count) -> TSharedRef<SWidget>;
    auto Build_SingleEntityInspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>;

    auto RegisterDefaultInspectors() -> void;
    auto OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void;

    TSharedPtr<SScrollBox> ScrollBox;
    TArray<TSharedPtr<ICkDebuggerComponentInspector_Base>> Inspectors;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;

    bool NeedsRebuild = true;
};