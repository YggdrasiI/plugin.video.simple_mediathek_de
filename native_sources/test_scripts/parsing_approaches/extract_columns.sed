# Test script to extract data from the MediathekView dataset.
#
#
# Example usage:
# xz -d -c /dev/shm/Filmliste-akt.xz |sed -e "s/],\"/],\n\"/g" > /dev/shm/Filmliste-akt.json
# time sed -n -f extract_columns.sed  > /dev/shm/tmp.sed.json  < /dev/shm/Filmliste-akt.json
#
# Example input:
#
#"Filmliste":["Sender","Thema","Titel","Datum","Zeit","Dauer","Größe [MB]","Beschreibung","Url","Website","Url Untertitel","Url RTMP","Url Klein","Url RTMP Klein","Url HD","Url RTMP HD","DatumL","Url History","Geo","neu"],
#"X":["3Sat","3sat","37 Grad: Feierabend, Bauer!","16.10.2017","23:55:00","00:29:13","506","Ein Hightech-Bauernhof und ein Biohof stehen vor einem Generationswechsel. Wie kommen Altbauern und Nachfolger in der Übergangsphase miteinander zurecht? Heißt es plötzlich Alt gegen Neu?","http://nrodl.zdf.de/dach/3sat/17/10/171016_37grad_feierabend_bauer_online/3/171016_37grad_feierabend_bauer_online_2328k_p35v13.mp4","http://www.3sat.de/mediathek/?mode=play&obj=69413","","","114|476k_p9v13.mp4","","114|3328k_p36v13.mp4","","1508190900","","DE-AT-CH","false"],
#
/^"X"/ {
  s/^"X":\["\([^"]*\)","\([^"]*\)","\([^"]\+\)","\(.\{8,10\}\)","\(.\{8\}\)","\(.\{8\}\)",.*$/["\1"|\4||\5||\6|,"\3 \2"],/p
  # Date time conversion
# [Code missing...]
  # Duration conversion
# [Code missing...]
}

# Old syntax (with spaces) was:
#/^  "X"/ {
#  s/^  "X" : \[ "\([^"]*\)", "\([^"]*\)", "\([^"]\+\)", "\(.\{8,10\}\)", "\(.\{8\}\)", "\(.\{8\}\)",.*$/["\1"|\4||\5||\6|,"\3 \2"],/p
