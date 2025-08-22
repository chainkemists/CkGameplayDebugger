// Let's create a much simpler entity selector first
// CkSimpleEntitySelector_Widget.h

#pragma once

#include "CkDebugTools/CkDebugTools_Style.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#include <Widgets/SCompoundWidget.h>
#include <Widgets/Input/SComboBox.h>

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGTOOLS_API SCkSimpleEntitySelector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkSimpleEntitySelector) {}
        SLATE_EVENT(FSimpleDelegate, OnSelectionChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Get currently selected entities
    auto Get_SelectedEntities() const -> TArray<FCk_Handle>;

    // Force refresh of the entity list
    auto Request_RefreshEntityList() -> void;

private:
    // UI Creation
    auto DoCreateEntityComboBox() -> TSharedRef<SWidget>;

    // Entity Discovery
    auto DoGetAllEntitiesFromWorld() -> TArray<FCk_Handle>;

    // Combo box callbacks
    auto DoGenerateComboWidget(TSharedPtr<FCk_Handle> InEntity) -> TSharedRef<SWidget>;
    auto DoOnEntitySelected(TSharedPtr<FCk_Handle> InSelectedEntity, ESelectInfo::Type SelectInfo) -> void;

    // World context helpers
    auto Get_CurrentWorld() const -> UWorld*;
    auto Get_DebuggerSubsystem() const -> UCk_EcsDebugger_Subsystem_UE*;
    auto Get_TransientEntity() const -> FCk_Handle;

private:
    // UI Components
    TSharedPtr<SComboBox<TSharedPtr<FCk_Handle>>> _EntityComboBox;

    // Data
    TArray<TSharedPtr<FCk_Handle>> _AvailableEntities;

    // Events
    FSimpleDelegate OnSelectionChanged;
};

