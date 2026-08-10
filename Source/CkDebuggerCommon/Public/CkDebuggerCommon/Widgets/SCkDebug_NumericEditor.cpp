#include "SCkDebug_NumericEditor.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"

// ====================================================================================================================

namespace ck_debug_numeric_editor
{
    constexpr auto MaxFractionalDigits = 6;

    auto Get_Justification() -> ETextJustify::Type
    {
        const auto& Selection = UCkDebuggerStyleSettings::Get_Selection();

        return ck::debug_axes::Values_AlignRight(Selection) || ck::debug_axes::Values_UseAlignedColumns(Selection)
            ? ETextJustify::Right
            : ETextJustify::Left;
    }
}

// ====================================================================================================================

auto
    SCkDebug_NumericEditor::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Value              = InArgs._Value;
    _ForegroundColor    = InArgs._ForegroundColor;
    _Kind               = InArgs._Kind;
    _MinValue           = InArgs._MinValue;
    _MaxValue           = InArgs._MaxValue;
    _FractionalDigits   = FMath::Clamp(InArgs._FractionalDigits, 0, ck_debug_numeric_editor::MaxFractionalDigits);
    _OnValueCommitted   = InArgs._OnValueCommitted;
    _OnEditStateChanged = InArgs._OnEditStateChanged;

    const auto Width = InArgs._Width > 0.0f ? InArgs._Width : DefaultWidth;

    const auto ForegroundColor = _ForegroundColor;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(Width)
        [
            SAssignNew(_TextBox, SEditableTextBox)
            .Text(TAttribute<FText>::CreateSP(this, &SCkDebug_NumericEditor::DoGetDisplayText))
            .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
            .Justification_Lambda([]() { return ck_debug_numeric_editor::Get_Justification(); })
            .ForegroundColor_Lambda([ForegroundColor]()
            {
                return FSlateColor{ForegroundColor.IsSet() ? ForegroundColor.Get() : CkStyle::Text()};
            })
            .SelectAllTextWhenFocused(true)
            .RevertTextOnEscape(true)
            .OnBeginTextEdit(this, &SCkDebug_NumericEditor::DoHandleBeginTextEdit)
            .OnTextChanged(this, &SCkDebug_NumericEditor::DoHandleTextChanged)
            .OnTextCommitted(this, &SCkDebug_NumericEditor::DoHandleTextCommitted)
        ]
    ];
}

// ====================================================================================================================

auto
    SCkDebug_NumericEditor::
    DoGetDisplayText() const
    -> FText
{
    // Frozen while typing: the bound live value must not overwrite half-typed text.
    if (_IsEditing)
    { return _FrozenText; }

    return FText::FromString(Format_Value(_Value.Get(0.0), _Kind, _FractionalDigits));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_NumericEditor::
    DoHandleBeginTextEdit(
        const FText& InText)
    -> void
{
    // The edit window opens at FOCUS, not at the first keystroke: a rebuild while the field merely
    // holds focus would still destroy it under the user (and eat the selection).
    _FrozenText = InText;
    DoSetEditing(true);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_NumericEditor::
    DoHandleTextChanged(
        const FText& InText)
    -> void
{
    _FrozenText = InText;

    // Whether anything was actually TYPED is a separate question from whether the field is focused —
    // it is what decides if the focus-loss commit is a real write or a click passing through.
    _HasPendingChange = true;

    DoSetEditing(true);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_NumericEditor::
    DoHandleTextCommitted(
        const FText&      InText,
        ETextCommit::Type InCommitType)
    -> void
{
    // A focus-loss commit on a field the user never typed into is not an edit — firing a request
    // there would turn "click past a control" into a silent write.
    const auto HadChange = _HasPendingChange;

    _HasPendingChange = false;
    DoSetEditing(false);

    if (NOT HadChange)
    { return; }

    if (NOT Should_Commit(InCommitType))
    { return; }

    const auto Parsed = Parse_Value(InText.ToString(), _Kind);

    if (NOT Parsed.IsSet())
    { return; }

    _OnValueCommitted.ExecuteIfBound(Clamp_Value(Parsed.GetValue(), _MinValue, _MaxValue, _Kind));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_NumericEditor::
    DoSetEditing(
        bool InIsEditing)
    -> void
{
    if (_IsEditing == InIsEditing)
    { return; }

    _IsEditing = InIsEditing;

    if (NOT InIsEditing)
    { _FrozenText = FText::GetEmpty(); }

    _OnEditStateChanged.ExecuteIfBound(InIsEditing);
}

// ====================================================================================================================
// Pure helpers

auto
    SCkDebug_NumericEditor::
    Format_Value(
        double               InValue,
        ECkDebug_NumericKind InKind,
        int32                InFractionalDigits)
    -> FString
{
    if (InKind == ECkDebug_NumericKind::Integer)
    { return FString::Printf(TEXT("%lld"), static_cast<int64>(FMath::RoundToDouble(InValue))); }

    const auto Digits = FMath::Clamp(InFractionalDigits, 0, ck_debug_numeric_editor::MaxFractionalDigits);
    return FString::Printf(TEXT("%.*f"), Digits, InValue);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_NumericEditor::
    Parse_Value(
        const FString&       InText,
        ECkDebug_NumericKind InKind)
    -> TOptional<double>
{
    const auto Trimmed = InText.TrimStartAndEnd();

    if (Trimmed.IsEmpty())
    { return {}; }

    // Non-numeric text is a typo, not an intent to zero the value — leave the value untouched.
    if (NOT Trimmed.IsNumeric())
    { return {}; }

    const auto Parsed = FCString::Atod(*Trimmed);

    return InKind == ECkDebug_NumericKind::Integer
        ? TOptional<double>{FMath::RoundToDouble(Parsed)}
        : TOptional<double>{Parsed};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_NumericEditor::
    Clamp_Value(
        double                   InValue,
        const TOptional<double>& InMin,
        const TOptional<double>& InMax,
        ECkDebug_NumericKind     InKind)
    -> double
{
    auto Result = InValue;

    if (InMin.IsSet())
    { Result = FMath::Max(Result, InMin.GetValue()); }

    if (InMax.IsSet())
    { Result = FMath::Min(Result, InMax.GetValue()); }

    return InKind == ECkDebug_NumericKind::Integer ? FMath::RoundToDouble(Result) : Result;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_NumericEditor::
    Should_Commit(
        ETextCommit::Type InCommitType)
    -> bool
{
    return InCommitType == ETextCommit::OnEnter || InCommitType == ETextCommit::OnUserMovedFocus;
}

// ====================================================================================================================
