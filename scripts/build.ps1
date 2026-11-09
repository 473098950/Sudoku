# Sudoku (Qt Widgets) build script -- ASCII only.
# Toolchain: Qt 6.11.1 mingw_64 + MinGW 13.1 (qmake + mingw32-make)
#
# NOTE: 本机 PATH 里另有一个 llvm-mingw 的伪 g++（clang-22），
# 编译器路径已在 sudoku.pro 里写死（可用环境变量 SUDOKU_QT_BIN / SUDOKU_MINGW_BIN 覆盖），不依赖 PATH。
#
# 用法：powershell -File scripts\build.ps1        （可在任意目录调用，路径由脚本自身位置推导）
$ErrorActionPreference = "Continue"

$qtBin    = if ($env:SUDOKU_QT_BIN)    { $env:SUDOKU_QT_BIN }    else { "D:\Qt\6.11.1\mingw_64\bin" }
$mingwBin = if ($env:SUDOKU_MINGW_BIN) { $env:SUDOKU_MINGW_BIN } else { "D:\Qt\Tools\mingw1310_64\bin" }
$env:PATH = "$mingwBin;$qtBin;" + $env:PATH

Remove-Item Env:HTTP_PROXY  -ErrorAction SilentlyContinue
Remove-Item Env:HTTPS_PROXY -ErrorAction SilentlyContinue
Remove-Item Env:ALL_PROXY   -ErrorAction SilentlyContinue

$root  = Split-Path $PSScriptRoot -Parent     # <repo>/scripts -> <repo>
$proj  = Join-Path $root "qt"
$build = Join-Path $root "build"
$bin   = Join-Path $root "bin"

Set-Location $root                                   # 必须先离开 build，否则目录占用删不掉
if (Test-Path $build) { Remove-Item -Recurse -Force $build }
New-Item -ItemType Directory -Force -Path $build | Out-Null
Set-Location $build

& "$qtBin\qmake.exe" (Join-Path $proj "sudoku.pro") 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Output "QMAKE_FAILED"; exit 1 }

& "$mingwBin\mingw32-make.exe" -j8 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Output "MAKE_FAILED"; exit 1 }

$exe = Join-Path $build "release\Sudoku.exe"
if (-not (Test-Path $exe)) { Write-Output "NO_EXE"; exit 1 }

New-Item -ItemType Directory -Force -Path $bin | Out-Null
Copy-Item $exe (Join-Path $bin "Sudoku.exe") -Force
Copy-Item (Join-Path $root "icon.ico") (Join-Path $bin "icon.ico") -Force
& "$qtBin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $bin "Sudoku.exe") | Out-Null

Write-Output ("BUILD_OK " + (Get-Item (Join-Path $bin "Sudoku.exe")).Length + " bytes")
