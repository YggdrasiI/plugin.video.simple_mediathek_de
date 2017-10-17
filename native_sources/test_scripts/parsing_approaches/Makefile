FOLDER=/dev/shm/
FILMLISTE=$(FOLDER)Filmliste-akt.json
OUTPUT_SED=$(FOLDER)tmp.sed.json
OUTPUT_AWK=$(FOLDER)tmp.awk.json
OUTPUT_PY=$(FOLDER)tmp.py.json
SCRIPT=../../simple_mediathek.local
FETCH=wget -qO-
UNPACK_XZ=xz -dc -
TIME_OUTPUT=/dev/shm/.time_stats
TIME=time -p -o "$(TIME_OUTPUT)"

main: note $(FILMLISTE) sed awk python


note:
	@echo "\tThe output formats of the scripts differs, but the time measurments\n\t"\
		"should still indicate which approach is faster."

$(FILMLISTE): download

download:
	LINK=$$($(SCRIPT) "link") ;\
	#echo $$LINK ;\
	$(FETCH) "$$LINK" | $(UNPACK_XZ) | sed -e "s/],\"/],\n\"/g" > "$(FILMLISTE)"

sed:
	@echo "Time consumption of sed script:"
	$(TIME)\
		sed -n -f extract_columns.sed \
		> "$(OUTPUT_SED)" < "$(FILMLISTE)"
	@cat "$(TIME_OUTPUT)"

awk:
	@echo "Time consumption of awk script:"
	$(TIME)\
		awk -b -f extract_columns.awk \
		> "$(OUTPUT_AWK)" < "$(FILMLISTE)"
	@cat "$(TIME_OUTPUT)"

python:
	@echo "Time consumption of python script:"
	$(TIME)\
		python extract_columns.py \
		> "$(OUTPUT_PY)" < "$(FILMLISTE)"
	@cat "$(TIME_OUTPUT)"
