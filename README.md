# scsd (scope screen dumper)

## What is this?
This is a small and simple Linux only screen dumper for Rigol MSO5000 series (and possibly other) oscilloscopes connected via USB. It simply requests the current screen content as BMP or PNG from the device and saves it into a file. It is meant to be used from inside a terminal or called directly, maybe mapped to a hot key for maximum convenience? In contrast to a lot of other similar tools this code only depends on libc and (optionally) libnotify, ie it avoids the (sometimes bloated) third-party abstraction layers.

## Licence and disclaimer
This tool is licenced under AGPLv3+ and comes WITHOUT ANY WARRANTY! Please note that the command for PNG output is undocumented so in theory your (expensive) scope could explode. Use at your own risk.

## Why?
It all boils down to lazyness and convenience... Until recently i used a USB drive to get screen captures of my scope onto my computer. Slow, cumbersome, prone to errors (what was this `file42.bmp` exactly showing again?). I *finally* connected the scope directly to the computer via USB and was soon disappointed: The official software from Rigol needed to get screenshots from the MSO5000 series is Windows only, closed-source, has lots of dependencies (including the NET Framework) and is - in my opinion - totally bloated (*several hundreds of MB* just for the Rigol software). All this stuff may be useful for some people and certainly allows to do a lot more than taking screenshots, but all i wanted is a screenshot-tool that is
* small (including dependencies)
* available for Linux
* free as in freedom
* simple to use.

So i looked around and found this thing called USBTMC (USB Test & Measurement Class) that the scope advertises (try `lsusb -v` if you are curious) when connected as an USB device. The [specifications](https://www.usb.org/document-library/test-measurement-class-specification/) are available for free and so is the [Programming Guide](https://int.rigol.com/Images/MSO5000ProgrammingGuideEN_tcm7-4051.pdf) for the 5000 series from Rigol. Basically it's just an ASCII string to send with a bit of additional housekeeping. This must be doable without hundreds of MB of stuff? Yes, it is.  
On a sidenote, you can even skip the USBTMC driver from Linux and use `libusb` directly (don't forget to do `sudo modprobe -r usbtmc` or libusb will not be able to talk to your scope!), but it is really inconvenient (e.g. because of the `bTag` inside the header, the MSO5000 series is somewhat picky about this) and why bother when the [needed driver](https://github.com/torvalds/linux/blob/master/drivers/usb/class/usbtmc.c) is in mainline Kernel? In theory you can even `cat` from and to `/dev/usbtmc0` directly, but this does not work reliably and is certainly not suitable for getting big blobs of binary (image) data (i tried).

## How to compile
### Without notification support
As simple as `gcc -Wall -Wextra -Werror -DNO_LIBNOTIFY -o scsd main.c`.
### With notification support
The notifications can always be inhibited at runtime, see below.
#### Dependencies
You will need `libnotify`. On Debian try `sudo apt install libnotify-dev`. 
#### Compilation
Run `./make` (i really need to learn Makefiles...) which is just a wrapper around a somewhat longer gcc-invocation.

## How to use
As said this tool is made so it can run from a terminal or be called directly via a hot key. In the latter case it will use notifications (those small messages you see appearing on the top right of your screen sometimes) to tell the user what is going on. This behaviour can be disabled at runtime (or directly at compile-time) so the tool will be completly quiet.
### Arguments
#### --device [mandatory]
Usually this will be `/dev/usbtmc0` unless you have several USBTMC capable devices connected to your computer.
#### --folder
Where do you want your screenshot to be saved? If not provided, the file will be created where the executable is.
#### --filename
Filename for your screenshot, if not provided a sane default will be used.
#### --png
Ask the scope for PNG (much smaller file) instead of default BMP. This uses an **undocumented** command, so use at your own risk. Thanks to the people from the eevblog-forum where i found this!
#### --no-notif
Don't show any notification(s).

### Speed?
Fast:
```
$ time ./scsd --dev /dev/usbtmc0 
real    0m0,631s
user    0m0,003s
sys     0m0,049s
```
And even faster with PNG (less data to transfer):
```
$ time ./scsd --dev /dev/usbtmc0 --png

real    0m0,277s
user    0m0,001s
sys     0m0,005s
```
(YMMV of course depending on your hardware and so on)
