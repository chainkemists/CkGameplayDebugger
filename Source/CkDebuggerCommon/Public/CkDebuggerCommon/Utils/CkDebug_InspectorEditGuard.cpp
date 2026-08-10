#include "CkDebug_InspectorEditGuard.h"

#include "CkCore/Macros/CkMacros.h"

// ====================================================================================================================

auto
    FCkInspectorEditGuard::
    Claim_EditKey()
    -> FEditKey
{
    return _NextKey++;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspectorEditGuard::
    Set_EditActive(
        FEditKey InKey,
        bool     InIsActive)
    -> void
{
    if (InKey == 0)
    { return; }

    if (InIsActive)
    { _ActiveEdits.Add(InKey); }
    else
    { _ActiveEdits.Remove(InKey); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspectorEditGuard::
    Get_HasActiveEdit() const
    -> bool
{
    return _ActiveEdits.Num() > 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspectorEditGuard::
    Get_ActiveEditCount() const
    -> int32
{
    return _ActiveEdits.Num();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspectorEditGuard::
    Clear_AllEdits()
    -> void
{
    _ActiveEdits.Reset();
}

// ====================================================================================================================

auto
    FCkInspectorEditGuard::
    Request_Rebuild()
    -> void
{
    _HasPendingRebuild = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspectorEditGuard::
    Get_HasPendingRebuild() const
    -> bool
{
    return _HasPendingRebuild;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspectorEditGuard::
    Consume_PendingRebuild()
    -> bool
{
    // The whole contract in three lines: an edit in flight leaves the request PENDING (it is not
    // dropped and not answered), and the first consume after the edit ends hands it over.
    if (NOT _HasPendingRebuild)
    { return false; }

    if (Get_HasActiveEdit())
    { return false; }

    _HasPendingRebuild = false;
    return true;
}

// ====================================================================================================================

FCkInspectorEditScope::
    FCkInspectorEditScope(
        TSharedPtr<FCkInspectorEditGuard> InGuard)
    : _Guard(InGuard)
{
    if (InGuard.IsValid())
    { _Key = InGuard->Claim_EditKey(); }
}

// --------------------------------------------------------------------------------------------------------------------

FCkInspectorEditScope::
    ~FCkInspectorEditScope()
{
    Set_Active(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspectorEditScope::
    Set_Active(
        bool InIsActive)
    -> void
{
    if (_IsActive == InIsActive)
    { return; }

    _IsActive = InIsActive;

    if (const auto Guard = _Guard.Pin();
        Guard.IsValid())
    { Guard->Set_EditActive(_Key, InIsActive); }
}

// ====================================================================================================================
