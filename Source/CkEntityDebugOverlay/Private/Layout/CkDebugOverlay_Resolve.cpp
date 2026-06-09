#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Resolve.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugoverlay
{

auto Resolve_EnabledFields(
    const FCk_DebugOverlay_Layout&            Layout,
    const FGameplayTag&                       ProviderTag,
    const TArray<FCk_DebugOverlay_FieldDesc>& AllFields) -> FGameplayTagContainer
{
    FGameplayTagContainer Out;

    const FCk_DebugOverlay_ProviderEntry* Entry =
        Layout.Entries.FindByPredicate([&](const auto& E) { return E.ProviderTag == ProviderTag; });

    const bool ParentEnabled  = Layout.EnabledProviders.HasTagExact(ProviderTag);
    const bool EntryHasFields = Entry && !Entry->EnabledFields.IsEmpty();

    for (const auto& F : AllFields)
    {
        // Path 1: provider is in EnabledProviders → honour DefaultEnabled.
        const bool ViaParent = ParentEnabled && F.DefaultEnabled;

        // Path 2: provider has an Entry but no explicit field list → honour DefaultEnabled.
        const bool ViaEntryAll = Entry && !EntryHasFields && F.DefaultEnabled;

        // Path 3: provider has an Entry with an explicit field list → only those exact tags.
        const bool ViaEntryField = EntryHasFields && Entry->EnabledFields.HasTagExact(F.Tag);

        if (ViaParent || ViaEntryAll || ViaEntryField)
        {
            Out.AddTag(F.Tag);
        }
    }
    return Out;
}

// --------------------------------------------------------------------------------------------------------------------

auto Resolve_Style(
    const FCk_DebugOverlay_Layout& Layout,
    const FGameplayTag&            ProviderTag) -> FCk_DebugOverlay_RenderStyle
{
    FCk_DebugOverlay_RenderStyle Style = Layout.DefaultStyle;

    if (const auto* E = Layout.Entries.FindByPredicate([&](const auto& X) { return X.ProviderTag == ProviderTag; }))
    {
        if (E->bOverrideStyle)
        {
            Style = E->StyleOverride;
        }
    }
    return Style;
}

// --------------------------------------------------------------------------------------------------------------------

auto Validate_Layout(
    const FCk_DebugOverlay_Layout& Layout,
    const FGameplayTagContainer&   KnownProviderTags) -> TArray<FString>
{
    TArray<FString> Problems;
    auto CheckTag = [&](const FGameplayTag& Tag)
    {
        if (Tag.IsValid() && !KnownProviderTags.HasTagExact(Tag))
        {
            Problems.Add(FString::Printf(
                TEXT("Layout references unknown provider tag: %s"), *Tag.ToString()));
        }
    };
    for (const auto& Tag : Layout.EnabledProviders) { CheckTag(Tag); }
    for (const auto& Entry : Layout.Entries)        { CheckTag(Entry.ProviderTag); }
    return Problems;
}

} // namespace ck_debugoverlay

// --------------------------------------------------------------------------------------------------------------------
