F=$$(pwd)
SHELL=/bin/bash
PROJECT=$(shell basename $(F))

main:
	@echo "'make native': Build cmdline tool"
	@echo "'make addon': Create addon archive at .."

native:
	cd native_sources; make debug && make install

rpi:
	echo "Cross compiling currently not work. Call this on Raspian only"
	#cd native_sources; make rpi && make install

addon:
	cd .. ; zip -r $(PROJECT).zip $(PROJECT) --exclude \*/.\* $(PROJECT)/native_sources/\*

clean:
	rm ../$(PROJECT).zip
