# 静态链接构建：产出单文件 exe（无 Qt / MinGW 运行时 DLL 依赖）
# 前置：静态 Qt 已编译安装（本机在 D:\Qt\6.11.1\static-mingw，由 D:\Qt\static-build 编译）
#
# 用法：powershell -File scripts\build_static.ps1
# 可用环境变量覆盖工具链：SUDOKU_QT_STATIC_BIN / SUDOKU_MINGW_BIN
$ErrorActionPreference = "Continue"

$qtBin    = if ($env:SUDOKU_QT_STATIC_BIN) { $env:SUDOKU_QT_STATIC_BIN } else { "D:\Qt\6.11.1\static-mingw\bin" }
$mingwBin = if ($env:SUDOKU_MINGW_BIN)     { $env:SUDOKU_MINGW_BIN }     else { "D:\Qt\Tools\mingw1310_64\bin" }
$env:PATH = "$mingwBin;$qtBin;" + $env:PATH

$root  = Split-Path $PSScriptRoot -Parent     # <repo>/scripts -> <repo>
$build = Join-Path $root "build-static"
$out   = Join-Path $root "dist"

Set-Location $root
if (Test-Path $build) { Remove-Item -Recurse -Force $build }
New-Item -ItemType Directory -Force -Path $build | Out-Null
Set-Location $build

& "$qtBin\qmake.exe" (Join-Path $root "qt\sudoku_static.pro") -spec win32-g++ "CONFIG+=release" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Output "QMAKE_FAILED"; exit 1 }

& "$mingwBin\mingw32-make.exe" -j8 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Output "MAKE_FAILED"; exit 1 }

$exe = Join-Path $build "release\Sudoku.exe"
if (-not (Test-Path $exe)) { Write-Output "NO_EXE"; exit 1 }

New-Item -ItemType Directory -Force -Path $out | Out-Null
Copy-Item $exe (Join-Path $out "Sudoku.exe") -Force

$size = (Get-Item $exe).Length
Write-Output ("BUILD_OK " + $size + " bytes")
