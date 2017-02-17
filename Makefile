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
	git ls-files | grep -v "native_sources\/" | grep -v ".gitignore" \
		| sed -n -e "s/.*/$(PROJECT)\/\0/p" \
		> /dev/shm/$(PROJECT).include
	echo "$(PROJECT)/root/*" >> /dev/shm/$(PROJECT).include
	@# 2. Create archive
	cd .. ; zip --symlinks -r $(PROJECT).zip . \
		-i@/dev/shm/$(PROJECT).include

clean:
	test \! -f ../$(PROJECT).zip || rm ../$(PROJECT).zip
