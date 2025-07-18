PCem v14 Linux supplement


You will need the following libraries :

SDL2
wxWidgets 3.x
OpenAL

and their dependencies.

On Debian/Ubuntu systems these can be installed with:

  sudo apt-get install build-essential libsdl2-dev libwxgtk3.2-dev libopenal-dev

Open a terminal window, navigate to the PCem directory then enter

./configure --enable-release
make

then ./pcem to run.

For a 32-bit build on a 64-bit system, install multilib development packages and use:

  make -f src/Makefile.linux32-wx-sdl2

The Linux version stores BIOS ROM images, configuration files, and other data in ~/.pcem

configure options are :
  --enable-release-build : Generate release build. Recommended for regular use.
  --enable-debug         : Compile with debugging enabled.
  --enable-networking    : Build with networking support.
  --enable-alsa          : Build with support for MIDI output through ALSA. Requires libasound.


The menu is a pop-up menu in the Linux port. Right-click on the main window when mouse is not
captured.

CD-ROM support currently only accesses /dev/cdrom. It has not been heavily tested.
