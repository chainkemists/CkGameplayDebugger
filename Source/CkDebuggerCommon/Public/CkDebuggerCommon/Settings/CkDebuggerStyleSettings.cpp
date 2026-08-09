#include "CkDebuggerStyleSettings.h"

#include "CkCore/Format/CkFormat.h"

// ====================================================================================================================

DEFINE_LOG_CATEGORY_STATIC(LogCkDebuggerStyle, Log, All);

// ====================================================================================================================

UCkDebuggerStyleSettings::
    UCkDebuggerStyleSettings()
{
    CategoryName = TEXT("Ck");
    SectionName  = TEXT("Debugger Style");
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkDebuggerStyleSettings::
    NotifyChanged()
    -> void
{
    ++_Revision;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkDebuggerStyleSettings::
    PostInitProperties()
    -> void
{
    Super::PostInitProperties();

    if (SchemaVersion < CurrentSchemaVersion)
    {
        UE_LOG(LogCkDebuggerStyle, Warning, TEXT("%s"), *ck::Format_UE(
            TEXT("Debugger style config was written against schema version {} (current is {}). Axes that still parse are kept; "
                 "renamed or removed axes fall back to their default. Re-save Editor Preferences -> Ck -> Debugger Style to clear this."),
            SchemaVersion, CurrentSchemaVersion));
    }

    SchemaVersion = CurrentSchemaVersion;
}

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR
auto
    UCkDebuggerStyleSettings::
    PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
    -> void
{
    Super::PostEditChangeProperty(InPropertyChangedEvent);

    NotifyChanged();
}
#endif

// ====================================================================================================================
