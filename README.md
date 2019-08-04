# cc2640r2f-sbl-linux
A cc26x0 serial bootloader for linux

Usage: 
1. Convert the .hex to .bin using the hex2bin python application.
2. Ensure the bootloader is activated on the cc26x0.
3. Run: ./sbl_out portname binfile
4. Example: ./sbl_out /dev/ttyUSB0 firmware.bin 

Enjoy :)
