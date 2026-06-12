param(
  [string]$Port = "COM15",
  [int]$Baud = 115200,
  [int]$Seconds = 180,
  [string]$Log = "d:\Luni\Luni_Robot\esp32-c5\c5_serial.log"
)
"" | Out-File -Encoding utf8 $Log
$sp = New-Object System.IO.Ports.SerialPort $Port,$Baud,None,8,one
$sp.ReadTimeout = 500
$sp.NewLine = "`n"
try { $sp.Open() } catch { "OPEN FAILED: $($_.Exception.Message)" | Out-File -Append -Encoding utf8 $Log; exit 1 }
$deadline = (Get-Date).AddSeconds($Seconds)
$sb = New-Object System.Text.StringBuilder
while ((Get-Date) -lt $deadline) {
  try {
    $chunk = $sp.ReadExisting()
    if ($chunk.Length -gt 0) {
      [void]$sb.Append($chunk)
      $text = $sb.ToString()
      $idx = $text.LastIndexOf("`n")
      if ($idx -ge 0) {
        $complete = $text.Substring(0, $idx + 1)
        $complete.TrimEnd("`n").Split("`n") | ForEach-Object { $_.TrimEnd("`r") } | Out-File -Append -Encoding utf8 $Log
        [void]$sb.Clear()
        [void]$sb.Append($text.Substring($idx + 1))
      }
    } else {
      Start-Sleep -Milliseconds 50
    }
  } catch { Start-Sleep -Milliseconds 50 }
}
$sp.Close()
"=== logger ended ===" | Out-File -Append -Encoding utf8 $Log
