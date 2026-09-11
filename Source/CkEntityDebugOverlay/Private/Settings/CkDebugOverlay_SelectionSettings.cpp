#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_SelectionSettings.h"

#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ====================================================================================================================

namespace ck_debugoverlay_selection_settings
{
    constexpr int32 JsonSchemaVersion = 1;

    template<typename EnumType>
    auto Is_OneOf(EnumType InValue, std::initializer_list<EnumType> InAllowed) -> bool
    {
        for (const auto Allowed : InAllowed)
        {
            if (InValue == Allowed)
            { return true; }
        }
        return false;
    }

    auto Is_Valid(const FCk_DebugOverlay_SelectionConfig& InConfig, FString& OutError) -> bool
    {
        const auto ValidHierarchy = Is_OneOf(InConfig.Hierarchy, { ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots, ECk_DebugOverlay_SelectionHierarchy::LiteralRoots, ECk_DebugOverlay_SelectionHierarchy::AllEntities });
        if (NOT ValidHierarchy) { OutError = TEXT("Hierarchy is invalid."); return false; }
        const auto ValidAnchor = Is_OneOf(InConfig.RootAnchor, { ECk_DebugOverlay_SelectionRootAnchor::Member, ECk_DebugOverlay_SelectionRootAnchor::Root });
        if (NOT ValidAnchor) { OutError = TEXT("RootAnchor is invalid."); return false; }
        const auto ValidScope = Is_OneOf(InConfig.Scope, { ECk_DebugOverlay_SelectionScope::ViewWithNearbyFallback, ECk_DebugOverlay_SelectionScope::InView, ECk_DebugOverlay_SelectionScope::Nearby });
        if (NOT ValidScope) { OutError = TEXT("Scope is invalid."); return false; }
        const auto ValidTargeting = Is_OneOf(InConfig.Targeting, { ECk_DebugOverlay_SelectionTargeting::Weighted, ECk_DebugOverlay_SelectionTargeting::Cone });
        if (NOT ValidTargeting) { OutError = TEXT("Targeting is invalid."); return false; }
        const auto ValidOrder = Is_OneOf(InConfig.Order, { ECk_DebugOverlay_SelectionOrder::Score, ECk_DebugOverlay_SelectionOrder::Screen });
        if (NOT ValidOrder) { OutError = TEXT("Order is invalid."); return false; }
        const auto ValidStability = Is_OneOf(InConfig.Stability, { ECk_DebugOverlay_SelectionStability::Frozen, ECk_DebugOverlay_SelectionStability::Live });
        if (NOT ValidStability) { OutError = TEXT("Stability is invalid."); return false; }
        const auto ValidFamily = Is_OneOf(InConfig.Family, { ECk_DebugOverlay_SelectionFamily::Hold, ECk_DebugOverlay_SelectionFamily::Toggle });
        if (NOT ValidFamily) { OutError = TEXT("Family is invalid."); return false; }
        const auto ValidLabels = Is_OneOf(InConfig.Labels, { ECk_DebugOverlay_SelectionLabels::Full, ECk_DebugOverlay_SelectionLabels::Numbers, ECk_DebugOverlay_SelectionLabels::Shortlist });
        if (NOT ValidLabels) { OutError = TEXT("Labels is invalid."); return false; }
        const auto ValidNumbers = FMath::IsFinite(InConfig.ViewBias) && InConfig.ViewBias >= 0.0f && InConfig.ViewBias <= 1.0f &&
            FMath::IsFinite(InConfig.SearchRadius) && InConfig.SearchRadius >= 0.0f && InConfig.SearchRadius <= 100000.0f &&
            FMath::IsFinite(InConfig.ConeHalfAngle) && InConfig.ConeHalfAngle >= 1.0f && InConfig.ConeHalfAngle <= 90.0f &&
            FMath::IsFinite(InConfig.DiamondScale) && InConfig.DiamondScale >= 0.1f && InConfig.DiamondScale <= 5.0f;
        if (NOT ValidNumbers) { OutError = TEXT("Selection numeric tuning is outside its supported range."); return false; }
        OutError.Reset();
        return true;
    }

    auto Get_Preset(ECk_DebugOverlay_SelectionPreset InPreset, FCk_DebugOverlay_SelectionConfig& OutConfig) -> bool
    {
        OutConfig = {};
        switch (InPreset)
        {
            case ECk_DebugOverlay_SelectionPreset::FocusFamily:
                OutConfig.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots;
                OutConfig.RootAnchor = ECk_DebugOverlay_SelectionRootAnchor::Root;
                OutConfig.Scope = ECk_DebugOverlay_SelectionScope::ViewWithNearbyFallback;
                OutConfig.Targeting = ECk_DebugOverlay_SelectionTargeting::Weighted;
                OutConfig.Stability = ECk_DebugOverlay_SelectionStability::Frozen;
                OutConfig.Family = ECk_DebugOverlay_SelectionFamily::Hold;
                return true;
            case ECk_DebugOverlay_SelectionPreset::SpatialSweep:
                OutConfig.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::AllEntities;
                OutConfig.Scope = ECk_DebugOverlay_SelectionScope::Nearby;
                OutConfig.Targeting = ECk_DebugOverlay_SelectionTargeting::Weighted;
                OutConfig.ViewBias = 0.35f;
                OutConfig.SearchRadius = 10000.0f;
                OutConfig.Order = ECk_DebugOverlay_SelectionOrder::Screen;
                OutConfig.Stability = ECk_DebugOverlay_SelectionStability::Live;
                OutConfig.Labels = ECk_DebugOverlay_SelectionLabels::Numbers;
                return true;
            case ECk_DebugOverlay_SelectionPreset::AimCone:
                OutConfig.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots;
                OutConfig.Scope = ECk_DebugOverlay_SelectionScope::InView;
                OutConfig.Targeting = ECk_DebugOverlay_SelectionTargeting::Cone;
                OutConfig.ViewBias = 1.0f;
                OutConfig.ConeHalfAngle = 15.0f;
                OutConfig.ShowAimCone = true;
                return true;
        }
        return false;
    }

    auto Are_Equal(const FCk_DebugOverlay_SelectionConfig& InLeft, const FCk_DebugOverlay_SelectionConfig& InRight) -> bool
    {
        return InLeft.Hierarchy == InRight.Hierarchy && InLeft.RootAnchor == InRight.RootAnchor &&
            InLeft.Scope == InRight.Scope && InLeft.Targeting == InRight.Targeting &&
            FMath::IsNearlyEqual(InLeft.ViewBias, InRight.ViewBias) &&
            FMath::IsNearlyEqual(InLeft.SearchRadius, InRight.SearchRadius) &&
            FMath::IsNearlyEqual(InLeft.ConeHalfAngle, InRight.ConeHalfAngle) &&
            InLeft.Order == InRight.Order && InLeft.Stability == InRight.Stability &&
            InLeft.Family == InRight.Family && InLeft.Labels == InRight.Labels &&
            InLeft.ShowAimCone == InRight.ShowAimCone && InLeft.IncludeOccluded == InRight.IncludeOccluded &&
            FMath::IsNearlyEqual(InLeft.DiamondScale, InRight.DiamondScale);
    }

    auto Set_ConfigJson(const FCk_DebugOverlay_SelectionConfig& InConfig, const TSharedRef<FJsonObject>& InObject) -> void
    {
        InObject->SetNumberField(TEXT("Hierarchy"), static_cast<uint8>(InConfig.Hierarchy));
        InObject->SetNumberField(TEXT("RootAnchor"), static_cast<uint8>(InConfig.RootAnchor));
        InObject->SetNumberField(TEXT("Scope"), static_cast<uint8>(InConfig.Scope));
        InObject->SetNumberField(TEXT("Targeting"), static_cast<uint8>(InConfig.Targeting));
        InObject->SetNumberField(TEXT("ViewBias"), InConfig.ViewBias);
        InObject->SetNumberField(TEXT("SearchRadius"), InConfig.SearchRadius);
        InObject->SetNumberField(TEXT("ConeHalfAngle"), InConfig.ConeHalfAngle);
        InObject->SetNumberField(TEXT("Order"), static_cast<uint8>(InConfig.Order));
        InObject->SetNumberField(TEXT("Stability"), static_cast<uint8>(InConfig.Stability));
        InObject->SetNumberField(TEXT("Family"), static_cast<uint8>(InConfig.Family));
        InObject->SetNumberField(TEXT("Labels"), static_cast<uint8>(InConfig.Labels));
        InObject->SetBoolField(TEXT("ShowAimCone"), InConfig.ShowAimCone);
        InObject->SetBoolField(TEXT("IncludeOccluded"), InConfig.IncludeOccluded);
        InObject->SetNumberField(TEXT("DiamondScale"), InConfig.DiamondScale);
    }

    auto TryGet_Number(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InName, double& OutValue) -> bool
    { return InObject.IsValid() && InObject->TryGetNumberField(InName, OutValue); }

    auto TryGet_Enum(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InName, uint8& OutValue) -> bool
    {
        double Value = 0.0;
        const auto ValidNumber = TryGet_Number(InObject, InName, Value) && FMath::IsFinite(Value) &&
            Value >= 0.0 && Value <= static_cast<double>(MAX_uint8) && FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value));
        if (NOT ValidNumber)
        { return false; }
        OutValue = static_cast<uint8>(Value);
        return true;
    }

    auto Get_ConfigJson(const TSharedPtr<FJsonObject>& InObject, FCk_DebugOverlay_SelectionConfig& OutConfig, FString& OutError) -> bool
    {
        double Value = 0.0;
        uint8 EnumValue = 0;
        bool BoolValue = false;
        if (NOT TryGet_Enum(InObject, TEXT("Hierarchy"), EnumValue)) { OutError = TEXT("Config.Hierarchy is required."); return false; } OutConfig.Hierarchy = static_cast<ECk_DebugOverlay_SelectionHierarchy>(EnumValue);
        if (NOT TryGet_Enum(InObject, TEXT("RootAnchor"), EnumValue)) { OutError = TEXT("Config.RootAnchor is required."); return false; } OutConfig.RootAnchor = static_cast<ECk_DebugOverlay_SelectionRootAnchor>(EnumValue);
        if (NOT TryGet_Enum(InObject, TEXT("Scope"), EnumValue)) { OutError = TEXT("Config.Scope is required."); return false; } OutConfig.Scope = static_cast<ECk_DebugOverlay_SelectionScope>(EnumValue);
        if (NOT TryGet_Enum(InObject, TEXT("Targeting"), EnumValue)) { OutError = TEXT("Config.Targeting is required."); return false; } OutConfig.Targeting = static_cast<ECk_DebugOverlay_SelectionTargeting>(EnumValue);
        if (NOT TryGet_Number(InObject, TEXT("ViewBias"), Value)) { OutError = TEXT("Config.ViewBias is required."); return false; } OutConfig.ViewBias = static_cast<float>(Value);
        if (NOT TryGet_Number(InObject, TEXT("SearchRadius"), Value)) { OutError = TEXT("Config.SearchRadius is required."); return false; } OutConfig.SearchRadius = static_cast<float>(Value);
        if (NOT TryGet_Number(InObject, TEXT("ConeHalfAngle"), Value)) { OutError = TEXT("Config.ConeHalfAngle is required."); return false; } OutConfig.ConeHalfAngle = static_cast<float>(Value);
        if (NOT TryGet_Enum(InObject, TEXT("Order"), EnumValue)) { OutError = TEXT("Config.Order is required."); return false; } OutConfig.Order = static_cast<ECk_DebugOverlay_SelectionOrder>(EnumValue);
        if (NOT TryGet_Enum(InObject, TEXT("Stability"), EnumValue)) { OutError = TEXT("Config.Stability is required."); return false; } OutConfig.Stability = static_cast<ECk_DebugOverlay_SelectionStability>(EnumValue);
        if (NOT TryGet_Enum(InObject, TEXT("Family"), EnumValue)) { OutError = TEXT("Config.Family is required."); return false; } OutConfig.Family = static_cast<ECk_DebugOverlay_SelectionFamily>(EnumValue);
        if (NOT TryGet_Enum(InObject, TEXT("Labels"), EnumValue)) { OutError = TEXT("Config.Labels is required."); return false; } OutConfig.Labels = static_cast<ECk_DebugOverlay_SelectionLabels>(EnumValue);
        if (NOT InObject->TryGetBoolField(TEXT("ShowAimCone"), BoolValue)) { OutError = TEXT("Config.ShowAimCone is required."); return false; } OutConfig.ShowAimCone = BoolValue;
        if (NOT InObject->TryGetBoolField(TEXT("IncludeOccluded"), BoolValue)) { OutError = TEXT("Config.IncludeOccluded is required."); return false; } OutConfig.IncludeOccluded = BoolValue;
        if (NOT TryGet_Number(InObject, TEXT("DiamondScale"), Value)) { OutError = TEXT("Config.DiamondScale is required."); return false; } OutConfig.DiamondScale = static_cast<float>(Value);
        return Is_Valid(OutConfig, OutError);
    }
}

// ====================================================================================================================

auto UCk_DebugOverlay_SelectionSettings::Get() -> const UCk_DebugOverlay_SelectionSettings*
{ return GetDefault<UCk_DebugOverlay_SelectionSettings>(); }

auto UCk_DebugOverlay_SelectionSettings::Get_Mutable() -> UCk_DebugOverlay_SelectionSettings*
{ return GetMutableDefault<UCk_DebugOverlay_SelectionSettings>(); }

auto UCk_DebugOverlay_SelectionSettings::PostInitProperties() -> void
{
    Super::PostInitProperties();
    if (IsTemplate())
    { Validate_LoadedConfig(); }
}

auto UCk_DebugOverlay_SelectionSettings::PostReloadConfig(FProperty* InPropertyThatWasLoaded) -> void
{
    Super::PostReloadConfig(InPropertyThatWasLoaded);
    if (IsTemplate())
    { Validate_LoadedConfig(); }
}

auto UCk_DebugOverlay_SelectionSettings::TryValidate_Config(const FCk_DebugOverlay_SelectionConfig& InCandidate, FString& OutError) -> bool
{ return ck_debugoverlay_selection_settings::Is_Valid(InCandidate, OutError); }

auto UCk_DebugOverlay_SelectionSettings::Fail(FString InError) -> bool
{
    _LastError = MoveTemp(InError);
    return false;
}

auto UCk_DebugOverlay_SelectionSettings::Validate_LoadedConfig() -> void
{
    FString Error;
    const auto ValidConfig = TryValidate_Config(Config, Error);
    if (NOT ValidConfig)
    {
        Config = FCk_DebugOverlay_SelectionConfig{};
        _LastError = FString::Printf(TEXT("Selection settings were invalid at load and are using runtime defaults: %s"), *Error);
        ++_Revision;
    }
    // The previous 3500 cm default had no persisted version/provenance marker. It is a valid
    // explicit preference as well, so load validation must preserve it rather than guessing.

    auto InvalidPresetNames = TArray<FName>{};
    for (const auto& Pair : NamedPresets)
    {
        FString PresetError;
        const auto ValidPreset = TryValidate_Config(Pair.Value, PresetError);
        if (NOT ValidPreset)
        { InvalidPresetNames.Add(Pair.Key); }
    }
    if (NOT InvalidPresetNames.IsEmpty())
    {
        for (const auto PresetName : InvalidPresetNames)
        { NamedPresets.Remove(PresetName); }
        if (_LastError.IsEmpty())
        {
            _LastError = FString::Printf(TEXT("%d corrupt named selection preset(s) were rejected at load."), InvalidPresetNames.Num());
        }
        else
        {
            _LastError += FString::Printf(TEXT(" %d corrupt named selection preset(s) were rejected."), InvalidPresetNames.Num());
        }
        ++_Revision;
    }
}

auto UCk_DebugOverlay_SelectionSettings::Commit(bool InSave) -> void
{
    ++_Revision;
    _LastError.Reset();
    if (InSave)
    { SaveConfig(); }
}

auto UCk_DebugOverlay_SelectionSettings::TrySet_Config(const FCk_DebugOverlay_SelectionConfig& InCandidate, bool InSave) -> bool
{
    FString Error;
    const auto Valid = TryValidate_Config(InCandidate, Error);
    if (NOT Valid)
    { return Fail(MoveTemp(Error)); }
    if (ck_debugoverlay_selection_settings::Are_Equal(Config, InCandidate))
    { _LastError.Reset(); return true; }
    Config = InCandidate;
    Commit(InSave);
    return true;
}

auto UCk_DebugOverlay_SelectionSettings::ApplyPreset(ECk_DebugOverlay_SelectionPreset InPreset, bool InSave) -> bool
{
    auto Candidate = FCk_DebugOverlay_SelectionConfig{};
    const auto ValidPreset = ck_debugoverlay_selection_settings::Get_Preset(InPreset, Candidate);
    if (NOT ValidPreset)
    { return Fail(TEXT("Selection preset is invalid.")); }
    return TrySet_Config(Candidate, InSave);
}

auto UCk_DebugOverlay_SelectionSettings::Reset_Config(bool InSave) -> void
{ TrySet_Config({}, InSave); }

auto UCk_DebugOverlay_SelectionSettings::TrySave_NamedPreset(FName InName, bool InSave) -> bool
{
    const auto ValidName = !InName.IsNone();
    if (NOT ValidName)
    { return Fail(TEXT("Preset names cannot be empty.")); }
    FString Error;
    const auto ValidConfig = TryValidate_Config(Config, Error);
    if (NOT ValidConfig)
    { return Fail(MoveTemp(Error)); }
    NamedPresets.Add(InName, Config);
    Commit(InSave);
    return true;
}

auto UCk_DebugOverlay_SelectionSettings::TryApply_NamedPreset(FName InName, bool InSave) -> bool
{
    const auto* Candidate = NamedPresets.Find(InName);
    const auto Found = Candidate != nullptr;
    if (NOT Found)
    { return Fail(TEXT("Named selection preset was not found.")); }
    return TrySet_Config(*Candidate, InSave);
}

auto UCk_DebugOverlay_SelectionSettings::Export_NamedPresetJson(FName InName, FString& OutJson) const -> bool
{
    const auto* Candidate = NamedPresets.Find(InName);
    if (Candidate == nullptr || InName.IsNone())
    { OutJson.Reset(); return false; }
    auto Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("SchemaVersion"), ck_debugoverlay_selection_settings::JsonSchemaVersion);
    Root->SetStringField(TEXT("Name"), InName.ToString());
    auto ConfigObject = MakeShared<FJsonObject>();
    ck_debugoverlay_selection_settings::Set_ConfigJson(*Candidate, ConfigObject);
    Root->SetObjectField(TEXT("Config"), ConfigObject);
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
    return FJsonSerializer::Serialize(Root, Writer);
}

auto UCk_DebugOverlay_SelectionSettings::TryImport_NamedPresetJson(const FString& InJson, FName& OutImportedName, bool InSave) -> bool
{
    OutImportedName = NAME_None;
    TSharedPtr<FJsonObject> Root;
    const auto Reader = TJsonReaderFactory<TCHAR>::Create(InJson);
    const auto Parsed = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
    if (NOT Parsed)
    { return Fail(TEXT("Selection preset JSON is invalid.")); }
    double SchemaVersion = 0.0;
    FString Name;
    const TSharedPtr<FJsonObject>* ConfigObject = nullptr;
    const auto ValidEnvelope = Root->TryGetNumberField(TEXT("SchemaVersion"), SchemaVersion) &&
        Root->TryGetStringField(TEXT("Name"), Name) && Root->TryGetObjectField(TEXT("Config"), ConfigObject) &&
        ConfigObject != nullptr && ConfigObject->IsValid() && SchemaVersion == ck_debugoverlay_selection_settings::JsonSchemaVersion && !Name.IsEmpty();
    if (NOT ValidEnvelope)
    { return Fail(TEXT("Selection preset JSON has an unsupported schema.")); }
    auto Candidate = FCk_DebugOverlay_SelectionConfig{};
    FString Error;
    const auto ValidConfig = ck_debugoverlay_selection_settings::Get_ConfigJson(*ConfigObject, Candidate, Error);
    if (NOT ValidConfig)
    { return Fail(MoveTemp(Error)); }
    const FName ImportedName{Name};
    const auto ValidName = !ImportedName.IsNone();
    if (NOT ValidName)
    { return Fail(TEXT("Selection preset JSON has an invalid name.")); }
    NamedPresets.Add(ImportedName, Candidate);
    OutImportedName = ImportedName;
    Commit(InSave);
    return true;
}

// ====================================================================================================================
