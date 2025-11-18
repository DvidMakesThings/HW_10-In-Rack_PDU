# flash_rp2040_and_serial.ps1
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

function Set-ConfigValue {
    param([string]$Path, [string]$Key, [string]$Value)
    $lines = Get-Content -LiteralPath $Path -ErrorAction Stop
    $pattern = "^\s*${Key}\s*="
    $idx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $pattern) { $idx = $i; break } }
    if ($idx -ge 0) { $lines[$idx] = "$Key=$Value" } else { $lines += "$Key=$Value" }
    Set-Content -LiteralPath $Path -Value $lines -Encoding UTF8
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

function Get-SerialPortList {
    Get-CimInstance Win32_PnPEntity -Filter "Name LIKE '%(COM%'" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '\((COM\d+)\)' } |
    ForEach-Object { [PSCustomObject]@{ Port = $matches[1]; Name = $_.Name } } |
    Sort-Object { [int]($_.Port -replace '\D') }
}

function Select-SerialPort {
    param([string]$CurrentPort, [string]$ConfigPath)
    $ports = @(Get-SerialPortList)
    if ($ports.Port -contains $CurrentPort) { return $CurrentPort }
    Write-Host "Configured PORT '$CurrentPort' not found."
    if ($ports.Count -eq 0) { throw "No serial ports detected." }
    Write-Host "Available ports:"
    for ($i = 0; $i -lt $ports.Count; $i++) { Write-Host ("  [{0}] {1}" -f $i, $ports[$i].Name) }
    do {
        $sel = Read-Host "Select port index"
        $ok = [int]::TryParse($sel, [ref]$null) -and [int]$sel -ge 0 -and [int]$sel -lt $ports.Count
    } until ($ok)
    $chosen = $ports[[int]$sel].Port
    Set-ConfigValue -Path $ConfigPath -Key 'PORT' -Value $chosen
    Write-Host "Using PORT '$chosen' and updated config."
    return $chosen
}

function Test-PortPresent {
    param([string]$Port)
    return ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Port)
}

try {
    # Change to project directory
    Set-Location "G:\_GitHub\HW_10-In-Rack_PDU\src\Software\ENERGIS_RTOS"
    
    # Remove existing build directory if it exists and create new one
    if (Test-Path build) { Remove-Item build -Recurse -Force }
    New-Item -ItemType Directory -Path build | Out-Null
    Set-Location build
    
    # Run CMake and Ninja build
    cmake -G Ninja -DPICO_SDK_PATH=C:/Users/sdvid/.pico-sdk/sdk/2.2.0 ..
    ninja
    
    # Continue with flashing and serial monitoring
    Set-Location $PSScriptRoot
    $confPath = Join-Path $PSScriptRoot 'serial_port.win.conf'
    Get-Config -Path $confPath

    if (-not $PORT) { throw "Missing 'PORT' in config." }
    if (-not $BAUD) { throw "Missing 'BAUD' in config." }
    if (-not $FIRMWARE) { throw "Missing 'FIRMWARE' in config." }
    if (-not $PY_UPLOADER) { throw "Missing 'PY_UPLOADER' in config." }
    if (-not $PUTTY) { throw "Missing 'PUTTY' in config." }
    if (-not $LOG_DIR) { throw "Missing 'LOG_DIR' in config." }

    $PORT = Select-SerialPort -CurrentPort $PORT -ConfigPath $confPath
    $FIRMWARE_ABS = Resolve-PathRelative $FIRMWARE
    $PY_ABS = Resolve-PathRelative $PY_UPLOADER
    $PUTTY_ABS = Resolve-PathRelative $PUTTY
    $LOG_DIR_ABS = Resolve-PathRelative $LOG_DIR

    if (-not (Test-Path -LiteralPath $PY_ABS)) { throw "Python uploader not found: $PY_ABS" }
    if (-not (Test-Path -LiteralPath $FIRMWARE_ABS)) { throw "Firmware file not found: $FIRMWARE_ABS" }
    if (-not (Test-Path -LiteralPath $PUTTY_ABS)) { throw "PuTTY not found: $PUTTY_ABS" }

    New-Item -ItemType Directory -Force -Path $LOG_DIR_ABS | Out-Null
    $log = Join-Path $LOG_DIR_ABS ("energis_{0}.log" -f (Get-Date -Format 'yyyyMMdd_HHmmss'))

    & python "$PY_ABS" --p "$PORT" --b $BAUD --f "$FIRMWARE_ABS"

    Write-Host ""
    Write-Host "[*] Opening serial on $PORT @ $BAUD"
    Write-Host "    Log: $log"
    Write-Host ""

    while (-not (Test-PortPresent -Port $PORT)) {
        Write-Host "Waiting for $PORT..."
        Start-Sleep -Milliseconds 500
    }

    $proc = Start-Process -FilePath "$PUTTY_ABS" -ArgumentList @('-serial', "$PORT", '-sercfg', "$BAUD,8,n,1,N", '-sessionlog', "$log") -PassThru

    while (-not $proc.HasExited) {
        if (-not (Test-PortPresent -Port $PORT)) {
            Write-Host "$PORT disconnected, closing PuTTY..."
            $null = $proc.CloseMainWindow()
            Start-Sleep -Seconds 1
            if (-not $proc.HasExited) { $proc.Kill() }
        }
        else {
            Start-Sleep -Milliseconds 500
        }
    }

    Write-Host "Serial session ended."
}
catch {
    Write-Error "flash_rp2040_and_serial.ps1 failed: $($_.Exception.Message)"
    exit 1
}
