# Run in PowerShell:  .\fetch_missing.ps1
# Downloads the 11 files the first download missed (shared bitmap-font descriptor + 10 device sounds).
# Only needed ONCE; afterwards the game runs fully offline.
param([string]$Out = (Join-Path $PSScriptRoot "game"))

$base = "https://g2.igroutka.ru/games/18/fireboy_and_watergirl_in_the_forest_temple/fireboy_and_witergirl/"
$files = @("assets/fonts/font.fnt") + ("Lever","Platform","PortalOpen","PortalLoop","PortalClose",
          "PortalTransport","Wind","Slider","Clock","LightPusher" | ForEach-Object { "assets/audio/$_.mp3" })

foreach ($rel in $files) {
    $dest = Join-Path $Out $rel
    New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
    try   { Invoke-WebRequest ($base + $rel) -OutFile $dest -UseBasicParsing; "OK    $rel" }
    catch { "FAIL  $rel" }
}
