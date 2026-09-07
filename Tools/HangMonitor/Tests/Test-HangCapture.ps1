# Requires a locally verified Sysinternals ProcDump binary. This test deliberately creates
# a real hung WinForms window and therefore never runs as part of normal Unreal automation.
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$ProcDumpPath,

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$OutputDirectory,

    [ValidateRange(15, 120)]
    [int]$TimeoutSeconds = 45
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-QuotedWindowsArgument
{
    param([Parameter(Mandatory)][string]$Value)
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Wait-ForFile
{
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][datetime]$Deadline)

    while ((Get-Date) -lt $Deadline)
    {
        if (Test-Path -LiteralPath $Path)
        { return $true }

        Start-Sleep -Milliseconds 100
    }

    return $false
}

function Wait-ForProcessExit
{
    param([Parameter(Mandatory)][System.Diagnostics.Process]$Process, [Parameter(Mandatory)][datetime]$Deadline)

    while ((Get-Date) -lt $Deadline)
    {
        if ($Process.HasExited)
        { return $true }

        Start-Sleep -Milliseconds 100
        $Process.Refresh()
    }

    return $Process.HasExited
}

function Assert-True
{
    param([Parameter(Mandatory)][bool]$Condition, [Parameter(Mandatory)][string]$Message)

    if (-not $Condition)
    { throw $Message }
}

$resolvedProcDumpPath = (Resolve-Path -LiteralPath $ProcDumpPath).Path
Assert-True (Test-Path -LiteralPath $resolvedProcDumpPath -PathType Leaf) "ProcDump path is not a file: $ProcDumpPath"
Assert-True ([IO.Path]::GetFileName($resolvedProcDumpPath).Equals('procdump64.exe', [StringComparison]::OrdinalIgnoreCase)) "The harness only accepts procdump64.exe: $resolvedProcDumpPath"

$signature = Get-AuthenticodeSignature -LiteralPath $resolvedProcDumpPath
Assert-True ($signature.Status -eq [System.Management.Automation.SignatureStatus]::Valid) "ProcDump signature is not valid: $($signature.Status)"
Assert-True ($signature.SignerCertificate.Subject -match 'Microsoft') "ProcDump signer is not Microsoft: $($signature.SignerCertificate.Subject)"

$cscPath = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
Assert-True (Test-Path -LiteralPath $cscPath -PathType Leaf) "Framework C# compiler is unavailable: $cscPath"

$fixtureSource = Join-Path $PSScriptRoot 'fixture.cs'
Assert-True (Test-Path -LiteralPath $fixtureSource -PathType Leaf) "Fixture source is unavailable: $fixtureSource"

$resolvedOutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$runDirectory = Join-Path $resolvedOutputDirectory ('HangCapture-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($runDirectory) | Out-Null

$fixturePath = Join-Path $runDirectory 'HangCaptureFixture.exe'
$compileOutput = Join-Path $runDirectory 'fixture-compile.stdout.log'
$compileError = Join-Path $runDirectory 'fixture-compile.stderr.log'
$compileArguments = '/nologo /target:winexe /platform:x64 /r:System.Windows.Forms.dll /r:System.Drawing.dll /out:' + (Get-QuotedWindowsArgument $fixturePath) + ' ' + (Get-QuotedWindowsArgument $fixtureSource)
$compiler = Start-Process -FilePath $cscPath -ArgumentList $compileArguments -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput $compileOutput -RedirectStandardError $compileError
Assert-True ($compiler.ExitCode -eq 0 -and (Test-Path -LiteralPath $fixturePath -PathType Leaf)) "Fixture compilation failed; inspect $compileOutput and $compileError"

function Start-Fixture
{
    param([Parameter(Mandatory)][ValidateSet('hang', 'exit')][string]$Mode, [Parameter(Mandatory)][string]$CaseDirectory)

    $readyPath = Join-Path $CaseDirectory 'fixture.ready'
    $arguments = '--mode ' + $Mode + ' --ready ' + (Get-QuotedWindowsArgument $readyPath) + ' --hang-delay-ms 2000 --recovery-ms 20000 --normal-exit-ms 1500'
    $fixture = Start-Process -FilePath $fixturePath -ArgumentList $arguments -PassThru -WindowStyle Hidden
    Assert-True (Wait-ForFile $readyPath ((Get-Date).AddSeconds(10))) "Fixture did not create ready marker: $readyPath"
    return $fixture
}

$results = [ordered]@{
    schemaVersion = 1
    procDumpPath = $resolvedProcDumpPath
    procDumpSigner = $signature.SignerCertificate.Subject
    fixturePath = $fixturePath
    cases = @()
}

try
{
    $hangDirectory = Join-Path $runDirectory 'hung-window'
    [IO.Directory]::CreateDirectory($hangDirectory) | Out-Null
    $hungFixture = Start-Fixture -Mode hang -CaseDirectory $hangDirectory
    $hungOutput = Join-Path $hangDirectory 'procdump.stdout.log'
    $hungError = Join-Path $hangDirectory 'procdump.stderr.log'
    $hungArguments = '-accepteula -h -n 1 -mm ' + $hungFixture.Id + ' ' + (Get-QuotedWindowsArgument $hangDirectory)
    $hungProcDump = Start-Process -FilePath $resolvedProcDumpPath -ArgumentList $hungArguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $hungOutput -RedirectStandardError $hungError

    Assert-True (Wait-ForProcessExit $hungProcDump ((Get-Date).AddSeconds($TimeoutSeconds))) "ProcDump did not finish after the hung-window timeout; inspect $hungOutput and $hungError"
    $hungDumps = @(Get-ChildItem -LiteralPath $hangDirectory -Filter '*.dmp' -File)
    Assert-True ($hungDumps.Count -eq 1) "Expected exactly one bounded hang dump; found $($hungDumps.Count)"
    $dumpHeader = [IO.File]::ReadAllBytes($hungDumps[0].FullName)
    Assert-True ($dumpHeader.Length -ge 4 -and $dumpHeader[0] -eq [byte][char]'M' -and $dumpHeader[1] -eq [byte][char]'D' -and $dumpHeader[2] -eq [byte][char]'M' -and $dumpHeader[3] -eq [byte][char]'P') "Hang dump is not a nonempty Windows minidump."
    Assert-True (-not $hungFixture.HasExited) 'Fixture exited before the harness could prove ProcDump did not kill it.'

    Assert-True (Wait-ForProcessExit $hungFixture ((Get-Date).AddSeconds(25))) 'Hung fixture did not self-recover and exit.'
    Assert-True ($hungFixture.ExitCode -eq 0) "Hung fixture recovery failed with exit code $($hungFixture.ExitCode)"
    $results.cases += [ordered]@{
        name = 'hung-window'
        procDumpExitCode = $hungProcDump.ExitCode
        dumpCount = $hungDumps.Count
        dumpPaths = @($hungDumps.FullName)
        fixtureExitCode = $hungFixture.ExitCode
    }

    $normalDirectory = Join-Path $runDirectory 'normal-exit'
    [IO.Directory]::CreateDirectory($normalDirectory) | Out-Null
    $normalFixture = Start-Fixture -Mode exit -CaseDirectory $normalDirectory
    $normalOutput = Join-Path $normalDirectory 'procdump.stdout.log'
    $normalError = Join-Path $normalDirectory 'procdump.stderr.log'
    $normalArguments = '-accepteula -h -n 1 -mm ' + $normalFixture.Id + ' ' + (Get-QuotedWindowsArgument $normalDirectory)
    $normalProcDump = Start-Process -FilePath $resolvedProcDumpPath -ArgumentList $normalArguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $normalOutput -RedirectStandardError $normalError

    Assert-True (Wait-ForProcessExit $normalFixture ((Get-Date).AddSeconds(10))) 'Normal fixture did not exit.'
    Assert-True (Wait-ForProcessExit $normalProcDump ((Get-Date).AddSeconds(15))) "ProcDump did not exit after a normal target exit; inspect $normalOutput and $normalError"
    $normalDumps = @(Get-ChildItem -LiteralPath $normalDirectory -Filter '*.dmp' -File)
    Assert-True ($normalDumps.Count -eq 0) "Normal target exit unexpectedly produced $($normalDumps.Count) dump(s)."
    $results.cases += [ordered]@{
        name = 'normal-exit'
        procDumpExitCode = $normalProcDump.ExitCode
        dumpCount = $normalDumps.Count
        fixtureExitCode = $normalFixture.ExitCode
    }

    $resultsPath = Join-Path $runDirectory 'result.json'
    $results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $resultsPath -Encoding utf8
    Write-Output "PASS HangCapture fixture verification: $resultsPath"
}
catch
{
    $_ | Out-String | Set-Content -LiteralPath (Join-Path $runDirectory 'failure.txt') -Encoding utf8
    throw
}
