#include "CkInputDebugger/Data/CkInputDebugger_Bindings.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkInput/CkKeyBinding_Utils.h"
#include "CkInput/CkPlayerMappableKeySettings.h"

#include "GameFramework/PlayerController.h"
#include "UserSettings/EnhancedInputUserSettings.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_debugger_bindings
{
    auto Get_SlotLabel(EPlayerMappableKeySlot InSlot) -> FString
    {
        switch (InSlot)
        {
            case EPlayerMappableKeySlot::First:   return TEXT("First");
            case EPlayerMappableKeySlot::Second:  return TEXT("Second");
            case EPlayerMappableKeySlot::Third:   return TEXT("Third");
            case EPlayerMappableKeySlot::Fourth:  return TEXT("Fourth");
            default:                              return TEXT("?");
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInputDebugger_BindingsSnapshot::
    Gather(
        APlayerController* InPlayerController)
    -> FCkInputDebugger_BindingsSnapshot
{
    using namespace ck_input_debugger_bindings;

    auto Snapshot = FCkInputDebugger_BindingsSnapshot{};

    if (ck::Is_NOT_Valid(InPlayerController))
    { return Snapshot; }

    auto ScopeTagsByName = TMap<FName, FString>{};
    auto SignatureBuilder = TStringBuilder<2048>{};

    for (const auto& Mapping : UCk_Utils_KeyBinding_UE::Get_AllRemappableKeys(InPlayerController))
    {
        const auto MappingName = Mapping.GetMappingName();

        auto Row = FCkInputDebugger_BindingRow{};
        Row.MappingName = MappingName;
        Row.DisplayName = Mapping.GetDisplayName().ToString();
        Row.Category    = Mapping.GetDisplayCategory().ToString();
        Row.DefaultKey  = Mapping.GetDefaultKey();
        Row.CurrentKey  = Mapping.GetCurrentKey();
        Row.SlotLabel   = Get_SlotLabel(Mapping.GetSlot());
        Row.IsGamepad   = (Row.DefaultKey.IsValid() ? Row.DefaultKey : Row.CurrentKey).IsGamepadKey();
        Row.IsRebound   = Row.DefaultKey.IsValid() && Row.CurrentKey != Row.DefaultKey;

        if (const auto* CachedScopeTags = ScopeTagsByName.Find(MappingName))
        { Row.ScopeTags = *CachedScopeTags; }
        else
        {
            if (const auto* CkSettings = Cast<UCk_PlayerMappableKeySettings_UE>(
                    UCk_Utils_KeyBinding_UE::Get_MappableSettingsForMapping(InPlayerController, MappingName));
                ck::IsValid(CkSettings))
            { Row.ScopeTags = CkSettings->Get_ScopeTags().ToStringSimple(); }

            ScopeTagsByName.Emplace(MappingName, Row.ScopeTags);
        }

        if (NOT Snapshot.RowsByCategory.Contains(Row.Category))
        {
            Snapshot.CategoryOrder.Emplace(Row.Category);
            Snapshot.RowsByCategory.Emplace(Row.Category);
        }

        SignatureBuilder << MappingName << TEXT('|') << Row.DefaultKey.GetFName() << TEXT('|')
                         << Row.CurrentKey.GetFName() << TEXT('|') << Row.Category << TEXT(';');

        Snapshot.RowsByCategory[Row.Category].Emplace(MoveTemp(Row));
    }

    Snapshot.Signature = SignatureBuilder.ToString();
    return Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
