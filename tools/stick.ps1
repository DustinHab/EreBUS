# stick.ps1 -- writes build\stick.img onto a usb stick (run as administrator).
#   powershell -File tools\stick.ps1            lists the disks
#   powershell -File tools\stick.ps1 <number>   writes the stick
# - refuses non-usb disks, disks over 128 GB, and any answer but the disk number at the prompt
# - everything on the stick is lost

param([int]$Disk = -1)

$img = Join-Path $PSScriptRoot "..\build\stick.img"
if (-not (Test-Path $img)) { "no build\stick.img; run tools/mkusb.sh in wsl first"; exit 1 }

if ($Disk -lt 0) {
    Get-Disk | Sort-Object Number | ForEach-Object {
        "{0,3}  {1,-40} {2,8:N1} GB  {3}" -f $_.Number, $_.FriendlyName, ($_.Size / 1GB), $_.BusType
    }
    exit 0
}

$d = Get-Disk -Number $Disk -ErrorAction Stop
"disk $Disk`: $($d.FriendlyName), $([math]::Round($d.Size / 1GB, 1)) GB, on $($d.BusType)"
if ($d.BusType -ne 'USB') { "that is not on usb; refusing"; exit 1 }
if ($d.Size -gt 128GB) { "that is larger than a stick; refusing"; exit 1 }

$answer = Read-Host "everything on disk $Disk will be lost. type the disk number to go on"
if ($answer -ne "$Disk") { "nothing written"; exit 1 }

# The volumes have to go before the raw device takes writes.
Clear-Disk -Number $Disk -RemoveData -RemoveOEM -Confirm:$false -ErrorAction Stop
Start-Sleep -Seconds 2

$in = [System.IO.File]::OpenRead($img)
$out = New-Object System.IO.FileStream("\\.\PhysicalDrive$Disk", [System.IO.FileMode]::Open, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
$buf = New-Object byte[] 4MB
$total = 0
while (($n = $in.Read($buf, 0, $buf.Length)) -gt 0) {
    $out.Write($buf, 0, $n)
    $total += $n
}
$out.Flush()
$out.Close()
$in.Close()
"written: $total bytes.  boot the machine from the stick with Secure Boot off."
