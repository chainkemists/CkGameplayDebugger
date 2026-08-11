#include "CkDebuggerCommon/Devices/CkDebug_KeyboardLayout.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_keyboard_layout
{
    auto
        Build_AnsiFull()
        -> TArray<FCkDebug_DeviceKeyDef>
    {
        auto Defs = TArray<FCkDebug_DeviceKeyDef>{};
        Defs.Reserve(105);

        const auto Add = [&](const FKey& InKey, const TCHAR* InLabel, float InX, float InY, float InW = 1.0f, float InH = 1.0f)
        {
            Defs.Add(FCkDebug_DeviceKeyDef{InKey, InLabel, InX, InY, InW, InH});
        };

        // F-row. The half-unit gaps between the F-key quads are the physical board's own grouping.
        Add(EKeys::Escape, TEXT("Esc"), 0.0f, 0.0f);
        Add(EKeys::F1, TEXT("F1"), 2.0f, 0.0f);
        Add(EKeys::F2, TEXT("F2"), 3.0f, 0.0f);
        Add(EKeys::F3, TEXT("F3"), 4.0f, 0.0f);
        Add(EKeys::F4, TEXT("F4"), 5.0f, 0.0f);
        Add(EKeys::F5, TEXT("F5"), 6.5f, 0.0f);
        Add(EKeys::F6, TEXT("F6"), 7.5f, 0.0f);
        Add(EKeys::F7, TEXT("F7"), 8.5f, 0.0f);
        Add(EKeys::F8, TEXT("F8"), 9.5f, 0.0f);
        Add(EKeys::F9, TEXT("F9"), 11.0f, 0.0f);
        Add(EKeys::F10, TEXT("F10"), 12.0f, 0.0f);
        Add(EKeys::F11, TEXT("F11"), 13.0f, 0.0f);
        Add(EKeys::F12, TEXT("F12"), 14.0f, 0.0f);

        // Number row.
        Add(EKeys::Tilde, TEXT("`"), 0.0f, 1.5f);
        Add(EKeys::One, TEXT("1"), 1.0f, 1.5f);
        Add(EKeys::Two, TEXT("2"), 2.0f, 1.5f);
        Add(EKeys::Three, TEXT("3"), 3.0f, 1.5f);
        Add(EKeys::Four, TEXT("4"), 4.0f, 1.5f);
        Add(EKeys::Five, TEXT("5"), 5.0f, 1.5f);
        Add(EKeys::Six, TEXT("6"), 6.0f, 1.5f);
        Add(EKeys::Seven, TEXT("7"), 7.0f, 1.5f);
        Add(EKeys::Eight, TEXT("8"), 8.0f, 1.5f);
        Add(EKeys::Nine, TEXT("9"), 9.0f, 1.5f);
        Add(EKeys::Zero, TEXT("0"), 10.0f, 1.5f);
        Add(EKeys::Hyphen, TEXT("-"), 11.0f, 1.5f);
        Add(EKeys::Equals, TEXT("="), 12.0f, 1.5f);
        Add(EKeys::BackSpace, TEXT("Bksp"), 13.0f, 1.5f, 2.0f);

        // Top letter row.
        Add(EKeys::Tab, TEXT("Tab"), 0.0f, 2.5f, 1.5f);
        Add(EKeys::Q, TEXT("Q"), 1.5f, 2.5f);
        Add(EKeys::W, TEXT("W"), 2.5f, 2.5f);
        Add(EKeys::E, TEXT("E"), 3.5f, 2.5f);
        Add(EKeys::R, TEXT("R"), 4.5f, 2.5f);
        Add(EKeys::T, TEXT("T"), 5.5f, 2.5f);
        Add(EKeys::Y, TEXT("Y"), 6.5f, 2.5f);
        Add(EKeys::U, TEXT("U"), 7.5f, 2.5f);
        Add(EKeys::I, TEXT("I"), 8.5f, 2.5f);
        Add(EKeys::O, TEXT("O"), 9.5f, 2.5f);
        Add(EKeys::P, TEXT("P"), 10.5f, 2.5f);
        Add(EKeys::LeftBracket, TEXT("["), 11.5f, 2.5f);
        Add(EKeys::RightBracket, TEXT("]"), 12.5f, 2.5f);
        Add(EKeys::Backslash, TEXT("\\"), 13.5f, 2.5f, 1.5f);

        // Home row.
        Add(EKeys::CapsLock, TEXT("Caps"), 0.0f, 3.5f, 1.75f);
        Add(EKeys::A, TEXT("A"), 1.75f, 3.5f);
        Add(EKeys::S, TEXT("S"), 2.75f, 3.5f);
        Add(EKeys::D, TEXT("D"), 3.75f, 3.5f);
        Add(EKeys::F, TEXT("F"), 4.75f, 3.5f);
        Add(EKeys::G, TEXT("G"), 5.75f, 3.5f);
        Add(EKeys::H, TEXT("H"), 6.75f, 3.5f);
        Add(EKeys::J, TEXT("J"), 7.75f, 3.5f);
        Add(EKeys::K, TEXT("K"), 8.75f, 3.5f);
        Add(EKeys::L, TEXT("L"), 9.75f, 3.5f);
        Add(EKeys::Semicolon, TEXT(";"), 10.75f, 3.5f);
        Add(EKeys::Apostrophe, TEXT("'"), 11.75f, 3.5f);
        Add(EKeys::Enter, TEXT("Enter"), 12.75f, 3.5f, 2.25f);

        // Bottom letter row.
        Add(EKeys::LeftShift, TEXT("Shift"), 0.0f, 4.5f, 2.25f);
        Add(EKeys::Z, TEXT("Z"), 2.25f, 4.5f);
        Add(EKeys::X, TEXT("X"), 3.25f, 4.5f);
        Add(EKeys::C, TEXT("C"), 4.25f, 4.5f);
        Add(EKeys::V, TEXT("V"), 5.25f, 4.5f);
        Add(EKeys::B, TEXT("B"), 6.25f, 4.5f);
        Add(EKeys::N, TEXT("N"), 7.25f, 4.5f);
        Add(EKeys::M, TEXT("M"), 8.25f, 4.5f);
        Add(EKeys::Comma, TEXT(","), 9.25f, 4.5f);
        Add(EKeys::Period, TEXT("."), 10.25f, 4.5f);
        Add(EKeys::Slash, TEXT("/"), 11.25f, 4.5f);
        Add(EKeys::RightShift, TEXT("Shift"), 12.25f, 4.5f, 2.75f);

        // Modifier row. The Windows keys arrive as the engine's Command keys; the menu key has no engine key at
        // all and stays decorative.
        Add(EKeys::LeftControl, TEXT("Ctrl"), 0.0f, 5.5f, 1.25f);
        Add(EKeys::LeftCommand, TEXT("Win"), 1.25f, 5.5f, 1.25f);
        Add(EKeys::LeftAlt, TEXT("Alt"), 2.5f, 5.5f, 1.25f);
        Add(EKeys::SpaceBar, TEXT("Space"), 3.75f, 5.5f, 6.25f);
        Add(EKeys::RightAlt, TEXT("Alt"), 10.0f, 5.5f, 1.25f);
        Add(EKeys::RightCommand, TEXT("Win"), 11.25f, 5.5f, 1.25f);
        Add(EKeys::Invalid, TEXT("Menu"), 12.5f, 5.5f, 1.25f);
        Add(EKeys::RightControl, TEXT("Ctrl"), 13.75f, 5.5f, 1.25f);

        // Nav cluster. PrintScreen never reaches the engine's key space; the cap still exists.
        Add(EKeys::Invalid, TEXT("Prt"), 15.5f, 0.0f);
        Add(EKeys::ScrollLock, TEXT("Scr"), 16.5f, 0.0f);
        Add(EKeys::Pause, TEXT("Pse"), 17.5f, 0.0f);

        Add(EKeys::Insert, TEXT("Ins"), 15.5f, 1.5f);
        Add(EKeys::Home, TEXT("Home"), 16.5f, 1.5f);
        Add(EKeys::PageUp, TEXT("PgUp"), 17.5f, 1.5f);

        Add(EKeys::Delete, TEXT("Del"), 15.5f, 2.5f);
        Add(EKeys::End, TEXT("End"), 16.5f, 2.5f);
        Add(EKeys::PageDown, TEXT("PgDn"), 17.5f, 2.5f);

        Add(EKeys::Up, TEXT("^"), 16.5f, 4.5f);
        Add(EKeys::Left, TEXT("<"), 15.5f, 5.5f);
        Add(EKeys::Down, TEXT("v"), 16.5f, 5.5f);
        Add(EKeys::Right, TEXT(">"), 17.5f, 5.5f);

        // Numpad. Numpad Enter shares VK_RETURN with the main Enter on Windows, so both caps light together —
        // that is the platform's truth, not a layout shortcut.
        Add(EKeys::NumLock, TEXT("Num"), 19.0f, 1.5f);
        Add(EKeys::Divide, TEXT("/"), 20.0f, 1.5f);
        Add(EKeys::Multiply, TEXT("*"), 21.0f, 1.5f);
        Add(EKeys::Subtract, TEXT("-"), 22.0f, 1.5f);

        Add(EKeys::NumPadSeven, TEXT("7"), 19.0f, 2.5f);
        Add(EKeys::NumPadEight, TEXT("8"), 20.0f, 2.5f);
        Add(EKeys::NumPadNine, TEXT("9"), 21.0f, 2.5f);
        Add(EKeys::Add, TEXT("+"), 22.0f, 2.5f, 1.0f, 2.0f);

        Add(EKeys::NumPadFour, TEXT("4"), 19.0f, 3.5f);
        Add(EKeys::NumPadFive, TEXT("5"), 20.0f, 3.5f);
        Add(EKeys::NumPadSix, TEXT("6"), 21.0f, 3.5f);

        Add(EKeys::NumPadOne, TEXT("1"), 19.0f, 4.5f);
        Add(EKeys::NumPadTwo, TEXT("2"), 20.0f, 4.5f);
        Add(EKeys::NumPadThree, TEXT("3"), 21.0f, 4.5f);
        Add(EKeys::Enter, TEXT("Ent"), 22.0f, 4.5f, 1.0f, 2.0f);

        Add(EKeys::NumPadZero, TEXT("0"), 19.0f, 5.5f, 2.0f);
        Add(EKeys::Decimal, TEXT("."), 21.0f, 5.5f);

        return Defs;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::debug_devices
{
    auto
        Get_KeyboardLayout_AnsiFull()
        -> const TArray<FCkDebug_DeviceKeyDef>&
    {
        static const auto Layout = ck_debug_keyboard_layout::Build_AnsiFull();
        return Layout;
    }

    auto
        Get_KeyboardLayout_ExtentUnits()
        -> FVector2D
    {
        return FVector2D{23.0f, 6.5f};
    }
}

// --------------------------------------------------------------------------------------------------------------------
