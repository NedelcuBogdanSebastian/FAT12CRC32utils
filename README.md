# FAT12CRC32utils


## The CRC32ToFile utility (the IDE is Embarcadero_Dev-Cpp_6.3_TDM-GCC 9.2_Setup )

Drag on top of `CRC32ToFile.exe` any file and it will compute the CRC32 of the file
and append the 8 digit hex of crc to the end of the file.
We will use this feature to test files integrity from the 25Q32 flash memory.


## The readFAT12 utility (the IDE is Embarcadero_Dev-Cpp_6.3_TDM-GCC 9.2_Setup )

This was work done to test FAT12 functions previously of implementing them in the 
microcontroller. 
Drag on top of the `readFAT12.exe`, one of the image files 25Q32FLASH,..., and you
will see the FAT12 table information, the file list from the image name, size and 
location, and also it will load and display a file and print each step of the cluster
parsing, the debug information of internall working.

## The FileSystemAnalyzer, HxD64, formatx, win32diskimager

- `FileSystemAnalyzer` free utility to see the raw data from disks
- `HxD64` hex utility to save disk content image, edit files, and much more
- `formatx` Windows format renamed (usage formatx.exe F: /FS:FAT /V:FLASH /Q /X)
- `win32diskimager` Windows tool for writing/reading images to USB,SD, and much more


