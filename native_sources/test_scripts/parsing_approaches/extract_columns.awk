# Test script to extract data from the MediathekView dataset.
#
# Call with '-b' if awk=gwak and source is utf-8 encoded.
# On Rpi is the BusyBox awk in use which not hat this option, 
# but ignore utf-8 encoding anyway.
#
# Notes:
#   - Some Fields needs to be merged because single char field separator ',' is not ideal
#     (, but faster?!)
# Required restrictions for LibreELEC on RPi:
#   - Busybox's 'date' only understands a few formats, i.e [YYYY.]MM.DD-hh:mm[:ss]
#   - Busybox's awk is also limited. There is no need to use --characters-as-bytes 
#
# Example usage:
#   xz -d -c /dev/shm/Filmliste-akt.xz | awk -b -f x.awk > /dev/shm/tmp.awk.json

BEGIN {
  # Do not use multiple chars to avoid usage of regex engine (effect not vertified.)
  FS = ","
  ORS = ""
  #used_fields[1] = "sender"
  #used_fields[2] = "thema"
  #used_fields[3] = "titel"
  #used_fields[4] = "datum"
  #used_fields[5] = "zeit"
  #used_fields[6] = "dauer"
  #for (i in used_fields) { print i, used_fields[i]}
  channels["\"\""] = 0
  now = systime()  # "date +%s"
  now = int(now / 86400) * 86400  # normalize
  print "{\"time_anchor\":" now ",\"entries\":["
  sep = ""
  }
BEGINFILE {
  globOffset = 0
}
END {
  # print "Senderliste: "
  sep = ""
  print "],\"sender\":{"
  for (s in channels) {
    if (length(s) > 1){
      # Key already nested by "
      printf("%s%s:%s", sep, s, channels[s]) 
      sep = ","
    }
  }
  print "}}"
}

!/^  "X" :/ { 
  globOffset += 1 + length($0) 
  next
}
{
  # Cut of '  "X" : [ '
  #$1 = substr($1, 11)
  sender = substr($1, 11)

  # Count number of joined fields to shift indizes of following fields...
  # Assume that only the title and thema field can contain extra ','
  # which spans string over multiple fields
  nfj = 0 
  curIdx = 2 + nfj
  thema = $curIdx
  curIdx += 1
  while (substr($(curIdx), 1, 2) != " \"" && curIdx < NF){
    thema = thema "," $curIdx
    nfj += 1
    curIdx += 1
  }
  thema = substr(thema, 3, length(thema) - 3)

  curIdx = 3 + nfj
  titel = $curIdx
  curIdx += 1
  while (substr($(curIdx), 1, 2) != " \"" && curIdx < NF){
    titel = titel "," $curIdx
    nfj += 1
    curIdx += 1
  }
  titel = substr(titel, 3, length(titel) - 3)

  datum = substr($(4+nfj), 1)
  zeit = $(5+nfj)
  dauer = $(6+nfj)

  # Convert date string
  # Assumed input formats: DD.MM.YYYY,  HH:MM:SS
  # Required: mktime("YYYY MM DD HH MM SS")
  split(datum, adatum, "[\".]")
  split(zeit, azeit, "[\":]")
  timestamp = sprintf("%i %i %i %i %i %i", adatum[4], adatum[3], adatum[2], azeit[2], azeit[3], azeit[4])
  timeunix = (now - mktime(timestamp)) / 60

  # Covert dauer into seconds
  split(dauer, adauer,"[\":]")
  sdauer = adauer[4] + 60 * adauer[3] + 3600 * adauer[2]

  if (!(sender in channels)) {
    channels[sender] = length(channels)
  }

  #print globOffset, sender
  #print titel, thema 
  #print datum, zeit, dauer
  #print timestamp, timeunix, sdauer

  # Json array
  printf("%s[%i,%i,%i,%i,\"%s %s\"]", sep, globOffset, channels[sender], timeunix, sdauer, titel, thema)

  # Eval byte position of next line
  # +1 for line break character
  # Note: utf-8 encoding of file should be ignored to get length in bytes.
  globOffset += 1 + length($0) 
  sep = ","
}
