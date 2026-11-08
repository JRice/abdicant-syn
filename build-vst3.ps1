param([string]$Compiler = 'C:\msys64\ucrt64\bin\g++.exe')
$ErrorActionPreference = 'Stop'
$previousPath = $env:PATH
Push-Location $PSScriptRoot
try {
    & ./build.ps1 -Compiler $Compiler
    $compilerDirectory = Split-Path -Parent $Compiler
    $env:PATH = $compilerDirectory + ';' + $env:PATH
    $cmake = Join-Path $PSScriptRoot 'build/tools/cmake/data/bin/cmake.exe'
    $ninja = Join-Path $PSScriptRoot 'build/tools/bin/ninja.exe'
    if (!(Test-Path $cmake) -or !(Test-Path $ninja)) { throw 'See third_party/SHIBOLETH-DEPENDENCIES.md for project-local build tools.' }
    $common = @('-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_C_COMPILER=$compilerDirectory/gcc.exe", "-DCMAKE_CXX_COMPILER=$Compiler", "-DCMAKE_MAKE_PROGRAM=$ninja")
    & $cmake -S . -B build/vst3-cmake @common
    if ($LASTEXITCODE -ne 0) { throw 'VST3 configuration failed.' }
    & $cmake --build build/vst3-cmake --target shiboleth_vst3 vst3_tests -j 6
    if ($LASTEXITCODE -ne 0) { throw 'VST3 build failed.' }
    & ./build/vst3-cmake/vst3_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'VST3 integration tests failed.' }
    & $cmake -S third_party/vst3sdk -B build/vst3-validator @common '-DCMAKE_EXE_LINKER_FLAGS=-municode -static' -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=ON -DSMTG_CREATE_PLUGIN_LINK=OFF
    if ($LASTEXITCODE -ne 0) { throw 'Validator configuration failed.' }
    & $cmake --build build/vst3-validator --target validator -j 6
    if ($LASTEXITCODE -ne 0) { throw 'Validator build failed.' }
    & ./build/vst3-validator/bin/validator.exe build/VST3/Shiboleth.vst3 *> build/vst3-validation.log
    if ($LASTEXITCODE -ne 0) { throw 'VST3 validation failed; see build/vst3-validation.log.' }
    Get-Content build/vst3-validation.log -Tail 4
    $release = 'build/Shiboleth-Windows-x64'
    New-Item -ItemType Directory -Force "$release/licenses" | Out-Null
    Copy-Item -LiteralPath 'build/VST3/Shiboleth.vst3' -Destination $release -Recurse -Force
    Copy-Item -LiteralPath 'build/Shiboleth.clap' -Destination $release -Force
    Copy-Item -LiteralPath 'SHARING.txt' -Destination "$release/INSTALL.txt" -Force
    Copy-Item -LiteralPath 'third_party/clap/LICENSE' -Destination "$release/licenses/CLAP.txt" -Force
    Copy-Item -LiteralPath 'third_party/clap-wrapper/LICENSE' -Destination "$release/licenses/CLAP-wrapper.txt" -Force
    Copy-Item -LiteralPath 'third_party/vst3sdk/LICENSE.txt' -Destination "$release/licenses/VST3-SDK.txt" -Force
    Copy-Item -LiteralPath 'third_party/ADDITIONAL-NOTICES.txt' -Destination "$release/licenses/ADDITIONAL-NOTICES.txt" -Force
    Compress-Archive -Path "$release/*" -DestinationPath 'build/Shiboleth-0.5.3-Windows-x64.zip' -Force
    Write-Output 'Created build/Shiboleth-0.5.3-Windows-x64.zip'
} finally { $env:PATH = $previousPath; Pop-Location }
