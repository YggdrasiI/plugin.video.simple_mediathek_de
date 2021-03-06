F=$$(pwd)
SHELL=/bin/bash
PROJECT=$(shell basename $(F))

main:
	@echo "'make native': Build cmdline tool"
	@echo "'make addon': Create addon archive at .."

native:
	cd native_sources; make && make install

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


# Kodi do not like symlinks
#brotli_resolve_symlinks:
#	$(foreach dir, $(shell ls -d root/*/lib),\
#		echo $(dir) && cd $(dir) && rename -v -f "s/.so.*$$/.so/" libbrotli*.so.1.* && cd - )

clean:
	test \! -f ../$(PROJECT).zip || mv ../$(PROJECT).zip ../$(PROJECT).old.zip

update:
	cp *.py $(HOME)/.kodi/addons/plugin.video.simple_mediathek_de/.

update_more:
	cp *.py *.xml $(HOME)/.kodi/addons/plugin.video.simple_mediathek_de/.
	cp -r resources/* $(HOME)/.kodi/addons/plugin.video.simple_mediathek_de/resources/.
# cp -r lib/* $(HOME)/.kodi/addons/plugin.video.simple_mediathek_de/lib/.

test_complete_build:
	cd /dev/shm \
		&& git clone "https://github.com/YggdrasiI/plugin.video.simple_mediathek_de.git" \
		&& cd plugin.video.simple_mediathek_de/native_sources \
		&& make && make tests && make install \
		&& cd .. && make addon
		@echo -e "Complete build successful.\n\tRemove /dev/shm/plugin.video.simple_mediathek_de manually"
