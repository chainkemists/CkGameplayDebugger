#include "CkDebugOverlay_Provider_Chain.h"

#include "CkChain/CkChain_Utils.h"
#include "CkChain/CkChainLink_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"

#include "NativeGameplayTags.h"

// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Chain, "Ck.OnScreenDebugger.Provider.Chain")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Chain_Summary, "Ck.OnScreenDebugger.Provider.Chain.Summary")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Chain_Link, "Ck.OnScreenDebugger.Provider.Chain.Link")

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_overlay_provider_chain
{
    auto
        Get_ChainName(const FCk_Handle_Chain& InChain)
        -> FString
    {
        return UCk_Utils_GameplayLabel_UE::Has(InChain)
            ? UCk_Utils_GameplayLabel_UE::Get_Label(InChain).ToString() : FString{TEXT("Unnamed")};
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_Chain::Get_ProviderTag() const
    -> FGameplayTag
{
    return TAG_Ck_OnScreenDebugger_Provider_Chain;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_Chain::Get_FieldTags() const
    -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {{TAG_Ck_OnScreenDebugger_Provider_Chain_Summary, true}, {TAG_Ck_OnScreenDebugger_Provider_Chain_Link, true}};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_Chain::CanProvide(const FCk_Handle& InEntity) const
    -> bool
{
    return ck::IsValid(InEntity)
        && (UCk_Utils_Chain_UE::Has_Any(InEntity) || UCk_Utils_ChainLink_UE::Has(InEntity));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_Chain::Collect(
        const FCk_Handle& InEntity,
        const FCk_DebugOverlay_ProviderConfig& InConfig,
        FCk_DebugOverlay_Section& OutSection)
    -> void
{
    OutSection.ProviderTag = Get_ProviderTag();
    OutSection.SortPriority = Get_SortPriority();
    if (ck::Is_NOT_Valid(InEntity))
    {
        return;
    }
    if (InConfig.EnabledFields.HasTagExact(TAG_Ck_OnScreenDebugger_Provider_Chain_Summary)
        && UCk_Utils_Chain_UE::Has_Any(InEntity))
    {
        auto Summaries = TArray<FString>{};
        UCk_Utils_Chain_UE::ForEach_Chain(InEntity, [&Summaries](FCk_Handle_Chain InChain)
        {
            Summaries.Add(ck::Format_UE(TEXT("{}({}, {} links, {} cm history)"),
                ck_debug_overlay_provider_chain::Get_ChainName(InChain), UCk_Utils_Chain_UE::Get_Solver(InChain),
                UCk_Utils_Chain_UE::Get_NumLinks(InChain), UCk_Utils_Chain_UE::Get_HistoryLengthCm(InChain)));
        });
        if (Summaries.Num() > 0)
        {
            auto Row = FCk_DebugOverlay_Row{};
            Row.FieldTag = TAG_Ck_OnScreenDebugger_Provider_Chain_Summary;
            Row.Value = FText::FromString(ck::Format_UE(TEXT("[{} chains] {}"), Summaries.Num(), FString::Join(Summaries, TEXT(", "))));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            OutSection.Rows.Add(MoveTemp(Row));
        }
    }
    if (InConfig.EnabledFields.HasTagExact(TAG_Ck_OnScreenDebugger_Provider_Chain_Link)
        && UCk_Utils_ChainLink_UE::Has(InEntity))
    {
        auto MutableEntity = InEntity;
        const auto Link = UCk_Utils_ChainLink_UE::Cast(MutableEntity);
        const auto Chain = UCk_Utils_ChainLink_UE::Get_Chain(Link);
        if (ck::IsValid(Chain))
        {
            auto Row = FCk_DebugOverlay_Row{};
            Row.FieldTag = TAG_Ck_OnScreenDebugger_Provider_Chain_Link;
            Row.Value = FText::FromString(ck::Format_UE(TEXT("Link #{} of {} @ {} cm"),
                UCk_Utils_ChainLink_UE::Get_Index(Link), ck_debug_overlay_provider_chain::Get_ChainName(Chain),
                UCk_Utils_ChainLink_UE::Get_DistanceFromHeadCm(Link)));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            OutSection.Rows.Add(MoveTemp(Row));
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_DebugOverlay_Provider_Chain::Get_CompactToken(
        const FCk_Handle& InEntity,
        const FCk_DebugOverlay_ProviderConfig& InConfig) const
    -> FString
{
    if (ck::Is_NOT_Valid(InEntity))
    {
        return {};
    }
    if (UCk_Utils_ChainLink_UE::Has(InEntity))
    {
        auto MutableEntity = InEntity;
        const auto Link = UCk_Utils_ChainLink_UE::Cast(MutableEntity);
        return ck::Format_UE(TEXT("Lnk:{}"), UCk_Utils_ChainLink_UE::Get_Index(Link));
    }
    if (NOT UCk_Utils_Chain_UE::Has_Any(InEntity))
    {
        return {};
    }
    auto Count = 0;
    UCk_Utils_Chain_UE::ForEach_Chain(InEntity, [&Count](FCk_Handle_Chain InChain)
    {
        Count += UCk_Utils_Chain_UE::Get_NumLinks(InChain);
    });
    return UCk_Utils_Chain_UE::Has_Any(InEntity) ? ck::Format_UE(TEXT("Chn:{}"), Count) : FString{};
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Chain)
