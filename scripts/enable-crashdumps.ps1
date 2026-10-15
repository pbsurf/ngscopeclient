# Enables Windows Error Reporting local crash dumps for ngscopeclient.exe.
# Run from an elevated PowerShell:  powershell -ExecutionPolicy Bypass -File scripts\enable-crashdumps.ps1
#   [-DumpFolder <folder>]   (default %LOCALAPPDATA%\CrashDumps of the user who runs ngscopeclient)
# Undo with:  Remove-Item 'HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\ngscopeclient.exe'
#
# CustomDumpFlags 0x1165 = DataSegs | HandleData | UnloadedModules | IndirectlyReferencedMemory |
# ProcessThreadData | ThreadInfo. That's a minidump plus the memory that pointers on the stack point to,
# which is enough to look at the object a crash was using without writing out the whole heap.

param([string]$DumpFolder = '%LOCALAPPDATA%\CrashDumps')

$k = 'HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\ngscopeclient.exe'
New-Item -Path $k -Force | Out-Null
New-ItemProperty -Path $k -Name DumpFolder -PropertyType ExpandString -Value $DumpFolder -Force | Out-Null
New-ItemProperty -Path $k -Name DumpCount -PropertyType DWord -Value 5 -Force | Out-Null
New-ItemProperty -Path $k -Name DumpType -PropertyType DWord -Value 0 -Force | Out-Null
New-ItemProperty -Path $k -Name CustomDumpFlags -PropertyType DWord -Value 0x1165 -Force | Out-Null
Get-ItemProperty $k | Format-List DumpFolder, DumpCount, DumpType, CustomDumpFlags
