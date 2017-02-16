#!/usr/bin/python
# -*- coding: utf-8 -*-

# Example usage:
#  xz -d -c /dev/shm/Filmliste-akt.xz > /dev/shm/filme.json
#  python extract_columns.py

import json
import time
import datetime
# import io
# import pdb


class FilmlisteParser:
    entries = []
    metadata = {}
    encoding = "utf-8"

    # Note: epoch not used for time diff, but
    # an date nearby the used one. This shortens
    # the cache file.
    epoch = datetime.datetime.utcfromtimestamp(3600)

    def __init__(self):
        self.channels = {"": 0}
        self.time_anchor = None

        # Normalize modulo day (optional)
        now = datetime.datetime.today()
        midnight = (int((now-self.epoch).total_seconds()) / 86400) * 86400
        self.time_anchor = datetime.datetime.fromtimestamp(midnight)

        midnightB = (int(time.time()) / 86400) * 86400
        print("Timestamp test... ",
              midnightB, int((self.time_anchor - self.epoch).total_seconds()))

    def parse_dumb(self, filename_full, filename_part=None):
        self.entries = []  # Clear old list
        self.channels = {"": 0}
        """
            Parsing linewise reduces the memory footprint, but
            assumes more structure.

            Assumed format of data:
            - Two headlines with key "Filmliste"
            - N-1 lines with key "X". Each separated with ","
            - 1 line with key "X", but no comma (last entry)
        """
        with open(filename_full) as fin:
        #with io.open(filename_full, "r", encoding="utf8") as fin: # lame!!
            line_beginning = fin.tell()
            data = fin.readline()  # undecoded utf-8 string
            while data:
                # Key : [Values] split
                if data.find(":") > -1:
                    try:
                        jline = "{{ {0} }}".format(
                            data[:-2] if data[-2] == "," else data)
                        # print(jline)
                        # entry = json.loads(jline)#, encoding=self.encoding)
                        entry = json.loads(jline)  # Utf-8 will be decoded
                        if "X" in entry:
                            self.handle_key_x(entry["X"], line_beginning)
                        elif "Filmliste" in entry:
                            self.handle_key_filmliste(entry["Filmliste"])
                    except ValueError as e:
                        print(e)

                # Fetch next line
                line_beginning = fin.tell()
                data = fin.readline()  # undecoded utf-8 string

            print("Readed lines: {0}".format(len(self.entries)))

    def parse_json(self, filename_full, filename_part=None):
        # TODO. (Less code, but consumes more memory)
        def process_pairs(obj):
            print(len(obj))
            return None

        with open(filename_full) as fin:
            json.load(fin, object_pairs_hook=process_pairs)

    """ Assumed raw input structure:
        "Filmliste" : [ "14.12.2016, 23:23", "14.12.2016, 22:23", "3",
            "MSearch [Vers.: 2.1.0]", "4d511865fac2da58fcd1a6a3e0100735" ],
        "Filmliste" : [ "Sender", "Thema", "Titel", "Datum", "Zeit",
            "Dauer", "Größe [MB]", "Beschreibung", "Url", "Website",
            "Url Untertitel", "Url RTMP", "Url Klein", "Url RTMP Klein",
            "Url HD", "Url RTMP HD", "DatumL", "Url History", "Geo", "neu" ],
    """
    def handle_key_filmliste(self, entry):
        if "hash" in self.metadata:
            # Second line with this key
            self.metadata["labels"] = entry
            self.metadata["used_label_indizes"] = {
                "sender": self.metadata["labels"].index("Sender"),
                "datum": self.metadata["labels"].index("Datum"),
                "zeit": self.metadata["labels"].index("Zeit"),
                "dauer": self.metadata["labels"].index("Dauer"),
                "thema": self.metadata["labels"].index("Thema"),
                "titel": self.metadata["labels"].index("Titel")
            }
        else:
            # First line with this key
            self.metadata["dateA"] = entry[0]
            self.metadata["dateB"] = entry[1]
            self.metadata["?"] = entry[2]
            self.metadata["msearch_version"] = entry[3]
            self.metadata["hash"] = entry[4]

    """ Assumed raw input structure:
        "X" : [ "3Sat", "3sat", "Goldkinder", "13.12.2016", "23:10:00",
        "00:44:14", "764", "[Desc]", "[url], "[website]", "", "", "", "",
        "88|3328k_p36v13.mp4", "", "1481667000", "", "DE-AT-CH", "true" ],
    """
    def handle_key_x(self, entry, seek_position=-1):
        if "used_label_indizes" in self.metadata:
            filtered_entry = [seek_position]  # Seek position in source data
            # filtered_entry.extend(
            #    [entry["X"][i] for i
            #       in self.metadata["used_label_indizes"].values()])
            filtered_entry.append(self.get_sender_id(entry))
            filtered_entry.append(self.get_start_time(entry))
            filtered_entry.append(self.get_dauer_time(entry))
            filtered_entry.append(self.get_titel(entry))
            self.entries.append(filtered_entry)
            # return [filtered_entry]

        # return []

    def get_sender_id(self, entry):
        sender_str = entry[self.metadata["used_label_indizes"]["sender"]]
        if sender_str not in self.channels:
            self.channels[sender_str] = len(self.channels)
        return self.channels[sender_str]

    def get_start_time(self, entry):
        datum_str = entry[self.metadata["used_label_indizes"]["datum"]]
        zeit_str = entry[self.metadata["used_label_indizes"]["zeit"]]
        datum_list = datum_str.split(".")
        zeit_list = zeit_str.split(":")
        # Sometimes (i.e. livestreams), the string values could be empty.
        # Replace them with dummies.
        if len(datum_list) < 3:
            datum_list = "1.1.1970".split(".")
        if len(zeit_list) < 3:
            zeit_list = "00:00:00".split(":")

        # print(datum_str + "|" + zeit_str)
        date = [int(d) for d in datum_list]
        time = [int(z) for z in zeit_list]
        dt = datetime.datetime(date[2], date[1], date[0],
                               time[0], time[1], time[2])

        return self.get_time_diff(dt)

    def get_dauer_time(self, entry):
        dauer_str = entry[self.metadata["used_label_indizes"]["dauer"]]
        dauer_list = dauer_str.split(":")
        # Livestreams had no duration. Use dummy value
        if len(dauer_list) < 3:
            dauer_list = "00:01:00".split(":")
        dauer = [int(d) for d in dauer_list]
        return (dauer[-1] + dauer[-2] * 60 + dauer[-3] * 3600)

    def get_titel(self, entry):
        titel = entry[self.metadata["used_label_indizes"]["titel"]]
        thema = entry[self.metadata["used_label_indizes"]["thema"]]

        # Encoding required to avoid '\u[...]' description in json string.
        return u"{0} {1}".format(titel, thema).encode("utf-8")  # Approx. 50% of data
        # return u"{0} {1}".format("", "")

    def get_time_diff(self, dt):
        " Get distance to time anchor in minutes. "
        if not self.time_anchor:
            self.time_anchor = dt
        return int((self.time_anchor - dt).total_seconds() / 60)

    def load(self, filename):
        pass

    def save(self, filename):
        with open(filename, "w") as fout:
        # with io.open(filename, "w", encoding="utf8") as fout:
            json.dump({"sender": self.channels,
                       "time_anchor": int((self.time_anchor
                                           - self.epoch).total_seconds()),
                       "entries": self.entries}, fout,
                      separators=(',', ':'),
                      ensure_ascii = False,
                      sort_keys=False)
            fout.close()

    """
    with open(filename) as fin:
        fin.seek(start_index)
        data = fin.read(end_index - start_index)
    """

p = FilmlisteParser()
# p.parse_dumb("../test.json")
p.parse_dumb("/dev/shm/filme.json")
p.save("/dev/shm/tmp.py.json")
