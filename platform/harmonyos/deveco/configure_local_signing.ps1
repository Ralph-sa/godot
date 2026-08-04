[CmdletBinding()]
param(
    [string]$BundleName = "com.godotengine.editor",
    [string]$DevEcoHome = "C:\Program Files\Huawei\DevEco Studio",
    [string]$StorePassword = "123456",
    [string]$KeyPassword = "123456"
)

$ErrorActionPreference = "Stop"

$projectDir = $PSScriptRoot
$signingDir = Join-Path $projectDir ".signing"
$sdkLibDir = Join-Path $DevEcoHome "sdk\default\openharmony\toolchains\lib"
$java = Join-Path $DevEcoHome "jbr\bin\java.exe"
$keytool = Join-Path $DevEcoHome "jbr\bin\keytool.exe"
$signTool = Join-Path $sdkLibDir "hap-sign-tool.jar"
$sdkKeyStore = Join-Path $sdkLibDir "OpenHarmony.p12"
$profileTemplate = Join-Path $sdkLibDir "UnsgnedReleasedProfileTemplate.json"
$profileSigningCert = Join-Path $sdkLibDir "OpenHarmonyProfileRelease.pem"

$applicationAlias = "openharmony application release"
$applicationCaAlias = "openharmony application ca"
$applicationRootCaAlias = "openharmony application root ca"
$profileAlias = "openharmony application profile release"
$keyStore = Join-Path $signingDir "OpenHarmony.p12"
$unsignedProfile = Join-Path $signingDir "GodotEditorReleaseProfile.json"
$signedProfile = Join-Path $signingDir "GodotEditorReleaseProfile.p7b"
$applicationCert = Join-Path $signingDir "GodotEditorApplicationRelease.cer"
$applicationCa = Join-Path $signingDir "OpenHarmonyApplicationCA.pem"
$applicationRootCa = Join-Path $signingDir "OpenHarmonyApplicationRootCA.pem"
$verifyResult = Join-Path $signingDir "GodotEditorReleaseProfile.verify.json"
$localConfig = Join-Path $projectDir "signing.local.json"

foreach ($requiredPath in @($java, $keytool, $signTool, $sdkKeyStore, $profileTemplate, $profileSigningCert)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required signing tool or material not found: $requiredPath"
    }
}

New-Item -ItemType Directory -Path $signingDir -Force | Out-Null
Copy-Item -LiteralPath $sdkKeyStore -Destination $keyStore -Force

function Export-CertificatePem([string]$Alias, [string]$OutputPath) {
    $exportArgs = @(
        "-exportcert", "-rfc",
        "-alias", $Alias,
        "-keystore", $keyStore,
        "-storetype", "PKCS12",
        "-storepass", $StorePassword,
        "-file", $OutputPath
    )
    & $keytool @exportArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to export certificate alias: $Alias"
    }
}

Export-CertificatePem $applicationCaAlias $applicationCa
Export-CertificatePem $applicationRootCaAlias $applicationRootCa

$profile = Get-Content -LiteralPath $profileTemplate -Raw | ConvertFrom-Json
$certificatePem = $profile.'bundle-info'.'distribution-certificate'
$certificateChain = $certificatePem + [System.IO.File]::ReadAllText($applicationCa) + [System.IO.File]::ReadAllText($applicationRootCa)
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($applicationCert, $certificateChain, $utf8NoBom)
$now = [DateTimeOffset]::UtcNow
$profile.uuid = [Guid]::NewGuid().ToString()
$profile.validity.'not-before' = $now.AddDays(-1).ToUnixTimeSeconds()
$profile.validity.'not-after' = $now.AddYears(5).ToUnixTimeSeconds()
$profile.'bundle-info'.'bundle-name' = $BundleName

$profileJson = $profile | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText($unsignedProfile, $profileJson, $utf8NoBom)

$signArgs = @(
    "-jar", $signTool, "sign-profile",
    "-mode", "localSign",
    "-keyAlias", $profileAlias,
    "-keyPwd", $KeyPassword,
    "-profileCertFile", $profileSigningCert,
    "-inFile", $unsignedProfile,
    "-signAlg", "SHA256withECDSA",
    "-keystoreFile", $keyStore,
    "-keystorePwd", $StorePassword,
    "-outFile", $signedProfile
)
& $java @signArgs
if ($LASTEXITCODE -ne 0) {
    throw "Failed to sign the local OpenHarmony profile."
}

& $java -jar $signTool verify-profile -inFile $signedProfile -outFile $verifyResult
if ($LASTEXITCODE -ne 0) {
    throw "Generated profile failed cryptographic verification."
}

$signingConfig = [ordered]@{
    name = "localOpenHarmony"
    type = "OpenHarmony"
    material = [ordered]@{
        storeFile = $keyStore
        storePassword = $StorePassword
        keyAlias = $applicationAlias
        keyPassword = $KeyPassword
        signAlg = "SHA256withECDSA"
        profile = $signedProfile
        certpath = $applicationCert
    }
}

$signingJson = $signingConfig | ConvertTo-Json -Depth 10
[System.IO.File]::WriteAllText($localConfig, $signingJson, $utf8NoBom)

Write-Host "Local signing configuration generated:"
Write-Host "  Bundle: $BundleName"
Write-Host "  Config: $localConfig"
Write-Host "  Profile verification: $verifyResult"
Write-Host "  Strategy: unsigned HarmonyOS build plus local OpenHarmony post-sign"
Write-Warning "This is a local OpenHarmony cryptographic signature, not an AGC-issued HarmonyOS release signature."
Write-Warning "For Huawei devices and release distribution, refresh certificate, P12 and profile in DevEco Studio/AGC."
