# openSerial_direct.ps1
#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-PortPresent {
    param([string]$Port)
    return ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Port)
}

# --- MAIN ---
try {
    # Parse command line arguments
    $portArg = $args | Where-Object { $_ -match '^--\d+$' } | Select-Object -First 1
    $reopen = $args -contains '--reopen'

    if (-not $portArg -or -not ($portArg -match '^--(\d+)$')) {
        throw "Usage: openSerial_direct.ps1 --N [--reopen] (where N is the COM port number, e.g. --3 for COM3)"
    }
    
    $portNum = $matches[1]
    $PORT = "COM$portNum"
    $BAUD = 115200  # Default baud rate
    $PUTTY = "C:\Program Files\PuTTY\putty.exe"  # Default PuTTY path
    $LOG_DIR = Join-Path $PSScriptRoot "logs"

    if (-not (Test-Path -LiteralPath $PUTTY)) { 
        # Try alternative common location
        $PUTTY = "C:\Program Files (x86)\PuTTY\putty.exe"
        if (-not (Test-Path -LiteralPath $PUTTY)) {
            throw "PuTTY not found at default locations. Please install PuTTY or update script with correct path."
        }
    }

    New-Item -ItemType Directory -Force -Path $LOG_DIR | Out-Null
    $log = Join-Path $LOG_DIR ("COM{0}_{1}.log" -f $portNum, (Get-Date -Format 'yyyyMMdd_HHmmss'))

    Write-Host "=== Serial Terminal (Windows) ==="
    Write-Host "Port: $PORT | Baud: $BAUD"
    Write-Host "Log:  $log"
    Write-Host ""

    # Wait for the port to appear
    while (-not (Test-PortPresent -Port $PORT)) {
        Write-Host ("[{0}] Waiting for {1}..." -f (Get-Date -Format HH:mm:ss), $PORT)
        Start-Sleep -Milliseconds 500
    }

    # Start PuTTY and keep a handle so we can close it on disconnect
    $uartprop = @('-serial', "$PORT", '-sercfg', "$BAUD,8,n,1,N", '-sessionlog', "$log")
    $proc = Start-Process -FilePath "$PUTTY" -ArgumentList $uartprop -PassThru
    Write-Host ("[{0}] Opened PuTTY (PID {1}) on {2} @{3}" -f (Get-Date -Format HH:mm:ss), $proc.Id, $PORT, $BAUD)

    # Monitor port availability and handle reconnection if --reopen is specified
    while ($true) {
        if (-not $proc.HasExited -and -not (Test-PortPresent -Port $PORT)) {
            Write-Host ("[{0}] {1} disappeared → closing PuTTY..." -f (Get-Date -Format HH:mm:ss), $PORT)
            $null = $proc.CloseMainWindow()
            Start-Sleep -Milliseconds 800
            if (-not $proc.HasExited) { $proc.Kill() }
            
            if (-not $reopen) { break }
            
            # Wait for port to reappear if --reopen was specified
            Write-Host ("[{0}] Waiting for {1} to reappear..." -f (Get-Date -Format HH:mm:ss), $PORT)
            while (-not (Test-PortPresent -Port $PORT)) {
                Start-Sleep -Milliseconds 500
            }
            
            # Create new log file and restart PuTTY
            $log = Join-Path $LOG_DIR ("COM{0}_{1}.log" -f $portNum, (Get-Date -Format 'yyyyMMdd_HHmmss'))
            Write-Host ("[{0}] Reopening {1} | Log: {2}" -f (Get-Date -Format HH:mm:ss), $PORT, $log)
            $proc = Start-Process -FilePath "$PUTTY" -ArgumentList $uartprop -PassThru
        }
        elseif ($proc.HasExited -and -not $reopen) {
            break
        }
        Start-Sleep -Milliseconds 300
    }

    Write-Host ("[{0}] Session ended." -f (Get-Date -Format HH:mm:ss))
}
catch {
    Write-Error "openSerial_direct.ps1 failed: $($_.Exception.Message)"
    exit 1
}