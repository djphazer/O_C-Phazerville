---
layout: default
---
# Backup / Restore

Backup/Restore is a utility that allows you to transfer complete sets of app and/or calibration data to and from your module. Possible uses include:

- Transferring app data to a new O_C module,
- Moving calibration to a replacement Teensy for the same O_C module,
- Changing settings between pieces during live shows,
- Intercontinental internet-based collaboration with other Hemisphere Suite users, or
- Simply preparing for inevitable disaster.

## Backing Up Your Module

Backup/Restore backs up data as it exists in the EEPROM. So if you want to take a backup of the O_C's current state, save the module's state by long-pressing the right button from the main menu.

Connect your computer or tablet to the module and enter the Backup/Restore app on your module. Turn either encoder to choose "Data" or "Calibration." Enable recording on your SysEx Librarian software, and then press the right encoder button ([BACKUP]). The size of the data dump will be 172 bytes for Calibration, and about 2.5K for Data.

## Restoring Your Module
Connect your computer or tablet to the module and enter the Backup/Restore app on your module. Press the left encoder button ([RESTORE]). Backup/Restore will indicate that it is listening for SysEx data. From your SysEx Librarian software, initiate the dump. The module's screen should indicate a progress bar. When the restore is successful, the module returns to the saved state.

**If you use Backup/Restore to restore calibration settings, you'll need to cycle the power on your module for the restored calibration settings to become active.**

*© 2018-2022, Jason Justian and Beige Maze Laboratories.*
