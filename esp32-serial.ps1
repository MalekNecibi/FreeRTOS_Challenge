# Custom Patch Script to Upload binaries compiled with Arduino IDE to ESP32 via WSL

Start-Process -FilePath wsl.exe -ArgumentList "echo Prepping WSL for Upload ; sleep 10" -WindowStyle Minimized

# Prevent Arduino IDE Serial Monitor from blocking upload
taskkill /im serial-monitor.exe /f 2> $null
taskkill /im putty.exe /f 2> $null

# Generic Search for any USB UART Serial device
$esp32=(usbipd wsl list | ? {$_ -match 'CP210x'}) 2> $null
if ([string]::IsNullOrWhitespace($esp32)) {
	echo "ERROR: Serial Device (CP210x) not found"
	pause
	return
}

$esp32_busid=($esp32.split(' ')[0])

if ( $esp32.Contains("Not attached") ) {
	echo "Attaching to WSL"
	usbipd wsl attach -d Ubuntu --busid $esp32_busid
	Start-Sleep -s 1
}

# Wait for WSL to finish attaching device
(usbipd wsl list 2> $null | ? {$_ -match 'CP210x'})
if ((usbipd wsl list 2> $null | ? {$_ -match 'CP210x'}).Contains("Not attached") ) {
	echo ""
	echo "ERROR: Another process is using the Serial Device"
	pause
	return
}


echo "Locating IDE Build Directory"

# Find current Arduino IDE sketch directory
$sketch_dir=(Get-ChildItem -Path C:\Users\Malek\AppData\Local\Temp\arduino\sketches | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName + "/"

$sketch_name=((Get-ChildItem -Path $sketch_dir | Sort-Object -Property Length -Descending | Select-Object -First 1).Name.split('.')[0])


echo "Prepping WSL File Names"

$bootloader_path_win=$sketch_dir+$sketch_name+".ino.bootloader.bin"
$partitions_path_win=$sketch_dir+$sketch_name+".ino.partitions.bin"
$boot_path_win="C:\Users\Malek\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.11/tools/partitions/boot_app0.bin"
$bin_path_win=$sketch_dir+$sketch_name+".ino.bin"

$bootloader_path_wsl=(wsl wslpath -u ($bootloader_path_win.replace('\','/')))
$partitions_path_wsl=(wsl wslpath -u ($partitions_path_win.replace('\','/')))
$boot_path_wsl=(wsl wslpath -u ($boot_path_win.replace('\','/')))
$bin_path_wsl=(wsl wslpath -u ($bin_path_win.replace('\','/')))


wsl ~/git_wsl/esptool_bin/esptool --chip esp32 --port /dev/ttyUSB0 --baud 921600  --before default_reset --after hard_reset write_flash  -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x1000 $bootloader_path_wsl 0x8000 $partitions_path_wsl 0xe000 $boot_path_wsl 0x10000 $bin_path_wsl

# Detach to Arduino IDE can serial
usbipd wsl detach -b $esp32_busid

# pause
return