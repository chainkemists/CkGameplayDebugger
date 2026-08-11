#pragma once

class UObject;

namespace ck::debugger_settings
{
    /**
     * One-time, editor-only import for debugger preferences that moved from
     * EditorPerProjectUserSettings to the packaged per-user GameUserSettings store.
     *
     * The import is deliberately non-destructive: an existing runtime section wins.
     */
    CKDEBUGGERCOMMON_API auto
    Migrate_EditorUserSettingsIfNeeded(
        UObject* InSettings) -> void;
}
