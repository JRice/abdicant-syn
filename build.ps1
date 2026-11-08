param([string]$Compiler = 'C:\msys64\ucrt64\bin\g++.exe')
$ErrorActionPreference = 'Stop'
$previousPath = $env:PATH
$env:PATH = (Split-Path -Parent $Compiler) + ';' + $env:PATH
Push-Location $PSScriptRoot
try {
    if (!(Test-Path -LiteralPath $Compiler)) { throw "Set -Compiler to a MinGW-w64 g++.exe installation." }
    if (!(Test-Path 'third_party/clap/include/clap/clap.h')) { throw 'The official CLAP headers are missing. See README.md.' }
    New-Item -ItemType Directory -Path build -Force | Out-Null
    $compilerFlags = @('-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-static', '-Ithird_party/clap/include')
    $resourceCompiler = Join-Path (Split-Path -Parent $Compiler) 'windres.exe'
    & $resourceCompiler '-I' 'src' 'src/resources.rc' '-O' 'coff' '-o' 'build/resources.o'
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
    & $Compiler @compilerFlags '-shared' 'src/plugin.cpp' 'build/resources.o' '-lgdiplus' '-lole32' '-lgdi32' '-luser32' '-o' 'build/Shiboleth.clap'
    if ($LASTEXITCODE -ne 0) { throw 'Plugin compilation failed.' }
    # Remove only the superseded generated filename, avoiding duplicate plug-in IDs.
    if (Test-Path -LiteralPath 'build/MonoStudy.clap') { Remove-Item -LiteralPath 'build/MonoStudy.clap' }
    & $Compiler @compilerFlags 'tests/tests.cpp' '-o' 'build/tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    & '.\build\tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Verification failed.' }
    & $Compiler @compilerFlags 'tests/editor_tests.cpp' '-lgdiplus' '-lole32' '-lgdi32' '-luser32' '-o' 'build/editor_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Editor test compilation failed.' }
    & '.\build\editor_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Editor verification failed.' }
} finally { $env:PATH = $previousPath; Pop-Location }
