#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

namespace ck::texture_debugger::asset_generation
{
enum class EMode : uint8
{
    ValidateOnly,
    Bootstrap,
};

struct FResult
{
    bool           Succeeded = false;
    TArray<FString> Errors;
    TArray<FString> GeneratedPackageNames;
};

CKTEXTUREDEBUGGER_API auto
Run(EMode InMode) -> FResult;
}

#endif
