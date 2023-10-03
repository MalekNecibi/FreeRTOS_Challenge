#SingleInstance, Force
Menu Tray, Icon, %A_ScriptDir%\esp32-icon.jpg
SetTitleMatchMode, 2

; Keyboard shortcuts to trigger Arduino compile with WSL upload

VID_PID := "10c4:ea60"

; GoSub, ESP32_Upload
return

; First Connection requires Admin
<!`::
	Run, wsl sleep 10,, hide
	Run, taskkill /im serial-monitor.exe /f
	Run *RunAs C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe "usbipd wsl attach -d Ubuntu -i %VID_PID% `; usbipd wsl detach -i %VID_PID% `; pause"
	return

; Upload without compiling
>!Enter::
	Run, wsl sleep 10,, hide
	GoTo, ReUpload
	return

; Compile and Upload
>^Enter::
ESP32_Upload:
	; Compile in Arduino IDE
	If WinExist("ahk_exe Arduino IDE.exe") {
		WinActivate
		Send ^r

	} else {
		MsgBox ERROR: Failed to locate Arduino IDE
		return
	}

	Run, wsl sleep 20,, Hide

	; Wait for evidence Arduino is compiling (max 20 seconds)
	Loop 2000 {
		if (%A_Index% >= 1999) {
			MsgBox ERROR: Couldn't tell if/when compilation completed
			return
		}
		
		Process, Exist, gen_esp32part.exe
		if (ErrorLevel) {
			break
		}
		Sleep 10
	}
	; Run, wsl sleep 15,, Min
	
	; Wait for Arduino compilation to complete (max 5 seconds)
	Loop 500 {
		if (%A_Index% >= 499) {
			MsgBox ERROR: Couldn't tell if/when compilation completed
			return
		}
		Process, Exist, gen_esp32part.exe
		if (!ErrorLevel) {
			break
		}
		Sleep 10
	}

ReUpload:

	; Upload with WSL
	; Run, C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe "C:\Users\Malek\Desktop\esp32-serial.ps1",, Min
	Run, C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe "C:\Users\Malek\Desktop\esp32-serial.ps1"
	Sleep 1000
	if WinExist("ahk_exe powershell.exe") {
		WinWaitClose
	
	} else {
		return
	}

	; Return to Arduino
	If WinExist("ahk_exe Arduino IDE.exe") {
		WinActivate
		ToolTip Upload Complete
		SetTimer, RemoveToolTip, -1250
	}
	return





#If
`::ExitApp

RemoveToolTip:
Tooltip
return
