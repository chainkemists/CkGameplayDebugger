#include "CkPerfLab_Module.h"

#include "CkPerfLab_Log.h"

#define LOCTEXT_NAMESPACE "FCkPerfLabModule"

// --------------------------------------------------------------------------------------------------------------------

void FCkPerfLabModule::StartupModule()
{
    ck::perf_lab::Verbose(TEXT("CkPerfLab module started"));
}

void FCkPerfLabModule::ShutdownModule()
{
    ck::perf_lab::Verbose(TEXT("CkPerfLab module shut down"));
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkPerfLabModule, CkPerfLab)
