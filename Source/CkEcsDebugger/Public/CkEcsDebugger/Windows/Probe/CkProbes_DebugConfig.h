#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

#include "CkProbes_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

struct FCk_Handle_Probe;

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_Probes_DebugWindowConfig : public UCogCommonConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Probes_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    auto
    Get_ProbeColor(
        const FCk_Handle_Probe& InProbe) const -> FVector4f;

private:
    inline static FVector4f _EnabledColor{0.0f, 1.0f, 0.5f, 1.0f};
    inline static FVector4f _DisabledColor{1.0f, 0.5f, 0.5f, 1.0f};
    inline static FVector4f _OverlappingColor{1.0f, 0.95f, 0.0f, 1.0f};
    inline static FVector4f _NotifyColor{0.0f, 0.8f, 1.0f, 1.0f};
    inline static FVector4f _SilentColor{0.6f, 0.6f, 0.6f, 1.0f};

public:
    UPROPERTY(Config)
    bool _SortByName = false;

    UPROPERTY(Config)
    bool _ShowEnabled = true;

    UPROPERTY(Config)
    bool _ShowDisabled = true;

    UPROPERTY(Config)
    bool _ShowOverlapping = true;

    UPROPERTY(Config)
    bool _ShowNotOverlapping = true;

    UPROPERTY(Config)
    bool _ShowNotifyPolicy = true;

    UPROPERTY(Config)
    bool _ShowSilentPolicy = true;

    UPROPERTY(Config)
    bool _AlwaysDrawDebug = true;

    UPROPERTY(Config)
    FVector4f EnabledColor = _EnabledColor;

    UPROPERTY(Config)
    FVector4f DisabledColor = _DisabledColor;

    UPROPERTY(Config)
    FVector4f OverlappingColor = _OverlappingColor;

    UPROPERTY(Config)
    FVector4f NotifyColor = _NotifyColor;

    UPROPERTY(Config)
    FVector4f SilentColor = _SilentColor;
};