$portName = "COM6"
$uf2Path = "G:\_GitHub\HW_10-In-Rack_PDU\src\Software\ENERGIS\build\ENERGIS.uf2"
$plinkPath = "C:\Program Files\PuTTY\plink.exe"

# Send BOOTSEL using Plink to wake USB CDC and trigger bootloader
try {
    Write-Host "Sending BOOTSEL command to ${portName} using Plink..."
    echo "BOOTSEL" | & "$plinkPath" -serial "$portName" -sercfg 115200,8,n,1,N -batch
    Write-Host "Sent 'BOOTSEL' to ${portName} (port will likely disconnect)"
} catch {
    Write-Warning "Plink error likely due to USB disconnect (safe to ignore): $($_.Exception.Message)"
}


# Wait for USB to reset and RPI-RP2 drive to appear
Start-Sleep -Seconds 1
Write-Host "Waiting for RPI-RP2 mass storage to appear..."
$drive = $null
$maxAttempts = 10
for ($i = 0; $i -lt $maxAttempts; $i++) {
    $drive = Get-Volume | Where-Object { $_.FileSystemLabel -eq "RPI-RP2" } | Select-Object -First 1 -ExpandProperty DriveLetter
    if ($drive) {
        Write-Host "Found RPI-RP2 on drive ${drive}:"
        break
    }
    Start-Sleep -Seconds 1
}

# Flash UF2 file
if ($drive) {
    try {
        Copy-Item -Path $uf2Path -Destination "${drive}:\\" -Force
        Write-Host "UF2 flashed successfully to ${drive}:"
    } catch {
        Write-Error "Failed to copy UF2 to ${drive}: $($_.Exception.Message)"
        exit 3
    }
} else {
    Write-Error "RPI-RP2 drive not found after $maxAttempts seconds"
    exit 2
}
