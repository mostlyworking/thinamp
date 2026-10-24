# Installs the Thinamp VST3 into the system VST3 folder (asks for admin once).
#   Online:  irm https://raw.githubusercontent.com/mostlyworking/thinamp/main/install.ps1 | iex
#   Offline: double-click "Install Thinamp.cmd" inside the release zip.
# Written for Windows PowerShell 5.1, which every Windows 10/11 machine has.
# Wrapped in a script block so nothing leaks into the caller's session when piped into iex.
& {
    $ErrorActionPreference = 'Stop'
    $ProgressPreference = 'SilentlyContinue'

    $url = 'https://github.com/mostlyworking/thinamp/releases/latest/download/Thinamp-windows.zip'
    $vst3Dir = Join-Path $env:CommonProgramFiles 'VST3'

    # Use the plugin next to this script when run from the unzipped release; otherwise download it.
    $src = $null
    $tmp = $null
    if ($PSScriptRoot -and (Test-Path (Join-Path $PSScriptRoot 'Thinamp.vst3'))) {
        $src = $PSScriptRoot
    } else {
        $tmp = Join-Path ([IO.Path]::GetTempPath()) ('thinamp-' + [guid]::NewGuid())
        New-Item -ItemType Directory $tmp | Out-Null
        Write-Host 'Downloading Thinamp...'
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest $url -OutFile (Join-Path $tmp 'thinamp.zip') -UseBasicParsing
        Expand-Archive (Join-Path $tmp 'thinamp.zip') (Join-Path $tmp 'unzipped')
        $src = Join-Path $tmp 'unzipped'
    }

    # Remove copies installed under the plugin's old name (same plugin ID, would show up twice), then copy.
    $q = { param($s) "'" + ($s -replace "'", "''") + "'" }
    $copy = @"
    `$ErrorActionPreference = 'Stop'
    New-Item -ItemType Directory -Force $(& $q $vst3Dir) | Out-Null
    foreach (`$old in 'Fourier Truncator.vst3', 'Thinamp.vst3') {
        `$p = Join-Path $(& $q $vst3Dir) `$old
        if (Test-Path `$p) { Remove-Item -Recurse -Force `$p }
    }
    Copy-Item -Recurse $(& $q (Join-Path $src 'Thinamp.vst3')) $(& $q $vst3Dir)
    # Clear the downloaded-from-internet flag so nothing prompts when the DAW loads it.
    Get-ChildItem -Recurse $(& $q (Join-Path $vst3Dir 'Thinamp.vst3')) | Unblock-File
"@

    Write-Host 'Installing...'
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if ($isAdmin) {
        & ([scriptblock]::Create($copy))
    } else {
        $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($copy))
        try {
            $proc = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -WindowStyle Hidden `
                -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encoded
        } catch {
            throw 'Installing needs administrator permission (the VST3 folder is under Program Files).'
        }
        if ($proc.ExitCode -ne 0) {
            throw "Copying into $vst3Dir failed. Close your DAW and try again."
        }
    }

    if ($tmp) { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }

    Write-Host @"

Thinamp is installed in $vst3Dir

Restart your DAW (or rescan plugins) and look for "Thinamp".

Ableton Live: Settings > Plug-Ins, turn on "Use VST3 Plug-In System Folders", click Rescan.
Other DAWs: https://github.com/mostlyworking/thinamp#finding-it-in-your-daw

"@
}
