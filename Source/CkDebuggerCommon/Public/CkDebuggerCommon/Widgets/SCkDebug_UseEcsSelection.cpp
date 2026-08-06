#include "SCkDebug_UseEcsSelection.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_UseEcsSelection::Construct(const FArguments& InArgs) -> void
{
    _TargetTabId = InArgs._TargetTabId;

    ChildSlot
    [
        SNew(SButton)
            .Text(this, &SCkDebug_UseEcsSelection::Get_Label)
            .ToolTipText(this, &SCkDebug_UseEcsSelection::Get_Tooltip)
            .IsEnabled(this, &SCkDebug_UseEcsSelection::Get_IsEnabled)
            .OnClicked(this, &SCkDebug_UseEcsSelection::OnClicked)
    ];
}

auto SCkDebug_UseEcsSelection::Get_Label() const -> FText
{
    const auto Selection = ck::DebugSelectionSync::Get_PrimaryEcsSelection();
    if (ck::Is_NOT_Valid(Selection))
    { return FText::FromString(TEXT("Sync from ECS")); }

    return FText::FromString(ck::Format_UE(TEXT("Sync from ECS {}"), Selection.Get_Entity()));
}

auto SCkDebug_UseEcsSelection::Get_Tooltip() const -> FText
{
    if (NOT ck::DebugSelectionSync::Has_PrimaryEcsSelectionProvider())
    { return FText::FromString(TEXT("Open the CK ECS Debugger and select an entity first")); }

    const auto Selection = ck::DebugSelectionSync::Get_PrimaryEcsSelection();
    if (ck::Is_NOT_Valid(Selection))
    { return FText::FromString(TEXT("The CK ECS Debugger has no valid primary selection")); }

    if (NOT FCkDebug_EntityTargetRegistry::Get().CanTarget(_TargetTabId, Selection))
    { return FText::FromString(TEXT("This debugger cannot target the current ECS selection")); }

    const auto Name = UCk_Utils_Handle_UE::Get_DebugName(Selection);
    return FText::FromString(ck::Format_UE(
        TEXT("Sync the matching entity in this debugger from the ECS Debugger{}"),
        Name.IsNone() ? FString{} : ck::Format_UE(TEXT(": {}"), Name)));
}

auto SCkDebug_UseEcsSelection::Get_IsEnabled() const -> bool
{
    const auto Selection = ck::DebugSelectionSync::Get_PrimaryEcsSelection();
    return FCkDebug_EntityTargetRegistry::Get().CanTarget(_TargetTabId, Selection);
}

auto SCkDebug_UseEcsSelection::OnClicked() -> FReply
{
    const auto Selection = ck::DebugSelectionSync::Get_PrimaryEcsSelection();
    FCkDebug_EntityTargetRegistry::Get().TryOpenAndTarget(_TargetTabId, Selection);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------
