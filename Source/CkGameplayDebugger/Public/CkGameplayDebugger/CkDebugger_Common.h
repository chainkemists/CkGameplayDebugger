#pragma once

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include <InputCoreTypes.h>

#include "CkDebugger_Common.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class FCk_GameplayDebugger_BackgroundWidth : uint8
{
    OneThird UMETA(DislayName = "1/3 Viewport Width"),
    Half UMETA(DislayName = "1/2 Viewport Width"),
    ThreeFourth UMETA(DislayName = "3/4 Viewport Width"),
    Full UMETA(DislayName = "Full Viewport Width"),
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(FCk_GameplayDebugger_BackgroundWidth);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_DebugNavControls
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_DebugNavControls);

private:
    // Key to hold down to enable Debugger keyboard controls
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _StickyModiferKey = EKeys::Insert;

    // Key to press to select the next filter in the list of filters available
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _NextFilterKey = EKeys::Up;

    // Key to press to select the previous filter in the list of filters available
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _PreviousFilterKey = EKeys::Down;

    // Key to press to select the next actor in the filtered lists of actor
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _NextActorKey = EKeys::Right;

    // Key to press to select the previous actor in the filtered lists of actor
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _PreviousActorKey = EKeys::Left;

    // Key to press to select the first actor in the filtered lists of actor
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _FirstActorKey = EKeys::Home;

    // Key to press to select the last actor in the filtered lists of actor
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _LastActorKey = EKeys::End;

    // Key to press to select the next UWorld
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _NextWorldKey = EKeys::PageUp;

    // Key to press to select the previous UWorld
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _PrevWorldKey = EKeys::PageDown;

    // Key to press to toggle between the Server UWorld and the FIRST Client UWorld
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FKey _ServerClientWorldToggleKey = EKeys::Delete;

public:
    CK_PROPERTY_GET(_StickyModiferKey);
    CK_PROPERTY_GET(_NextFilterKey);
    CK_PROPERTY_GET(_PreviousFilterKey);
    CK_PROPERTY_GET(_NextActorKey);
    CK_PROPERTY_GET(_PreviousActorKey);
    CK_PROPERTY_GET(_FirstActorKey);
    CK_PROPERTY_GET(_LastActorKey);
    CK_PROPERTY_GET(_NextWorldKey);
    CK_PROPERTY_GET(_PrevWorldKey);
    CK_PROPERTY_GET(_ServerClientWorldToggleKey);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_DebugActorList
{
    GENERATED_BODY()

public:
    FCk_GameplayDebugger_DebugActorList() = default;
    explicit FCk_GameplayDebugger_DebugActorList(
        TArray<AActor*> InActors);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<AActor>> _DebugActors;

public:
    CK_PROPERTY_GET(_DebugActors);
};

// --------------------------------------------------------------------------------------------------------------------
