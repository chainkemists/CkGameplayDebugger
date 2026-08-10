#pragma once

#include "Misc/Optional.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;

// ====================================================================================================================
// THE numeric entry field for debugger surfaces. Promoted from CkGoapDebugger's file-local
// MakeNumericEditor (SCkGoapDebugger_AgentColumn.cpp) so inspector rows, the GOAP settings drawer,
// and anything else editing a live scalar behave identically.
//
// Contract:
//   - COMMIT ON ENTER / LOST FOCUS ONLY. Never per keystroke — a per-keystroke commit turns "12"
//     on the way to "125" into a real request, and a clamped one at that.
//   - The display value is an ATTRIBUTE, so it tracks the live entity between edits with no rebuild.
//     While the user is typing the attribute is FROZEN (see _IsEditing) so an incoming value cannot
//     stomp half-typed text.
//   - Min/Max are optional and clamp the COMMITTED value, not the typed text.
//   - Integer kind rounds and formats without a fractional part; float kind honours FractionalDigits.
//   - OnEditStateChanged brackets the type-in window. The inspector panel's FCkInspectorEditGuard
//     hangs off it to defer rebuilds; see CkDebug_InspectorEditGuard.h.
//
// Axis-aware sizing: with the default Width (0) the field takes the shared numeric column width so a
// row of three editors lines up with the read-only aligned numeric rows, and text justification
// follows the ValueAlignment axis.
// ====================================================================================================================

enum class ECkDebug_NumericKind : uint8
{
    Float,
    Integer,
};

DECLARE_DELEGATE_OneParam(FOnCkDebug_NumericCommitted, double /* CommittedValue */);
DECLARE_DELEGATE_OneParam(FOnCkDebug_NumericEditStateChanged, bool /* IsEditing */);

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_NumericEditor : public SCompoundWidget
{
public:
    // The width the field takes when Width is left at 0 — the same column budget the inspector's
    // aligned numeric rows use, so editable and read-only numeric rows share one grid.
    static constexpr auto DefaultWidth = 70.0f;

public:
    SLATE_BEGIN_ARGS(SCkDebug_NumericEditor)
        : _Kind(ECkDebug_NumericKind::Float)
        , _Width(0.0f)
        , _FractionalDigits(2)
    {}
        /** Live value shown between edits. */
        SLATE_ATTRIBUTE(double, Value)
        SLATE_ARGUMENT(ECkDebug_NumericKind, Kind)
        SLATE_ARGUMENT(TOptional<double>, MinValue)
        SLATE_ARGUMENT(TOptional<double>, MaxValue)
        /** 0 = the axis-aware default width. */
        SLATE_ARGUMENT(float, Width)
        SLATE_ARGUMENT(int32, FractionalDigits)
        /** Text tint — vector rows pass the X/Y/Z axis roles here. */
        SLATE_ATTRIBUTE(FLinearColor, ForegroundColor)
        SLATE_EVENT(FOnCkDebug_NumericCommitted, OnValueCommitted)
        SLATE_EVENT(FOnCkDebug_NumericEditStateChanged, OnEditStateChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

public:
    // ----- Pure helpers (shared with the specs) -------------------------------

    static auto Format_Value(double InValue, ECkDebug_NumericKind InKind, int32 InFractionalDigits) -> FString;

    /** Unset when the text is not a number at all — the caller then leaves the value untouched. */
    static auto Parse_Value(const FString& InText, ECkDebug_NumericKind InKind) -> TOptional<double>;

    static auto Clamp_Value(
        double                     InValue,
        const TOptional<double>&   InMin,
        const TOptional<double>&   InMax,
        ECkDebug_NumericKind       InKind) -> double;

    /** Enter and focus-loss commit; everything else (Escape, programmatic set) does not. */
    static auto Should_Commit(ETextCommit::Type InCommitType) -> bool;

private:
    auto DoGetDisplayText() const -> FText;
    auto DoHandleBeginTextEdit(const FText& InText) -> void;
    auto DoHandleTextChanged(const FText& InText) -> void;
    auto DoHandleTextCommitted(const FText& InText, ETextCommit::Type InCommitType) -> void;
    auto DoSetEditing(bool InIsEditing) -> void;

    TAttribute<double>       _Value;
    TAttribute<FLinearColor> _ForegroundColor;
    ECkDebug_NumericKind     _Kind = ECkDebug_NumericKind::Float;
    TOptional<double>        _MinValue;
    TOptional<double>        _MaxValue;
    int32                    _FractionalDigits = 2;

    FOnCkDebug_NumericCommitted        _OnValueCommitted;
    FOnCkDebug_NumericEditStateChanged _OnEditStateChanged;

    TSharedPtr<SEditableTextBox> _TextBox;

    // Frozen snapshot of the display text for the duration of a type-in, so the bound live value
    // cannot overwrite what the user is typing. _IsEditing spans the FOCUS window (what the edit
    // guard defers rebuilds on); _HasPendingChange says whether anything was actually typed (what
    // decides if the focus-loss commit is a real write).
    bool  _IsEditing = false;
    bool  _HasPendingChange = false;
    FText _FrozenText;
};

// ====================================================================================================================
