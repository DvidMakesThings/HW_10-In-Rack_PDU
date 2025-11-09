# openSerial.ps1
#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-Config {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { throw "Config file not found: $Path" }
    Get-Content -LiteralPath $Path | ForEach-Object {
        $line = $_.Trim()
        if (-not $line -or $line.StartsWith('#') -or $line.StartsWith(';')) { return }
        if ($line -match '^\s*([^=\s#;]+)\s*=\s*(.+?)\s*$') {
            $key = $matches[1].Trim()
            $val = $matches[2].Trim().Trim('"')
            Set-Variable -Name $key -Value $val -Scope Script
        }
    }
}

function Resolve-PathRelative {
    param([string]$PathLike)
    if ([string]::IsNullOrWhiteSpace($PathLike)) { return $PathLike }
    if ([System.IO.Path]::IsPathRooted($PathLike)) {
        $rp = Resolve-Path -LiteralPath $PathLike -ErrorAction SilentlyContinue
        if ($rp) { return $rp.Path } else { return $PathLike }
    } else {
        $full = Join-Path $PSScriptRoot $PathLike
        $rp = Resolve-Path -LiteralPath $full -ErrorAction SilentlyContinue
        if ($rp) { return $rp.Path } else { return $full }
    }
}

function Test-PortPresent {
    param([string]$Port)
    return ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Port)
}

try {
    $confPath = Join-Path $PSScriptRoot 'serial_port.win.conf'
    Get-Config -Path $confPath

    if (-not $PORT) { throw "Missing 'PORT' in config." }
    if (-not $BAUD) { throw "Missing 'BAUD' in config." }
    if (-not $PUTTY) { throw "Missing 'PUTTY' in config." }
    if (-not $LOG_DIR) { throw "Missing 'LOG_DIR' in config." }

    $PUTTY_ABS   = Resolve-PathRelative $PUTTY
    $LOG_DIR_ABS = Resolve-PathRelative $LOG_DIR

    if (-not (Test-Path -LiteralPath $PUTTY_ABS)) { throw "PuTTY not found: $PUTTY_ABS" }

    New-Item -ItemType Directory -Force -Path $LOG_DIR_ABS | Out-Null
    $log = Join-Path $LOG_DIR_ABS ("energis_{0}.log" -f (Get-Date -Format 'yyyyMMdd_HHmmss'))

    Write-Host "=== Auto-reopen Serial (Windows) ===  Ctrl+C to quit"
    while ($true) {
        while (-not (Test-PortPresent -Port $PORT)) {
            Start-Sleep -Milliseconds 500
        }

        $proc = Start-Process -FilePath "$PUTTY_ABS" -ArgumentList @('-serial', "$PORT", '-sercfg', "$BAUD,8,n,1,N", '-sessionlog', "$log") -PassThru

        while (-not $proc.HasExited) {
            if (-not (Test-PortPresent -Port $PORT)) {
                $null = $proc.CloseMainWindow()
                Start-Sleep -Seconds 1
                if (-not $proc.HasExited) { $proc.Kill() }
            } else {
                Start-Sleep -Milliseconds 500
            }
        }

        while (-not (Test-PortPresent -Port $PORT)) {
            Start-Sleep -Milliseconds 500
        }
        Write-Host "Reconnecting..."
    }
}
catch {
    Write-Error "openSerial.ps1 failed: $($_.Exception.Message)"
    exit 1
}
