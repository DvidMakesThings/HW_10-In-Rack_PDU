$portName = "COM3"
$plinkPath = "C:\Program Files\PuTTY\plink.exe"

# Send BOOTSEL using Plink to wake USB CDC and trigger bootloader
try {
    Write-Host "Sending BOOTSEL command to ${portName} using Plink..."
    Write-Output "BOOTSEL" | & "$plinkPath" -serial "$portName" -sercfg 115200,8,n,1,N -batch
    Write-Host "Sent 'BOOTSEL' to ${portName} (port will likely disconnect)"
} catch {
    Write-Warning "Plink error likely due to USB disconnect (safe to ignore): $($_.Exception.Message)"
}

