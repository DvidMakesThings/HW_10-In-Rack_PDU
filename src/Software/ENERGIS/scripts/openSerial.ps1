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
    }
    else {
        $full = Join-Path $PSScriptRoot $PathLike
        $rp = Resolve-Path -LiteralPath $full -ErrorAction SilentlyContinue
        if ($rp) { return $rp.Path } else { return $full }
    }
}

function Test-PortPresent {
    param([string]$Port)
    return ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Port)
}

# --- MAIN ---
try {
    $confPath = Join-Path $PSScriptRoot 'serial_port.win.conf'
    Get-Config -Path $confPath

    if (-not $PORT) { throw "Missing 'PORT' in config ($confPath)." }
    if (-not $BAUD) { throw "Missing 'BAUD' in config ($confPath)." }
    if (-not $PUTTY) { throw "Missing 'PUTTY' in config ($confPath)." }
    if (-not $LOG_DIR) { throw "Missing 'LOG_DIR' in config ($confPath)." }

    $PUTTY_ABS = Resolve-PathRelative $PUTTY
    $LOG_DIR_ABS = Resolve-PathRelative $LOG_DIR

    if (-not (Test-Path -LiteralPath $PUTTY_ABS)) { throw "PuTTY not found: $PUTTY_ABS" }

    New-Item -ItemType Directory -Force -Path $LOG_DIR_ABS | Out-Null
    $log = Join-Path $LOG_DIR_ABS ("energis_{0}.log" -f (Get-Date -Format 'yyyyMMdd_HHmmss'))

    Write-Host "=== Auto-reopen Serial (Windows) ===  Ctrl+C to quit"
    Write-Host "Port: $PORT | Baud: $BAUD"
    Write-Host "Log:  $log"
    Write-Host ""

    while ($true) {
        # Wait for the port to appear
        while (-not (Test-PortPresent -Port $PORT)) {
            Write-Host ("[{0}] Waiting for {1}..." -f (Get-Date -Format HH:mm:ss), $PORT)
            Start-Sleep -Milliseconds 500
        }

        # Start PuTTY and keep a handle so we can close it on disconnect
        $args = @('-serial', "$PORT", '-sercfg', "$BAUD,8,n,1,N", '-sessionlog', "$log")
        $proc = Start-Process -FilePath "$PUTTY_ABS" -ArgumentList $args -PassThru
        Write-Host ("[{0}] Opened PuTTY (PID {1}) on {2} @{3}" -f (Get-Date -Format HH:mm:ss), $proc.Id, $PORT, $BAUD)

        # Monitor port availability; if the device reboots (port disappears), close PuTTY immediately
        while (-not $proc.HasExited) {
            if (-not (Test-PortPresent -Port $PORT)) {
                Write-Host ("[{0}] {1} disappeared → closing PuTTY..." -f (Get-Date -Format HH:mm:ss), $PORT)
                $null = $proc.CloseMainWindow()
                Start-Sleep -Milliseconds 800
                if (-not $proc.HasExited) { $proc.Kill() }
                break
            }
            Start-Sleep -Milliseconds 300
        }

        # If user manually closed PuTTY, we also loop back and reopen when the port is present
        Write-Host ("[{0}] Session ended. Reconnecting when {1} is available..." -f (Get-Date -Format HH:mm:ss), $PORT)
        # brief backoff to avoid thrashing
        Start-Sleep -Milliseconds 500
    }
}
catch {
    Write-Error "openSerial.ps1 failed: $($_.Exception.Message)"
    exit 1
}
