#pragma once

#include "CkPerfLab/Session/CkPerfLab_Session.h"

#include "CkPerfLab_SessionStore.generated.h"

// --------------------------------------------------------------------------------------------------------------------

/** One row in the session list: enough to draw it, not enough to have cost anything. */
USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_SessionRow
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_SessionRow);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _SessionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _MapPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_RunState _State = ECk_PerfLab_RunState::Booting;

    /** False when the directory exists but its session could not be decoded. Shown, never hidden. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    bool _IsReadable = false;

public:
    CK_PROPERTY(_SessionId);
    CK_PROPERTY(_MapPath);
    CK_PROPERTY(_State);
    CK_PROPERTY(_IsReadable);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    /**
     * List every session on disk, newest id first.
     *
     * A directory whose session will not decode still produces a row, marked unreadable. Silently
     * dropping it would leave a user staring at a folder the tool claims does not exist.
     */
    CKPERFLAB_API auto
    Get_SessionRows() -> TArray<FCk_PerfLab_SessionRow>;

    /** Load one session in full. */
    CKPERFLAB_API auto
    TryLoad_Session(
        const FString& InSessionId,
        FCk_PerfLab_Session& OutSession) -> bool;

    /**
     * Delete one session's directory.
     *
     * Refuses anything that does not resolve inside the store's own root, so a malformed or hostile
     * id cannot be turned into a delete somewhere else.
     */
    CKPERFLAB_API auto
    Try_DeleteSession(
        const FString& InSessionId) -> bool;

    /** The root every session lives under. */
    CKPERFLAB_API auto
    Get_SessionsRoot() -> FString;
}

// --------------------------------------------------------------------------------------------------------------------
