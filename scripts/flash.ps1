# コマンドライン引数を取得
param(
    [Parameter(Mandatory=$false)]
    [string]$Side = '',
    [Parameter(Mandatory=$false)]
    [string]$BuildDir = "C:\Users\tatuy\Documents\zmk\app\build\MKB\zephyr", # デフォルトのビルドディレクトリ
    [Parameter(Mandatory=$false)]
    [string]$Uf2FileName = "zmk.uf2" # UF2ファイル名
)

$FullUf2Path = Join-Path $BuildDir $Uf2FileName
$lastKnownWriteTime = [datetime]::MinValue # ファイルの初回検出/更新を検知するために、非常に古い日付で初期化

Write-Host "Starting UF2 Flasher and Monitor."
Write-Host "Monitoring firmware file: $FullUf2Path"
Write-Host "Press Ctrl+C at any time to stop monitoring and exit."

try {
    while ($true) { # ファイル監視のメインループ
        if (Test-Path $FullUf2Path) {
            $currentWriteTime = (Get-Item $FullUf2Path).LastWriteTime
            if ($currentWriteTime -gt $lastKnownWriteTime) {
                Write-Host ("Firmware file '{0}' updated at {1}. Preparing to flash." -f $FullUf2Path, $currentWriteTime)
                $lastKnownWriteTime = $currentWriteTime

                # --- 書き込み処理の開始 ---
                Write-Host "Waiting for new drive... (Press Ctrl+C to cancel this flashing attempt and return to monitoring)"
                
                # 初期のドライブ一覧を取得 (この書き込み試行のため)
                $initialDrivesForThisAttempt = Get-PSDrive -PSProvider FileSystem
                
                $flashAttemptActive = $true
                while ($flashAttemptActive) { # 個別の書き込み試行ループ
                    try {
                        Start-Sleep -Milliseconds 500 # 新しいドライブをポーリング

                        $currentDrives = Get-PSDrive -PSProvider FileSystem
                        
                        # 新しく追加されたドライブを検出
                        $newDrives = $currentDrives | Where-Object {
                            $driveInfo = $_
                            -not ($initialDrivesForThisAttempt | Where-Object { $_.Name -eq $driveInfo.Name })
                        }

                        if ($newDrives) {
                            $targetDrive = $newDrives[0]
                            # ターゲットパスには元のファイル名を使用
                            $targetPath = Join-Path ($targetDrive.Name + ":\") $Uf2FileName 

                            Write-Host ("New drive detected: {0}" -f $targetDrive.Name)
                            Write-Host ("Copying firmware from '{0}' to '{1}'..." -f $FullUf2Path, $targetPath)

                            # ファイルサイズを取得
                            $fileSize = (Get-Item $FullUf2Path).Length
                            if ($fileSize -eq 0) {
                                Write-Warning "Firmware file '$FullUf2Path' is empty. Skipping copy."
                                $flashAttemptActive = $false # この書き込み試行を中止
                                continue # if ($newDrives) ブロックを抜け、次のループ処理へ
                            }
                            
                            $buffer = New-Object byte[] (1MB) # 1MB バッファ
                            $totalBytesRead = 0

                            # ファイルストリームを開く
                            $sourceStream = $null
                            $destinationStream = $null
                            try {
                                $sourceStream = [System.IO.File]::OpenRead($FullUf2Path)
                                $destinationStream = [System.IO.File]::Create($targetPath)

                                do {
                                    $bytesRead = $sourceStream.Read($buffer, 0, $buffer.Length)
                                    if ($bytesRead -gt 0) {
                                        $destinationStream.Write($buffer, 0, $bytesRead)
                                        $totalBytesRead += $bytesRead
                                        # パーセンテージ計算のために浮動小数点除算を確実に行う
                                        $percentComplete = [math]::Min(100, ($totalBytesRead * 100.0) / $fileSize) 
                                        Write-Progress -Activity "Copying firmware" -Status ("{0}% complete" -f [math]::Round($percentComplete)) -PercentComplete $percentComplete
                                    }
                                } while ($bytesRead -gt 0)
                            }
                            finally {
                                if ($sourceStream) { $sourceStream.Close(); $sourceStream.Dispose() }
                                if ($destinationStream) { $destinationStream.Close(); $destinationStream.Dispose() }
                            }

                            Write-Progress -Activity "Copying firmware" -Completed
                            Write-Host "Flash completed!"
                            $flashAttemptActive = $false # この書き込み試行のドライブ検出ループを終了
                        }
                        # 新しいドライブがない場合はループを継続してポーリング
                    }
                    catch [System.Management.Automation.Host.HostException] {
                        Write-Host "`nFlashing attempt cancelled by user. Returning to file monitoring." -ForegroundColor Yellow
                        $flashAttemptActive = $false # ドライブ検出ループを終了
                    }
                    catch {
                        Write-Error "An error occurred during the flashing process: $_"
                        Write-Host "Aborting this flash attempt. Returning to file monitoring." -ForegroundColor Yellow
                        $flashAttemptActive = $false # ドライブ検出ループを終了
                    }
                } # while ($flashAttemptActive) の終了
                Write-Host ("Now monitoring '{0}' for the next update." -f $FullUf2Path)
                # --- 書き込み処理の終了 ---
            }
            # ファイルが存在するが更新されていない場合は何もせず、次のチェックを待つ
        } else {
            # ファイルが存在しない場合
            Write-Host ("Firmware file '{0}' not found. Waiting for it to appear..." -f $FullUf2Path) -ForegroundColor Yellow
            $lastKnownWriteTime = [datetime]::MinValue # リセットし、ファイル出現時に「新規」として扱われるようにする
        }

        Start-Sleep -Seconds 2 # ファイル監視のポーリング間隔
    }
}
catch [System.Management.Automation.Host.HostException] {
    Write-Host "`nMonitoring cancelled by user. Exiting." -ForegroundColor Green
    exit 0
}
catch {
    Write-Error "An unexpected error occurred in the main monitoring loop: $_"
    exit 1
}