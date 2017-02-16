# Test script to extract data from the MediathekView dataset.
#
#
# Example usage:
# time xz -d -c /dev/shm/Filmliste-akt.xz | sed -n -f extract_columns.sed  > /dev/shm/tmp.sed.json
#
# Example input:
#  "Filmliste" : [ "Sender", "Thema", "Titel", "Datum", "Zeit", "Dauer", "Größe [MB]", "Beschreibung", "Url", "Website", "Untertitel", "UrlRTMP", "Url_Klein", "UrlRTMP_Klein", "Url_HD", "UrlRTMP_HD", "DatumL", "Url_History", "Geo", "neu" ],
#  "X" : [ "3Sat", "3sat", "Goldkinder", "13.12.2016", "23:10:00", "00:44:14", "764", "Gold ist allgegenwärtig: der Ring, die Zahnkrone oder die Währungsreserven von Staaten. Doch kaum jemand fragt nach, unter welchen Bedingungen das Luxusprodukt gewonnen wird. Oft geschieht das in...", "http://nrodl.zdf.de/dach/3sat/16/12/161213_goldkinder_online/3/161213_goldkinder_online_2328k_p35v13.mp4", "http://www.3sat.de/mediathek/?mode=play&obj=63486", "", "", "88|476k_p9v13.mp4", "", "88|3328k_p36v13.mp4", "", "1481667000", "", "DE-AT-CH", "true" ],
#
#
/^  "X"/ {
  s/^  "X" : \[ "\([^"]*\)", "\([^"]*\)", "\([^"]\+\)", "\(.\{8,10\}\)", "\(.\{8\}\)", "\(.\{8\}\)",.*$/["\1"|\4||\5||\6|,"\3 \2"],/p
  # Date time conversion
# [Code missing...]
  # Duration conversion
# [Code missing...]
}
