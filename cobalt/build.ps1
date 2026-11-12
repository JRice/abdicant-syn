param([string]$Compiler = 'C:\msys64\ucrt64\bin\g++.exe')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$previousPath = $env:PATH
Push-Location $repoRoot
try {
    $compilerDirectory = Split-Path -Parent $Compiler
    $env:PATH = $compilerDirectory + ';' + $env:PATH
    $cmake = Join-Path $repoRoot 'build/tools/cmake/data/bin/cmake.exe'
    $ninja = Join-Path $repoRoot 'build/tools/bin/ninja.exe'
    if (!(Test-Path -LiteralPath $Compiler) -or !(Test-Path -LiteralPath $cmake) -or !(Test-Path -LiteralPath $ninja)) {
        throw 'The workspace MinGW, CMake and Ninja tools are required. See third_party/SHIBOLETH-DEPENDENCIES.md.'
    }
    & $cmake -S cobalt -B cobalt/build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=$compilerDirectory/gcc.exe" "-DCMAKE_CXX_COMPILER=$Compiler" "-DCMAKE_MAKE_PROGRAM=$ninja"
    if ($LASTEXITCODE -ne 0) { throw 'Cobalt configuration failed.' }
    & $cmake --build cobalt/build/cmake --target cobalt_clap cobalt_vst3 cobalt_tests cobalt_transport_tests cobalt_vst3_tests -j 6
    if ($LASTEXITCODE -ne 0) { throw 'Cobalt compilation failed.' }
    foreach ($testName in 'cobalt_tests','cobalt_transport_tests','cobalt_vst3_tests') {
        & "./cobalt/build/cmake/$testName.exe" *> "cobalt/build/$testName.log"
        if ($LASTEXITCODE -ne 0) { Get-Content "cobalt/build/$testName.log"; throw "$testName failed." }
        Get-Content "cobalt/build/$testName.log" -Tail 1
    }
    python -m unittest discover -s cobalt/tests -q
    if ($LASTEXITCODE -ne 0) { throw 'Python protocol tests failed.' }
    $validator = Join-Path $repoRoot 'build/vst3-validator/bin/validator.exe'
    if (!(Test-Path -LiteralPath $validator)) { throw 'Build the workspace Steinberg validator with build-vst3.ps1 first.' }
    & $validator cobalt/build/VST3/CobaltEditor.vst3 *> cobalt/build/steinberg-validation.log
    if ($LASTEXITCODE -ne 0) { throw 'Steinberg validation failed; see cobalt/build/steinberg-validation.log.' }
    Get-Content cobalt/build/steinberg-validation.log -Tail 4
    $release = Join-Path $PSScriptRoot 'build/CobaltEditor-0.1.0-Windows-x64'
    New-Item -ItemType Directory -Force -Path "$release/licenses" | Out-Null
    Copy-Item -LiteralPath 'cobalt/build/CLAP/CobaltEditor.clap' -Destination "$release/CobaltEditor.clap" -Force
    Copy-Item -LiteralPath 'cobalt/build/VST3/CobaltEditor.vst3' -Destination $release -Recurse -Force
    Copy-Item -LiteralPath 'cobalt/INSTALL.md' -Destination "$release/INSTALL.md" -Force
    Copy-Item -LiteralPath 'LICENSE' -Destination "$release/licenses/CobaltEditor-MIT.txt" -Force
    Copy-Item -LiteralPath 'third_party/clap/LICENSE' -Destination "$release/licenses/CLAP.txt" -Force
    Copy-Item -LiteralPath 'third_party/clap-wrapper/LICENSE' -Destination "$release/licenses/CLAP-wrapper.txt" -Force
    Copy-Item -LiteralPath 'third_party/vst3sdk/LICENSE.txt' -Destination "$release/licenses/VST3-SDK.txt" -Force
    Copy-Item -LiteralPath 'third_party/ADDITIONAL-NOTICES.txt' -Destination "$release/licenses/ADDITIONAL-NOTICES.txt" -Force
    Compress-Archive -Path "$release/*" -DestinationPath 'cobalt/build/CobaltEditor-0.1.0-Windows-x64.zip' -Force
    Get-FileHash -LiteralPath 'cobalt/build/CLAP/CobaltEditor.clap','cobalt/build/VST3/CobaltEditor.vst3/Contents/x86_64-win/CobaltEditor.vst3' -Algorithm SHA256 | Format-List | Out-File -Encoding utf8 cobalt/build/binary-hashes.txt
    Write-Output 'Packaged cobalt/build/CobaltEditor-0.1.0-Windows-x64.zip'
} finally {
    $env:PATH = $previousPath
    Pop-Location
}
