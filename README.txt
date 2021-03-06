====  Kodi plugin for MediathekView ====

Dependencies:
  cmake, zip, gcc

Build:
  1. Clone this repository.
  2. Build native binary with 'make native'.
     This will also compile libbrotli (compression library)
  3. Build addon archive with 'make addon'
     This should create 'plugin.video.simple_mediathek_de.zip'.

  A build version can also be found at
  https://forum.mediathekview.de/topic/193/mediatheken-plugin-für-kodi/1

Installation:
  Variant A). Open Addon-Browser in Kodi and select 'Install from zip file'.

    Note for  Kodi 17.0 (Krypton) and above:
      You had to allow addons from unknown sources in Preferences>System>Addons

  Variant B)
    Unzip the addon archive into '.kodi/addons/.' and refresh the add-on list (or restart Kodi)
    The position of the .kodi folder, default is $HOME/.kodi, depends on your system/setup.
    Note that $HOME is '/storage' on OpenELEC/LibreELEC and $HOME is '/home/osmc' on OSMC.

    I.e.
      cd $HOME/.kodi/addons
      [...]
      unzip plugin.video.simple_mediathek.zip
      kodi-send -a "UpdateLocalAddons"


Tested with:
  Kodi 17.5 / OSMC Oct-2017
  Kodi 17.1 / LibreELEC 8.0.1
  Kodi 17.0 / LibreELEC 7.95
  Kodi 16.1 / LibreELEC 7.0.2

Notes about usage with OSMC:
  Install the unpacking tool 'xz', if not installed (sudo apt-get install xz-utils).

Notes about usage on Raspberry Pi 1:
  The periodical redrawing of Kodi's GUI consumes a lot CPU cycles. Depending on
  the selected skin the idle consumption varies between 20%-40%. Scrolling text
  could raise this to 100%!
  This slows down the update process of this addon. You could raise the speed by
  enabling '(Experimental/For RPi 1) Addon pauses Kodi process'.
  It's marked as experimental because it had some side effects.

Fanart-Icon:
  Icon is remixed image of the following image created
  by Rcdrun and will be distributed under the same license
  (Creative Commons Attribution-Share Alike 4.0 International license.)
  https://commons.wikimedia.org/wiki/File:Malachite_in_Geita,_Tanzania,_on_balcony.jpg
