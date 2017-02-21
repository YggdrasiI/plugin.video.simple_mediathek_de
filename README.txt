====  Kodi plugin for MediathekView ====

Dependencies:
  cmake, zip, gcc

Build:
  1. Clone this repository.
  2. Build native binary with 'make native'.
     This will also compile libbrotli (compression library)
  3. Build addon archive with 'make addon'

Installation: 
  Copy addon archive into .kodi/addons/. and refresh add-on list (or restart Kodi)
  
  I.e.
    cd $HOME/.kodi/addons
    [...]
    unzip plugin.video.simple_mediathek.zip
    kodi-send -a "UpdateLocalAddons"
	

Tested with:
  Kodi 17.0 / LibreELEC 7.95
  Kodi 16.1 / LibreELEC 7.0.2


Fanart-Icon:
	Icon is remixed image of the following image created
	by Rcdrun and will be distributed under the same license
	(Creative Commons Attribution-Share Alike 4.0 International license.)
	https://commons.wikimedia.org/wiki/File:Malachite_in_Geita,_Tanzania,_on_balcony.jpg
