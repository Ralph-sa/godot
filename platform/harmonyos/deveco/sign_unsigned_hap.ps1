[CmdletBinding()]
param(
    [ValidateSet("debug", "release")]
    [string]$BuildMode = "debug",
    [string]$DevEcoHome = "C:\Program Files\Huawei\DevEco Studio"
)

$ErrorActionPreference = "Stop"

$projectDir = $PSScriptRoot
$configPath = Join-Path $projectDir "signing.local.json"
$buildScript = Join-Path $projectDir "build_hap_only.bat"
$java = Join-Path $DevEcoHome "jbr\bin\java.exe"
$signTool = Join-Path $DevEcoHome "sdk\default\openharmony\toolchains\lib\hap-sign-tool.jar"

if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectDir "configure_local_signing.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure local signing."
    }
}

$config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
& cmd.exe /c ('"' + $buildScript + '" ' + $BuildMode)
if ($LASTEXITCODE -ne 0) {
    throw "Hvigor HAP build failed."
}

$outputDir = Join-Path $projectDir "entry\build\default\outputs\default"
if ($config.type -eq "HarmonyOS") {
    $harmonySigned = Get-ChildItem -LiteralPath $outputDir -Filter "*-signed.hap" |
        Where-Object { $_.Name -notlike "*-local-signed.hap" } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $harmonySigned) {
        throw "HarmonyOS signing was selected, but no signed HAP was produced."
    }
    Write-Host "HarmonyOS signed HAP: $($harmonySigned.FullName)"
    exit 0
}

if ($config.type -ne "OpenHarmony") {
    throw "Unsupported local signing type: $($config.type)"
}

$unsignedHap = Get-ChildItem -LiteralPath $outputDir -Filter "*-unsigned.hap" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $unsignedHap) {
    throw "Unsigned HAP was not produced."
}

$signedHap = Join-Path $outputDir ($unsignedHap.BaseName.Replace("-unsigned", "-local-signed") + ".hap")
$verifyDir = Join-Path $projectDir ".signing\hap-verify"
$outCertChain = Join-Path $verifyDir "signed-hap-cert-chain.cer"
$outProfile = Join-Path $verifyDir "signed-hap-profile.p7b"
$profileVerify = Join-Path $verifyDir "signed-hap-profile.verify.json"
New-Item -ItemType Directory -Path $verifyDir -Force | Out-Null

$material = $config.material
$signArgs = @(
    "-jar", $signTool, "sign-app",
    "-mode", "localSign",
    "-keyAlias", $material.keyAlias,
    "-keyPwd", $material.keyPassword,
    "-appCertFile", $material.certpath,
    "-profileFile", $material.profile,
    "-profileSigned", "1",
    "-inFile", $unsignedHap.FullName,
    "-signAlg", $material.signAlg,
    "-keystoreFile", $material.storeFile,
    "-keystorePwd", $material.storePassword,
    "-outFile", $signedHap,
    "-compatibleVersion", "24",
    "-signCode", "1"
)
& $java @signArgs
if ($LASTEXITCODE -ne 0) {
    throw "OpenHarmony post-sign failed."
}

& $java -jar $signTool verify-app -inFile $signedHap -outCertChain $outCertChain -outProfile $outProfile
if ($LASTEXITCODE -ne 0) {
    throw "Signed HAP verification failed."
}

& $java -jar $signTool verify-profile -inFile $outProfile -outFile $profileVerify
if ($LASTEXITCODE -ne 0) {
    throw "Embedded profile verification failed."
}

Write-Host "Local signed HAP: $signedHap"
Write-Host "Verification result: $profileVerify"
Write-Warning "This OpenHarmony signature is for local cryptographic/package testing only, not Huawei device distribution."
