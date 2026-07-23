$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $projectRoot
try {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) {
        $python = Get-Command py -ErrorAction SilentlyContinue
    }
    if (-not $python) {
        $platformIoPython = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\python.exe"
        if (Test-Path -LiteralPath $platformIoPython) {
            $python = Get-Item -LiteralPath $platformIoPython
        }
    }
    if (-not $python) {
        throw "Python introuvable (python, py ou environnement PlatformIO)"
    }
    $pythonExe = if ($python.Source) { $python.Source } else { $python.FullName }
    $previousPioEnv = $env:PIOENV
    $previousCfgdocProfile = $env:FLOW_CFGDOC_PROFILE
    $env:PIOENV = "Waveshare-ESP32-S3"
    $env:FLOW_CFGDOC_PROFILE = "flowios3"

    & $pythonExe "scripts/generate_config_docs.py"
    if ($LASTEXITCODE -ne 0) { throw "generate_config_docs.py failed" }
    & $pythonExe "scripts/generate_cfgdoc_chunks.py"
    if ($LASTEXITCODE -ne 0) { throw "generate_cfgdoc_chunks.py failed" }

    $assets = @(
        "data/webinterface/index.html",
        "data/webinterface/sh.html",
        "data/webinterface/app.js",
        "data/webinterface/i18n/fr.json",
        "data/webinterface/i18n/en.json",
        "data/webinterface/app-core.css",
        "data/webinterface/app-core.js",
        "data/webinterface/light.html",
        "data/webinterface/light.css",
        "data/webinterface/light.js",
        "data/webinterface/runtimeui.json"
    )
    $assets += Get-ChildItem -LiteralPath "data/wc" -Filter "*.j" -File -ErrorAction SilentlyContinue |
        ForEach-Object { $_.FullName }

    foreach ($asset in $assets) {
        $sourcePath = (Resolve-Path -LiteralPath $asset).Path
        $targetPath = "$sourcePath.gz"
        $input = [System.IO.File]::OpenRead($sourcePath)
        try {
            $output = [System.IO.File]::Create($targetPath)
            try {
                $gzip = [System.IO.Compression.GZipStream]::new(
                    $output,
                    [System.IO.Compression.CompressionLevel]::Optimal,
                    $false
                )
                try {
                    $input.CopyTo($gzip)
                } finally {
                    $gzip.Dispose()
                }
            } finally {
                $output.Dispose()
            }
        } finally {
            $input.Dispose()
        }
    }

    @(
        "data/webinterface/cfgdocs.json",
        "data/webinterface/cfgmods.json",
        "data/webinterface/cfgdocs.jz",
        "data/webinterface/cfgmods.jz"
    ) | ForEach-Object {
        if (Test-Path -LiteralPath $_) {
            Remove-Item -LiteralPath $_
        }
    }
    $env:PIOENV = $previousPioEnv
    $env:FLOW_CFGDOC_PROFILE = $previousCfgdocProfile
} finally {
    Pop-Location
}
