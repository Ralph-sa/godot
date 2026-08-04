[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StoreFile,
    [Parameter(Mandatory = $true)]
    [string]$StorePassword,
    [Parameter(Mandatory = $true)]
    [string]$KeyAlias,
    [Parameter(Mandatory = $true)]
    [string]$KeyPassword,
    [Parameter(Mandatory = $true)]
    [string]$ProfileFile,
    [Parameter(Mandatory = $true)]
    [string]$CertFile,
    [string]$DevEcoHome = "C:\Program Files\Huawei\DevEco Studio"
)

$ErrorActionPreference = "Stop"

$projectDir = $PSScriptRoot
$targetDir = Join-Path $projectDir ".signing\harmonyos"
$node = Join-Path $DevEcoHome "tools\node\node.exe"
$passwordTool = Join-Path $projectDir "encrypt_signing_password.cjs"
$localConfig = Join-Path $projectDir "signing.local.json"

foreach ($requiredPath in @($StoreFile, $ProfileFile, $CertFile, $node, $passwordTool)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required HarmonyOS signing material not found: $requiredPath"
    }
}

New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
$localStore = Join-Path $targetDir ([System.IO.Path]::GetFileName($StoreFile))
$localProfile = Join-Path $targetDir ([System.IO.Path]::GetFileName($ProfileFile))
$localCert = Join-Path $targetDir ([System.IO.Path]::GetFileName($CertFile))
Copy-Item -LiteralPath $StoreFile -Destination $localStore -Force
Copy-Item -LiteralPath $ProfileFile -Destination $localProfile -Force
Copy-Item -LiteralPath $CertFile -Destination $localCert -Force

$encryptedStorePassword = ($StorePassword | & $node $passwordTool $targetDir).Trim()
$encryptedKeyPassword = ($KeyPassword | & $node $passwordTool $targetDir).Trim()
if ($LASTEXITCODE -ne 0 -or $encryptedStorePassword.Length -lt 32 -or $encryptedKeyPassword.Length -lt 32) {
    throw "Failed to encrypt HarmonyOS signing passwords for Hvigor."
}

$config = [ordered]@{
    name = "agcHarmonyOS"
    type = "HarmonyOS"
    material = [ordered]@{
        storeFile = $localStore
        storePassword = $encryptedStorePassword
        keyAlias = $KeyAlias
        keyPassword = $encryptedKeyPassword
        signAlg = "SHA256withECDSA"
        profile = $localProfile
        certpath = $localCert
    }
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($localConfig, ($config | ConvertTo-Json -Depth 10), $utf8NoBom)
Write-Host "AGC HarmonyOS signing configuration generated: $localConfig"
