#include "CkPerfLab_SessionStore.h"

#include "CkPerfLab_Log.h"

#include "CkPerfLab/Host/CkPerfLab_Subprocess.h"
#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_store
{
    /**
     * A session id names ONE directory directly under the store. Validating that here rather than
     * relying only on comparing resolved paths is deliberate: an empty id resolves to the store root
     * itself, and a containment check written as "not equal to root" misses it the moment either
     * path carries a trailing separator — which would delete the entire store rather than one
     * session. The id is therefore rejected before it is ever turned into a path.
     */
    auto
        Get_IsWellFormedSessionId(
            const FString& InSessionId)
        -> bool
    {
        if (InSessionId.TrimStartAndEnd().IsEmpty())
        {
            return false;
        }

        if (InSessionId.Contains(TEXT("/")) || InSessionId.Contains(TEXT("\\")))
        {
            return false;
        }

        // Catches "..", "../x" and the bare "." — anything that could climb or self-reference.
        if (InSessionId.Contains(TEXT("..")) || InSessionId == TEXT("."))
        {
            return false;
        }

        return true;
    }

    /** Compare paths without letting a trailing separator decide the answer. */
    auto
        Get_Normalised(
            const FString& InPath)
        -> FString
    {
        auto Normalised = FPaths::ConvertRelativePathToFull(InPath);
        FPaths::NormalizeDirectoryName(Normalised);
        return Normalised;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Get_SessionRows()
        -> TArray<FCk_PerfLab_SessionRow>
    {
        auto Rows = TArray<FCk_PerfLab_SessionRow>{};

        auto Directories = TArray<FString>{};
        IFileManager::Get().FindFiles(Directories, *(Get_SessionsRoot() / TEXT("*")), false, true);

        for (const auto& Directory : Directories)
        {
            auto Row = FCk_PerfLab_SessionRow{}.Set_SessionId(Directory);

            auto Json = FString{};

            if (FFileHelper::LoadFileToString(Json,
                    *Get_SessionFilePath(FPaths::Combine(Get_SessionsRoot(), Directory))))
            {
                auto Summary = FCk_PerfLab_Session{};

                // The summary path reads only the header, so listing thirty sessions does not decode
                // thirty sets of measurements to draw thirty rows.
                if (TryRead_SessionSummaryFromJson(Json, Summary))
                {
                    Row.Set_MapPath(Summary.Get_Request().Get_MapPath())
                       .Set_State(Summary.Get_State())
                       .Set_IsReadable(true);
                }
            }

            Rows.Add(Row);
        }

        // Ids begin with a UTC timestamp, so descending id is newest first.
        ck::algo::Sort(Rows, [](const FCk_PerfLab_SessionRow& InA, const FCk_PerfLab_SessionRow& InB)
        {
            return InA.Get_SessionId() > InB.Get_SessionId();
        });

        return Rows;
    }

    auto
        TryLoad_Session(
            const FString& InSessionId,
            FCk_PerfLab_Session& OutSession)
        -> bool
    {
        auto Json = FString{};

        if (NOT FFileHelper::LoadFileToString(Json,
                *Get_SessionFilePath(Get_SessionDirFor(InSessionId))))
        {
            return false;
        }

        return TryRead_SessionFromJson(Json, OutSession);
    }

    auto
        Try_DeleteSession(
            const FString& InSessionId)
        -> bool
    {
        if (NOT ck_perf_lab_store::Get_IsWellFormedSessionId(InSessionId))
        {
            ck::perf_lab::Error(TEXT("Refusing to delete [{}]: it is not a well-formed session id"), InSessionId);
            return false;
        }

        const auto Root   = ck_perf_lab_store::Get_Normalised(Get_SessionsRoot());
        const auto Target = ck_perf_lab_store::Get_Normalised(Get_SessionDirFor(InSessionId));

        // Belt and braces behind the id validation above: even a well-formed id must resolve to a
        // STRICT child of the store. Comparing normalised paths and requiring the separator is what
        // closes the trailing-slash hole that let an empty id delete the store root itself.
        if (Target == Root || NOT Target.StartsWith(Root + TEXT("/")))
        {
            ck::perf_lab::Error(TEXT("Refusing to delete [{}]: it resolves outside the session store"), Target);
            return false;
        }

        if (NOT IFileManager::Get().DirectoryExists(*Target))
        {
            return false;
        }

        return IFileManager::Get().DeleteDirectory(*Target, false, true);
    }
}

// --------------------------------------------------------------------------------------------------------------------
