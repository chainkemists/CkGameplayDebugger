#include "CkInspector_Network.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Net/CkNet_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Network)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_inspector_network
{
    // Tone mapping — "who is in charge here?" reads at a glance:
    //   Host / Authority  = this instance owns the entity            -> Ok
    //   ClientAndHost     = listen server, both hats                 -> Accent (the notable case)
    //   Client / Proxy    = following someone else's authority       -> Info
    //   Unknown / None    = not resolved / not networked             -> Neutral
    static auto Get_NetModeTone(
        ECk_Net_NetModeType InNetMode)
        -> ECk_Tone
    {
        switch (InNetMode)
        {
            case ECk_Net_NetModeType::Host:          return ECk_Tone::Ok;
            case ECk_Net_NetModeType::ClientAndHost: return ECk_Tone::Accent;
            case ECk_Net_NetModeType::Client:        return ECk_Tone::Info;
            case ECk_Net_NetModeType::Unknown:
            default:                                 return ECk_Tone::Neutral;
        }
    }

    static auto Get_NetRoleTone(
        ECk_Net_EntityNetRole InNetRole)
        -> ECk_Tone
    {
        switch (InNetRole)
        {
            case ECk_Net_EntityNetRole::Authority: return ECk_Tone::Ok;
            case ECk_Net_EntityNetRole::Proxy:     return ECk_Tone::Info;
            case ECk_Net_EntityNetRole::None:
            default:                               return ECk_Tone::Neutral;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspector_Network::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Network"));
}

auto FCkInspector_Network::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_Network::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    // Captured by value and re-validated per read — same contract as every other inspector row
    // attribute (rows outlive nothing, but the entity can die under them mid-PIE).
    const auto CapturedEntity = Entity;

    return FCkInspectorWidgetBuilder()
        .AddStatusPillRow(
            FText::FromString(TEXT("NetMode:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Net_UE::Get_EntityNetMode(CapturedEntity)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity)) { return ECk_Tone::Neutral; }
                return ck_inspector_network::Get_NetModeTone(UCk_Utils_Net_UE::Get_EntityNetMode(CapturedEntity));
            }))
        .AddStatusPillRow(
            FText::FromString(TEXT("NetRole:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Net_UE::Get_EntityNetRole(CapturedEntity)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity)) { return ECk_Tone::Neutral; }
                return ck_inspector_network::Get_NetRoleTone(UCk_Utils_Net_UE::Get_EntityNetRole(CapturedEntity));
            }))
        .Build(Entity);
}

auto FCkInspector_Network::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed for network info
}