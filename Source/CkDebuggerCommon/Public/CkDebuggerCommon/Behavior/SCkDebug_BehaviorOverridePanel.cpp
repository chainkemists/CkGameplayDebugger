#include "SCkDebug_BehaviorOverridePanel.h"

#include "CkDebuggerCommon/Behavior/CkDebug_BehaviorOverrideRegistry.h"
#include "CkDebuggerCommon/Behavior/SCkDebug_BehaviorOverrideRow.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_BehaviorOverridePanel::Construct(const FArguments& InArgs) -> void
{
    ChildSlot[SAssignNew(_Rows, SVerticalBox)];
    _RegistryChangedHandle = FCkDebug_BehaviorOverrideRegistry::Get().Get_OnChanged().AddSP(
        this, &SCkDebug_BehaviorOverridePanel::RebuildRows);
    RebuildRows();
}

SCkDebug_BehaviorOverridePanel::~SCkDebug_BehaviorOverridePanel()
{
    if (_RegistryChangedHandle.IsValid())
    { FCkDebug_BehaviorOverrideRegistry::Get().Get_OnChanged().Remove(_RegistryChangedHandle); }
}

auto SCkDebug_BehaviorOverridePanel::RebuildRows() -> void
{
    if (NOT _Rows.IsValid()) { return; }
    _Rows->ClearChildren();
    const auto Descriptors = FCkDebug_BehaviorOverrideRegistry::Get().Get_Descriptors();

    if (Descriptors.IsEmpty())
    {
        _Rows->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("No session behavior overrides are registered.")))
            .ColorAndOpacity(CkStyle::TextMute())
        ];
    }

    for (const auto& Descriptor : Descriptors)
    {
        _Rows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
        [
            SNew(SCkDebug_BehaviorOverrideRow)
            .OverrideId(Descriptor.Get_Id())
        ];
    }

}

// --------------------------------------------------------------------------------------------------------------------
