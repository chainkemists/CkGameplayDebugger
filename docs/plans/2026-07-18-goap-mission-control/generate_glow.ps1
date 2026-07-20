# Generates the monochrome soft-glow 9-slice PNGs used by FCkDebuggerCommonStyle.
# Output: Source/CkDebuggerCommon/Resources/Common/Glow_Soft.png / Glow_Tight.png
#
# Each PNG is a white rounded-rect whose alpha falls off smoothly OUTWARD from
# the core rect. Slate draws it as a Box (9-slice) brush tinted at runtime
# (accent cyan for active-chain glow, warn amber for fallback glow). White RGB +
# alpha falloff keeps it fully tintable.
#
# Re-run only if the glow look needs tuning; commit the PNGs.

param(
    [string]$OutDir = (Join-Path $PSScriptRoot "..\..\..\Source\CkDebuggerCommon\Resources\Common")
)

Add-Type -AssemblyName System.Drawing

function New-GlowPng {
    param(
        [string]$Path,
        [int]$Size,         # canvas (square)
        [int]$CoreInset,    # inset of the solid core rect from each edge
        [float]$CornerR,    # corner radius of the core rect
        [float]$Falloff,    # distance over which alpha fades to 0 outside the core
        [float]$CoreAlpha,  # alpha inside the core (glow center strength)
        [float]$Gamma       # falloff curve shape (>1 = tighter center)
    )

    $bmp = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

    $x0 = $CoreInset + $CornerR; $x1 = $Size - 1 - $CoreInset - $CornerR
    $y0 = $CoreInset + $CornerR; $y1 = $Size - 1 - $CoreInset - $CornerR

    for ($y = 0; $y -lt $Size; $y++) {
        for ($x = 0; $x -lt $Size; $x++) {
            # Signed distance to the rounded core rect: clamp to the inner
            # (radius-shrunk) rect, measure, subtract radius.
            $cx = [Math]::Min([Math]::Max($x, $x0), $x1)
            $cy = [Math]::Min([Math]::Max($y, $y0), $y1)
            $dx = $x - $cx; $dy = $y - $cy
            $d  = [Math]::Sqrt($dx*$dx + $dy*$dy) - $CornerR

            if ($d -le 0) { $a = $CoreAlpha }
            elseif ($d -ge $Falloff) { $a = 0.0 }
            else {
                $t = 1.0 - ($d / $Falloff)
                $a = $CoreAlpha * [Math]::Pow($t, $Gamma)
            }

            $alpha = [int][Math]::Round(255.0 * $a)
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, 255, 255, 255))
        }
    }

    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "wrote $Path"
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

# Soft: wide halo for active-chain leaf / card emphasis (mockup: 12px blur).
New-GlowPng -Path (Join-Path $OutDir "Glow_Soft.png")  -Size 64 -CoreInset 22 -CornerR 8 -Falloff 20 -CoreAlpha 0.85 -Gamma 1.8

# Tight: crisp 4-6px halo for chips / trace ring (mockup: 0 0 0 2px ring + slight bloom).
New-GlowPng -Path (Join-Path $OutDir "Glow_Tight.png") -Size 48 -CoreInset 16 -CornerR 6 -Falloff 10 -CoreAlpha 0.95 -Gamma 1.4
