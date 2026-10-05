# Builds the release zip from an already-built plugin binary.
#
# Usage (from the repository root):
#   powershell -ExecutionPolicy Bypass -File scripts\package-release.ps1 -Version 0.1.0
#
# Build the plugin first (see docs/BUILD_SETUP.md).
# Output: dist\AVI-MCP-v<Version>-win64.zip and a .sha256 file next to it.
param(
    [Parameter(Mandatory = $true)]
    [string]$Version
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$pluginBinary = Join-Path $root "bridge-plugin\build\Release\AviUtl2McpBridge.aux2"
$name = "AVI-MCP-v$Version-win64"
$dist = Join-Path $root "dist"
$stage = Join-Path $dist $name
$zipPath = Join-Path $dist "$name.zip"

# Refuse to package uncommitted changes, so the zip always matches a commit.
$dirty = git -C $root status --porcelain
if ($dirty) {
    throw "Uncommitted changes found. Commit them first:`n$dirty"
}

if (-not (Test-Path $pluginBinary)) {
    throw "Plugin binary not found: $pluginBinary (build it first)"
}

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $stage "plugin") | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stage "server\src") | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stage "server\test") | Out-Null

Copy-Item $pluginBinary (Join-Path $stage "plugin")
Copy-Item (Join-Path $root "mcp-server\package.json") (Join-Path $stage "server")
Copy-Item (Join-Path $root "mcp-server\package-lock.json") (Join-Path $stage "server")
Copy-Item (Join-Path $root "mcp-server\src\*.js") (Join-Path $stage "server\src")
Copy-Item (Join-Path $root "mcp-server\test\manual_client_test.js") (Join-Path $stage "server\test")
Copy-Item (Join-Path $root "LICENSE") $stage
Copy-Item (Join-Path $root "THIRD_PARTY_LICENSES.md") $stage
Copy-Item (Join-Path $root "release\README.txt") $stage
Copy-Item (Join-Path $root "release\mcp.json.example") $stage

if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path $stage -DestinationPath $zipPath

$hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLower()
Set-Content -Path "$zipPath.sha256" -Value "$hash  $name.zip" -Encoding ascii

Write-Host "Created: $zipPath"
Write-Host "SHA256 : $hash"
