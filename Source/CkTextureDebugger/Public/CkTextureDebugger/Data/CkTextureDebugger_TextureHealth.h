#pragma once

#include "CkTextureDebugger/Data/CkTextureDebugger_Types.h"

class UTexture;

// --------------------------------------------------------------------------------------------------------------------

namespace ck::texture_debugger::health
{
    /** Does not construct a streaming manager. It is safe before manager startup and during teardown. */
    CKTEXTUREDEBUGGER_API auto Get_StreamingAvailability() -> ECkTextureDebugger_StreamingAvailability;

    /** Returns copied runtime/cooked facts only. Editor source facts belong in the editor-gated adapter. */
    CKTEXTUREDEBUGGER_API auto Describe(
        UTexture* InTexture,
        ECkTextureDebugger_StreamingAvailability InStreamingAvailability) -> FCkTextureDebugger_TextureHealth;
}
