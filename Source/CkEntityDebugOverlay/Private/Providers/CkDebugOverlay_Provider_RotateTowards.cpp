#include "CkDebugOverlay_Provider_RotateTowards.h"

#include "CkRotateTowards/CkRotateTowards_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"

#include "NativeGameplayTags.h"

// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_RotateTowards, "Ck.OnScreenDebugger.Provider.RotateTowards")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_RotateTowards_Summary, "Ck.OnScreenDebugger.Provider.RotateTowards.Summary")

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_overlay_provider_rotate_towards
{
    auto
        Get_RotateTowards(const FCk_Handle& InEntity)
        -> FCk_Handle_RotateTowards
    {
        auto MutableEntity = InEntity;
        return UCk_Utils_RotateTowards_UE::Cast(MutableEntity);
    }

    auto
        Get_TargetName(const FCk_Handle_Transform& InTarget)
        -> FString
    {
        if (ck::Is_NOT_Valid(InTarget))
        {
            return FString{TEXT("none")};
        }
        return UCk_Utils_GameplayLabel_UE::Has(InTarget)
            ? UCk_Utils_GameplayLabel_UE::Get_Label(InTarget).ToString() : ck::Format_UE(TEXT("{}"), InTarget);
    }

    auto
        Get_Status(const FCk_Handle_RotateTowards& InRotateTowards)
        -> FString
    {
        if (NOT UCk_Utils_RotateTowards_UE::Get_IsEnabled(InRotateTowards))
        {
            return FString{TEXT("Disabled")};
        }
        if (NOT UCk_Utils_RotateTowards_UE::Get_HasTarget(InRotateTowards))
        {
            return FString{TEXT("No target")};
        }
        return FString{UCk_Utils_RotateTowards_UE::Get_IsAtTarget(InRotateTowards) ? TEXT("At target") : TEXT("Turning")};
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_RotateTowards::Get_ProviderTag() const
    -> FGameplayTag
{
    return TAG_Ck_OnScreenDebugger_Provider_RotateTowards;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_RotateTowards::Get_FieldTags() const
    -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {{TAG_Ck_OnScreenDebugger_Provider_RotateTowards_Summary, true}};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_RotateTowards::CanProvide(const FCk_Handle& InEntity) const
    -> bool
{
    return ck::IsValid(InEntity) && UCk_Utils_RotateTowards_UE::Has(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_RotateTowards::Collect(
        const FCk_Handle& InEntity,
        const FCk_DebugOverlay_ProviderConfig& InConfig,
        FCk_DebugOverlay_Section& OutSection)
    -> void
{
    OutSection.ProviderTag = Get_ProviderTag();
    OutSection.SortPriority = Get_SortPriority();
    if (ck::Is_NOT_Valid(InEntity) || NOT InConfig.EnabledFields.HasTagExact(TAG_Ck_OnScreenDebugger_Provider_RotateTowards_Summary))
    {
        return;
    }
    const auto RotateTowards = ck_debug_overlay_provider_rotate_towards::Get_RotateTowards(InEntity);
    if (ck::Is_NOT_Valid(RotateTowards))
    {
        return;
    }
    const auto Remaining = UCk_Utils_RotateTowards_UE::Get_RemainingRotation(RotateTowards);
    auto Row = FCk_DebugOverlay_Row{};
    Row.FieldTag = TAG_Ck_OnScreenDebugger_Provider_RotateTowards_Summary;
    Row.Value = FText::FromString(ck::Format_UE(TEXT("RotateTowards → {} | rem P {:.1f} Y {:.1f} R {:.1f} | {}"),
        ck_debug_overlay_provider_rotate_towards::Get_TargetName(UCk_Utils_RotateTowards_UE::Get_Target(RotateTowards)),
        Remaining.Pitch, Remaining.Yaw, Remaining.Roll,
        ck_debug_overlay_provider_rotate_towards::Get_Status(RotateTowards)));
    Row.Severity = UCk_Utils_RotateTowards_UE::Get_IsEnabled(RotateTowards) ? ECk_DebugOverlay_Severity::Normal : ECk_DebugOverlay_Severity::Warn;
    OutSection.Rows.Add(MoveTemp(Row));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_RotateTowards::Get_CompactToken(
        const FCk_Handle& InEntity,
        const FCk_DebugOverlay_ProviderConfig& InConfig) const
    -> FString
{
    if (ck::Is_NOT_Valid(InEntity))
    {
        return {};
    }
    const auto RotateTowards = ck_debug_overlay_provider_rotate_towards::Get_RotateTowards(InEntity);
    if (ck::Is_NOT_Valid(RotateTowards))
    {
        return {};
    }
    if (NOT UCk_Utils_RotateTowards_UE::Get_HasTarget(RotateTowards))
    {
        return FString{TEXT("RT:--")};
    }
    return ck::Format_UE(TEXT("RT:{:.0f}"), UCk_Utils_RotateTowards_UE::Get_RemainingRotation(RotateTowards).Yaw);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_RotateTowards)
