# Installs Microsoft's external hang-capture utility for the current user.
# This script and its downloads are not runtime dependencies or packaged game content.
[CmdletBinding()]
param(
    [string] $InstallDirectory = (Join-Path $env:LOCALAPPDATA 'CkDiagnostics\ProcDump'),
    [switch] $AcceptEula
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-MicrosoftExecutable([string] $Path)
{
    $Signature = Get-AuthenticodeSignature -LiteralPath $Path
    if ($Signature.Status -ne 'Valid' -or $null -eq $Signature.SignerCertificate -or
        $Signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation(?:,|$)')
    {
        throw "ProcDump signature verification failed for [$Path]. No executable was launched."
    }

    $Version = [Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    if ($Version.OriginalFilename -ine 'procdump' -or $Version.ProductName -ne 'ProcDump' -or
        [IO.Path]::GetFileName($Path) -ine 'procdump64.exe')
    {
        throw "[$Path] is not the expected 64-bit ProcDump executable."
    }
}

$InstallDirectory = [IO.Path]::GetFullPath($InstallDirectory)
$Executable = Join-Path $InstallDirectory 'procdump64.exe'
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf))
{
    $DownloadDirectory = Join-Path $env:LOCALAPPDATA ('CkDiagnostics\Downloads\' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $DownloadDirectory -Force | Out-Null
    $Archive = Join-Path $DownloadDirectory 'Procdump.zip'
    Invoke-WebRequest -Uri 'https://download.sysinternals.com/files/Procdump.zip' -OutFile $Archive -UseBasicParsing
    $Extracted = Join-Path $DownloadDirectory 'extracted'
    Expand-Archive -LiteralPath $Archive -DestinationPath $Extracted
    $Candidate = Join-Path $Extracted 'procdump64.exe'
    Assert-MicrosoftExecutable $Candidate

    New-Item -ItemType Directory -Path $InstallDirectory -Force | Out-Null
    Copy-Item -LiteralPath $Candidate -Destination $Executable
    Copy-Item -LiteralPath (Join-Path $Extracted 'Eula.txt') -Destination (Join-Path $InstallDirectory 'Eula.txt')
}

Assert-MicrosoftExecutable $Executable
$Identity = [ordered]@{
    source = 'https://download.sysinternals.com/files/Procdump.zip'
    executable = $Executable
    sha256 = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash
    fileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($Executable).FileVersion
    verifiedUtc = [DateTime]::UtcNow.ToString('o')
}
$Identity | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $InstallDirectory 'installation.json') -Encoding UTF8

if ($AcceptEula)
{
    $SetupLog = Join-Path $InstallDirectory 'setup.log'
    $SetupErrorLog = Join-Path $InstallDirectory 'setup-error.log'
    $Process = Start-Process -FilePath $Executable -ArgumentList @('-accepteula', '-?') -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput $SetupLog -RedirectStandardError $SetupErrorLog
    if (-not $Process.WaitForExit(15000))
    {
        throw "ProcDump setup did not finish within 15 seconds (PID $($Process.Id)); inspect [$SetupLog]."
    }
    # ProcDump 12 returns -1 for its help command even after accepting the EULA.
    $EulaState = Get-ItemProperty -LiteralPath 'HKCU:\Software\Sysinternals\ProcDump' -ErrorAction SilentlyContinue
    if ($Process.ExitCode -notin @(0, -1) -or $null -eq $EulaState -or $EulaState.EulaAccepted -ne 1)
    {
        throw "ProcDump setup failed with exit code $($Process.ExitCode); inspect [$SetupLog] and [$SetupErrorLog]."
    }
}

Write-Output "Verified ProcDump: $Executable"
Write-Output "SHA256: $($Identity.sha256)"
if (-not $AcceptEula)
{
    Write-Output 'Before arming Hang Monitor, review Eula.txt and rerun this script with -AcceptEula.'
}
