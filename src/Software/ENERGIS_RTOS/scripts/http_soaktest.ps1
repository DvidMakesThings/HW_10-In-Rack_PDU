# http_soaktest.ps1 — 1h HTTP soak using a single HttpClient (keep-alive)

$uri        = 'http://192.168.0.12/api/status'
$dur        = [TimeSpan]::FromHours(1)
$intervalMs = 50
$timeoutSec = 2
$rollupSec  = 60
$outCsv     = ".\soak_status_$((Get-Date).ToString('yyyyMMdd_HHmmss')).csv"

Add-Type -AssemblyName System.Net.Http

$handler = [System.Net.Http.HttpClientHandler]::new()
$handler.AllowAutoRedirect = $false
$handler.UseCookies        = $false
$client  = [System.Net.Http.HttpClient]::new($handler)
$client.Timeout = [TimeSpan]::FromSeconds($timeoutSec)
if (-not $client.DefaultRequestHeaders.Contains('Connection')) { [void]$client.DefaultRequestHeaders.Add('Connection','keep-alive') }
if (-not $client.DefaultRequestHeaders.Contains('Accept'))     { [void]$client.DefaultRequestHeaders.Add('Accept','application/json') }

$swAll  = [Diagnostics.Stopwatch]::StartNew()
$swRoll = [Diagnostics.Stopwatch]::StartNew()

$latRoll = New-Object System.Collections.Generic.List[double]
$latAll  = New-Object System.Collections.Generic.List[double]
$codes   = @{}
$ok = 0; $err = 0

"timestamp,ok,err,http_code,latency_ms" | Out-File -Encoding ascii $outCsv

function Percentile([double[]]$arr, [double]$p) {
    if ($arr.Count -eq 0) { return [double]::NaN }
    $sorted = [double[]]($arr | Sort-Object)
    $i = [Math]::Min([Math]::Max([int][Math]::Ceiling(($p/100.0) * $sorted.Count) - 1, 0), $sorted.Count - 1)
    return $sorted[$i]
}

while ($swAll.Elapsed -lt $dur) {
    $t0 = [Diagnostics.Stopwatch]::GetTimestamp()
    try {
        $resp = $client.GetAsync($uri, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
        $code = [int]$resp.StatusCode
        if ($resp.Content -ne $null) { $null = $resp.Content.ReadAsStringAsync().GetAwaiter().GetResult() }
        $resp.Dispose()
        $ok++
    } catch {
        $code = -1
        $err++
        Write-Host ("ERROR: {0}" -f $_.Exception.Message) -ForegroundColor Red
    }
    $t1 = [Diagnostics.Stopwatch]::GetTimestamp()
    $ms = (($t1 - $t0) * 1000.0) / [Diagnostics.Stopwatch]::Frequency

    [void]$latRoll.Add($ms)
    [void]$latAll.Add($ms)
    if ($codes.ContainsKey($code)) { $codes[$code]++ } else { $codes[$code] = 1 }

    ('{0},{1},{2},{3},{4:n3}' -f (Get-Date).ToString('o'), $ok, $err, $code, $ms) | Add-Content -Encoding ascii $outCsv

    if ($swRoll.Elapsed.TotalSeconds -ge $rollupSec) {
        $secs = [Math]::Max($swRoll.Elapsed.TotalSeconds, 0.001)
        $rps  = $latRoll.Count / $secs
        $p50  = Percentile($latRoll.ToArray(), 50)
        $p95  = Percentile($latRoll.ToArray(), 95)
        $mx   = if ($latRoll.Count) { [Math]::Round(($latRoll | Measure-Object -Maximum).Maximum, 3) } else { [double]::NaN }
        $codesStr = ($codes.Keys | Sort-Object | ForEach-Object { '{0}={1}' -f $_, $codes[$_] }) -join ' '
        Write-Host ("[{0}] RPS={1:n1} OK={2} ERR={3} p50={4:n1}ms p95={5:n1}ms max={6:n1}ms codes[{7}]"
            -f (Get-Date).ToString('HH:mm:ss'), $rps, $ok, $err, $p50, $p95, $mx, $codesStr)

        $swRoll.Restart()
        $latRoll.Clear()
        $codes.Clear()
    }

    Start-Sleep -Milliseconds $intervalMs
}

$totalSec = [Math]::Max($swAll.Elapsed.TotalSeconds, 0.001)
$p50f = Percentile($latAll.ToArray(), 50)
$p95f = Percentile($latAll.ToArray(), 95)
$maxf = if ($latAll.Count) { [Math]::Round(($latAll | Measure-Object -Maximum).Maximum, 3) } else { [double]::NaN }
$finalRps = $ok / $totalSec
$csvCount = $latAll.Count

Write-Host ("DONE {0}: OK={1} ERR={2} RPS={3:n1} p50={4:n1}ms p95={5:n1}ms max={6:n1}ms SAMPLES={7} CSV={8}"
    -f $swAll.Elapsed, $ok, $err, $finalRps, $p50f, $p95f, $maxf, $csvCount, $outCsv)

$client.Dispose()
$handler.Dispose()
