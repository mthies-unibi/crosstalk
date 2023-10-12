# Major enhancements compared to the forked version 0.2

- Works on Raspberry Pi 400 (and possibly 4B 8 GB version).  
  Other Raspberry models should also work, but have not been tested.
- Updated to Circle library version 45.2
- Improved keyboard handling, including international keyboard layouts supported by Circle library. The layout is set by parameter `keymap=` in `cmdline.txt`.
- &lt;Ctrl> and &lt;Ctrl>+&lt;Shift> shortcuts from ST-80 should work, even on international keyboards.  
  For example:
  * &lt;Ctrl>-t/f/d to insert `ifTrue:`, `ifFalse:`, the current date
  * &lt;Ctrl>-(/'/"/[ to enclose the selected text in parentheses, single/double quotes, brackets
  * &lt;Ctrl>-b/B/i/I/u/U to turn on/off bold/italic/underline text styling
  * &lt;Ctrl>-&lt;*digit*> to switch fonts
  * ...
- Display resolution changed to 1280x720 to stay below the ST-80 object size limit for the display bitmap and still get a completely filled 16:9 HDMI screen.
- Fixes real-time-clock never incrementing its value when running on Raspberry Pi 4 and later. Now, the clock starts at midnight January 1, 1970 UTC when booting the Raspberry and increments every second, as intended.
- Somewhat experimental support for NTP syncing the time of the Raspberry, either once when ST-80 starts running (`ntp=0` in `cmdline.txt`) or every *N* minutes (`ntp=`*N* with *N*>0). This happens in the background to not increase the startup time. So `Date today` or `Time now` may report the start of the Unix epoch, if invoked very early, before the NTP sync has completed for the first time.
- Adapted the `Time class` method `currentTime: formatted` for Germany (with current DST rules). This required just a change to the method local variable `m570`, which encodes the time zone offset in hours and the starting day of the year for DST. Variable `m571`, which encodes the ending day of DST and the minutes part of the time zone offset, happened to be correct already, as given for California with the DST rules valid until 1986
- Fixes the `bitShift:` primitive to avoid lost bits for larger positive shift distances and to avoid C++ bit shifts with undefined behavior in the implementation.

# Original README for the forked version 0.2

Smalltalk-80 for Raspberry Pi version 0.2
This is a bare metal Smalltalk-80 port to the Raspberry Pi.

See docs/Changelog.txt for updates.

Based on the Smalltalk-80 C++ implementation by Dan Banay (https://github.com/dbanay/Smalltalk)
and the circle bare metal library by Rene Stange (https://github.com/rsta2/circle)

Tested hardware (known working):
- Raspberry Pi 1B and Zero W
- Raspberry Pi 2B V 1.1 (with BCM2836)
- Raspberry Pi 3B V 1.2
- Raspberry Pi 4B (4 GB version)

Tested hardware (NOT working):
- Raspberry Pi 4 (8 GB version)

Required additional hardware:
- HDMI screen with 1920x1080 resolution
- USB mouse/keyboard (USB hub via OTG on Raspberry Pi Zero works)

Building is tested on MacOS X 10.14 with arm-none-eabi toolchain installed via homebrew

To build, simply execute ./build.sh in the smalltalk-raspi-circle directory.
GNU make 4.0 or newer is required to build circle (install via homebrew).

This builds kernels for 
- Raspberry Pi 1B/Zero (kernel.img)
- Raspberry Pi 2B/3B   (kernel7.img)
- Raspberry Pi 4B      (kernel7l.img)

To use, copy the contents of the "sdboot" directory to the boot FAT partition of an SD card.

Enjoy!
-- Michael

