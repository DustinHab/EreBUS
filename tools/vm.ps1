# vm.ps1 -- the live machine, in a native window on the desktop.
#
# Runs the Windows build of QEMU rather than the one inside WSL: WSLg
# opens windows for Linux programs and then, some days, never paints
# into them. A native window has no such moods.
#
# One instance at a time: a new start replaces the old. The store is
# kept unless "fresh" is asked for, so the machine that appears is the
# machine that was left.
#
#   powershell -File tools\vm.ps1          start with the existing store
#   powershell -File tools\vm.ps1 fresh    start with an empty store

param([string]$mode = "")

$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$qemu  = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$code  = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
$vars  = "C:\Program Files\qemu\share\edk2-i386-vars.fd"

if (-not (Test-Path (Join-Path $build "esp.img"))) {
    Write-Error "build\esp.img is missing -- run make first"
    exit 1
}

Get-Process qemu-system-x86_64 -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$store = Join-Path $build "store.img"
if ($mode -eq "fresh" -and (Test-Path $store)) { Remove-Item $store -Force }
if (-not (Test-Path $store)) {
    $f = [System.IO.File]::Create($store)
    $f.SetLength(32MB)
    $f.Close()
}

# The firmware variable store starts fresh every run; stale boot
# entries in it boot into nothing.
Copy-Item $vars (Join-Path $build "win-vars.fd") -Force

# The exchange disk, when one lies next to the project: a FAT32 image
# whose files appear inside the system as "the disk". Make one with
# mkfs.vfat (or format a stick image) and drop it here.
$exchange = Join-Path $root "exchange.img"
$xchgArgs = @()
if (Test-Path $exchange) {
    $xchgArgs = @(
        "-drive", "id=xchg,file=$exchange,format=raw,if=none",
        "-device", "ide-hd,drive=xchg,bus=ide.2"
    )
}

& $qemu `
    -machine q35 -m 512M `
    -drive "if=pflash,format=raw,readonly=on,file=$code" `
    -drive "if=pflash,format=raw,file=$(Join-Path $build 'win-vars.fd')" `
    -drive "format=raw,file=$(Join-Path $build 'esp.img')" `
    -vga none -device "VGA,edid=on,xres=1280,yres=800" `
    -drive "id=store,file=$store,format=raw,if=none" `
    -device "ide-hd,drive=store,bus=ide.1" `
    @xchgArgs `
    -device "e1000,netdev=n0" `
    -netdev "user,id=n0,hostfwd=udp::7801-:7800,hostfwd=tcp::8080-:80,hostfwd=tcp::22222-:22" `
    -name "Erebus" `
    -serial "file:$(Join-Path $build 'vm-serial.log')"
