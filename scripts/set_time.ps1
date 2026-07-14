# PCの現在時刻を M5Stack StopWatch に送って時刻合わせするスクリプト
# 使い方:  powershell -ExecutionPolicy Bypass -File .\scripts\set_time.ps1
#          ポートを指定する場合:  ... set_time.ps1 -Port COM5
# 注意: PlatformIOのシリアルモニタが開いていると失敗します。先に閉じてください(Ctrl+C)。

param([string]$Port)

if (-not $Port) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 0) {
        Write-Host "COMポートが見つかりません。USBケーブルの接続を確認してください。" -ForegroundColor Red
        exit 1
    }
    $Port = $ports[-1]
    Write-Host "ポート $Port を使います (違う場合は -Port COM番号 で指定)"
}

$sp = New-Object System.IO.Ports.SerialPort $Port, 115200, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$sp.NewLine = "`n"
$sp.DtrEnable = $true
$sp.RtsEnable = $true
$sp.ReadTimeout = 500

try {
    $sp.Open()
} catch {
    Write-Host "ポート $Port を開けませんでした。シリアルモニタを閉じてから再実行してください。" -ForegroundColor Red
    exit 1
}

Start-Sleep -Milliseconds 500
$sp.DiscardInBuffer()

$now = Get-Date
$cmd = "D " + $now.ToString("yyyy-MM-dd HH:mm:ss")
Write-Host "送信: $cmd"
$sp.WriteLine($cmd)

# デバイスからの返事を最大2秒待つ
$deadline = (Get-Date).AddSeconds(2)
$resp = ""
while ((Get-Date) -lt $deadline -and $resp -notmatch "OK|NG") {
    Start-Sleep -Milliseconds 100
    $resp += $sp.ReadExisting()
}
$sp.Close()

if ($resp -match "OK") {
    Write-Host "時刻を合わせました! デバイスの時計を確認してください。" -ForegroundColor Green
} elseif ($resp.Length -gt 0) {
    Write-Host "デバイスの返事: $resp" -ForegroundColor Yellow
} else {
    Write-Host "デバイスから返事がありませんでした。" -ForegroundColor Yellow
    Write-Host "・最新のファームウェアを書き込み済みか確認 (PlatformIOのUpload)"
    Write-Host "・別のポートを試す: このスクリプトに -Port COM3 などを付けて再実行"
}
