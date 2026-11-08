param([string]$Compiler = 'C:\msys64\ucrt64\bin\g++.exe')
$ErrorActionPreference = 'Stop'
$previousPath = $env:PATH
$env:PATH = (Split-Path -Parent $Compiler) + ';' + $env:PATH
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    $flags = @('-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-static', '-I../third_party/clap/include')
    $resourceCompiler = Join-Path (Split-Path -Parent $Compiler) 'windres.exe'
    & $resourceCompiler '-I' 'src' 'src/resources.rc' '-O' 'coff' '-o' 'build/resources.o'
    if ($LASTEXITCODE -ne 0) { throw 'Imperius artwork compilation failed.' }
    & $Compiler @flags '-shared' 'src/plugin.cpp' 'build/resources.o' '-lgdiplus' '-lole32' '-lgdi32' '-luser32' '-o' 'build/Imperius.clap'
    if ($LASTEXITCODE -ne 0) { throw 'Imperius plugin compilation failed.' }
    & $Compiler @flags 'tests/plugin_tests.cpp' '-o' 'build/plugin_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Imperius host test compilation failed.' }
    & '.\build\plugin_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Imperius host verification failed.' }
    & $Compiler @flags 'tests/editor_tests.cpp' '-lgdiplus' '-lole32' '-lgdi32' '-luser32' '-o' 'build/editor_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Imperius editor test compilation failed.' }
    & '.\build\editor_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Imperius editor verification failed.' }
    & $Compiler '-std=c++20' '-O2' '-Wall' '-Wextra' '-Wpedantic' '-static' 'tests/audition.cpp' '-o' 'build/audition.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Imperius compilation failed.' }
    & '.\build\audition.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Imperius verification failed.' }
} finally { $env:PATH = $previousPath; Pop-Location }
