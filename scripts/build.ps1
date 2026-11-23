param([string]$BuildRoot = '', [switch]$Package)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (!$BuildRoot) { $BuildRoot = Join-Path $repo 'build' }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$downloads = Join-Path $BuildRoot 'downloads'
$sources = Join-Path $BuildRoot 'sources'
$prefix = Join-Path $BuildRoot 'deps'
New-Item -ItemType Directory -Force $downloads,$sources | Out-Null
function Run-CMake([string[]]$Arguments) {
    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) { throw "CMake failed: $Arguments" }
}
function Source-Archive($Name, $Url, $Hash, $Folder) {
    $archive = Join-Path $downloads $Name
    if (!(Test-Path -LiteralPath $archive)) { Invoke-WebRequest -UseBasicParsing $Url -OutFile $archive }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $Hash) { throw "SHA256 mismatch: $Name" }
    $source = Join-Path $sources $Folder
    if (!(Test-Path -LiteralPath $source)) {
        & tar -xf $archive -C $sources
        if ($LASTEXITCODE -ne 0) { throw "Cannot extract $Name" }
    }
    return $source
}
$zlib = Source-Archive 'zlib-1.3.1.tar.gz' 'https://zlib.net/fossils/zlib-1.3.1.tar.gz' '9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23' 'zlib-1.3.1'
$hdf5 = Source-Archive 'hdf5-1.14.6.tar.gz' 'https://github.com/HDFGroup/hdf5/releases/download/hdf5_1.14.6/hdf5-1.14.6.tar.gz' 'e4defbac30f50d64e1556374aa49e574417c9e72c6b1de7a4ff88c4b1bea6e9b' 'hdf5-1.14.6'
$tcl = Source-Archive 'tcl-8.6.14.tar.gz' 'https://github.com/tcltk/tcl/archive/refs/tags/core-8-6-14.tar.gz' '186a75c524ad20ef0cbcf159253bfcb0510b3b2e605c05d2d7abe93351fa5636' 'tcl-core-8-6-14'
$common = @('-G','Visual Studio 17 2022','-A','x64',"-DCMAKE_INSTALL_PREFIX=$prefix",'-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded')
if (!(Test-Path -LiteralPath (Join-Path $prefix 'lib/zlibstatic.lib'))) {
    Run-CMake (@('-S',$zlib,'-B',"$BuildRoot/zlib",'-DZLIB_BUILD_EXAMPLES=OFF') + $common)
    Run-CMake @('--build',"$BuildRoot/zlib",'--config','Release','--target','INSTALL','--parallel')
}
if (!(Test-Path -LiteralPath (Join-Path $prefix 'lib/libhdf5.lib'))) {
    Run-CMake (@('-S',$hdf5,'-B',"$BuildRoot/hdf5",'-DBUILD_SHARED_LIBS=OFF','-DBUILD_TESTING=OFF','-DHDF5_BUILD_TESTING=OFF','-DHDF5_BUILD_TOOLS=OFF','-DHDF5_BUILD_EXAMPLES=OFF','-DHDF5_BUILD_HL_LIB=OFF','-DHDF5_BUILD_CPP_LIB=OFF','-DHDF5_ENABLE_Z_LIB_SUPPORT=ON','-DHDF5_ENABLE_SZIP_SUPPORT=OFF',"-DZLIB_ROOT=$prefix", "-DZLIB_LIBRARY=$prefix/lib/zlibstatic.lib") + $common)
    Run-CMake @('--build',"$BuildRoot/hdf5",'--config','Release','--target','INSTALL','--parallel')
}
Copy-Item -LiteralPath (Join-Path $hdf5 'COPYING') -Destination (Join-Path $repo 'third_party/licenses/HDF5.txt')
Copy-Item -LiteralPath (Join-Path $zlib 'LICENSE') -Destination (Join-Path $repo 'third_party/licenses/zlib.txt')
Run-CMake (@('-S',$repo,'-B',"$BuildRoot/plugin", "-DCMAKE_PREFIX_PATH=$prefix", "-DTCL_SOURCE_DIR=$tcl") + $common)
Run-CMake @('--build',"$BuildRoot/plugin",'--config','Release','--parallel')
if ($Package) {
    $stage = Join-Path $repo 'dist/vmd-cndb-0.2.1-windows-x64'
    Run-CMake @('--install',"$BuildRoot/plugin",'--config','Release','--prefix',$stage)
    Compress-Archive -Path "$stage/*" -DestinationPath "$stage.zip" -Force
    $checksum = (Get-FileHash -Algorithm SHA256 -LiteralPath "$stage.zip").Hash.ToLowerInvariant()
    "$checksum  vmd-cndb-0.2.1-windows-x64.zip" | Set-Content -LiteralPath "$stage.zip.sha256" -Encoding ascii
    Write-Output "Windows ZIP: $stage.zip ($checksum)"
}
