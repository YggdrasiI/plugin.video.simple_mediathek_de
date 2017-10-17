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

addon: clean
	@# 1. Create list of addon files. Filter out uncommited data
	@# and native_sources folder, but add root folder.
	git ls-files | grep -v "\(native_sources\/\|[.]gitignore\|Makefile\)" \
		| sed -n -e "s/.*/$(PROJECT)\/\0/p" \
		> /dev/shm/$(PROJECT).include
	echo "$(PROJECT)/root/*" >> /dev/shm/$(PROJECT).include
	@# 2. Create archive
	# Note zip's --symlinks-flag produces non-installable archives for Kodi.
	cd .. ; zip -r $(PROJECT).zip . \
		-i@/dev/shm/$(PROJECT).include

clean:
	test \! -f ../$(PROJECT).zip || mv ../$(PROJECT).zip ../$(PROJECT).old.zip


update:
	cp addon.py $(HOME)/.kodi/addons/plugin.video.simple_mediathek_de/.
