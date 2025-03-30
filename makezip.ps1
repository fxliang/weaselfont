param([string]$zipFileName = "weaselfont.zip")
$itemsToArchive = @("weaselfont.exe", "LICENSE.txt", "data.json")
if (Test-Path $zipFileName) {
  Remove-Item $zipFileName
}
foreach ($item in $itemsToArchive) {
  if (Test-Path $item) {
    Compress-Archive -Path $item -Update -DestinationPath $zipFileName
  } else {
    Write-Host "Warning: $item does not exist and will not be included in the archive."
  }
}
Write-Host "Files and directories have been archived into $zipFileName"
