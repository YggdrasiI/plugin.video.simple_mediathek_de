# -*- coding: utf-8 -*-

import sys
import xbmc
import xbmcgui
import xbmcplugin
import xbmcaddon
import time
import datetime
import urllib
import urlparse
import os.path
# import xml.etree.ElementTree as ET
import json
import subprocess

import keyboard

"""
Kodi uses Python 2.7.1. It follows an incomplete list of changes
 if the addon need to changed onto Python 3.x
- viewkeys() -> keys()
- viewvalues() -> values()
- view/iteritems() -> items()
  …
"""

# If False, state_diff will be saved with setProperty.
save_state_in_url = False

# If True, duration menu is prepended by min/max-selection
duration_separate_minmax = False

# Caching leads shows old values in main menu
directory_cache = False

max_num_entries_per_page = 15
SKINS_LIST = ['skin.confluence', 'skin.estuary', 'skin.estuary.480']
SKINS_WIDE_LIST = ['skin.confluence.480']

base_url = sys.argv[0]

# (Extra empty entries for list[-1]-access )
search_ranges_str = {
    "duration": ["10 min", "30 min", "60 min", "1,5 h",
                 "2 h", ""],
    "direction": [u"Höchstens", u"Mindestens"],
    "direction_b": [u"Suche auf Mindestlänge umstellen...",
                    u"Suche auf Maximallänge umstellen..."],
    "time": ["0-10 Uhr", "10-16 Uhr", "16-20 Uhr", "20-24 Uhr", ""],
    "day": ["Heute und gestern", "2 Tage", "5 Tage", "7 Tage", "14 Tage",
            ""],  # "Kalender", ""],
    "day_range": ["Kalender"],
    "channel": ["Kanal %s"],
}
"""
Matching arguments for the search program.
search_ranges_str["duration"][i] corresponends with
[search_range["duration"][i], search_range["duration"][i+1] - 1]
"""
search_ranges = {
    "duration": [0, 600, 1800, 3600, 5400, 7200, -1],
    "time": [0, 36000, 57600, 72000, 86400, -1],
    "day": [0, 1, 2, 5, 7, 14, -1, ],  # -1],
}

# Wrapper to track if dict entry was changed


class Dict(dict):
    changed = False
    parent_Dict = None

    def __init__(self, *args, **kwargs):
        super(Dict, self).__init__(*args, **kwargs)
        # Replace added dicts (values only) by Dict, too.
        for (k, v) in self.viewitems():
            if isinstance(v, __builtins__.dict):
                self[k] = Dict(v)

    def __setitem__(self, item, value):
        # Assume that value has no parent_Dict...
        if isinstance(value, Dict):
            super(Dict, self).__setitem__(item, value)
            value.parent_Dict = self
            self.set_changed()
        elif isinstance(value, Dict):
            copy = Dict(value)
            super(Dict, self).__setitem__(item, copy)
            copy.parent_Dict = self
            self.set_changed()
        else:
            super(Dict, self).__setitem__(item, value)
            self.set_changed()

    def update(self, udict):
        super(Dict, self).update(udict)
        if len(udict) > 0:
            self.set_changed()

    def set_changed(self):
        self.changed = True
        if self.parent_Dict:
            self.parent_Dict.set_changed()


class SimpleMediathek:

    state = Dict()
    MAX_HIST_LEN = 20

    def __init__(self, state=None):
        if isinstance(state, __builtins__.dict):
            self.state.update(state)
        else:
            self.state.update(read_state_file())

        if "current_pattern" not in self.state:
            self.add_default_pattern()
        self.state.changed = False

    """ # self not useable here?!
    def __del__(self):
        # Store current state in file
        # if isinstance(self.state, Dict):
        if False:
            if self.state.changed:
                write_state_file(self.state)
        elif isinstance(self.state, __builtins__.dict):
            write_state_file(self.state)
    """

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        # Store current state in file
        if isinstance(self.state, Dict):
            if self.state.changed:
                write_state_file(self.state)
        elif isinstance(self.state, __builtins__.dict):
            write_state_file(self.state)

    def add_default_pattern(self):
        if "current_pattern" not in self.state:
            self.update_state({"current_pattern": {}}, False)

    def get_current_pattern(self):
        return self.state.get("current_pattern", {})

    def update_state(self, changes, b_update_changed_flag):
        flag_backup = self.state.changed
        if isinstance(changes, __builtins__.dict):
            self.state.update(changes)
            if not b_update_changed_flag:
                self.state.changed = flag_backup

    def update_channel_list(self):
        (exit_code, data) = call_binary(["--info"])
        if exit_code == 0:
            try:
                js = json.loads(data)
                if False:
                    # Just update channel list
                    self.update_state(
                        {"channels": js.get("channels", [])}, False)
                else:
                    # Note: This updates multiple keys!
                    self.update_state(js, True)
            except:
                # On error or without first update
                pass

            return True

        return False

    def get_last_update_time(self):
        cstr = self.state.get("listcreation", "")
        ctime = self.state.get("ilistcreation", -1)
        return (ctime, cstr)

    def is_index_outtated(self):
        ctime_index = self.state.get("icreation", -1)
        ctime_list = self.state.get("ilistcreation", -1)
        if ctime_index == -1 or ctime_list == -1:
            return True
        now = int(time.time())
        if (now - ctime_index) < 1 * 3600:
            return False
        if (now - ctime_list) > 3 * 3600:
            return True

        return False

    def try_diff_update(self):
        ctime_list = self.state.get("ilistcreation", -1)
        now = int(time.time())
        if (now - ctime_list) < 86400:
            return True
        return False

    def get_channel_list(self):
        # Note value is a dict, not a list
        return self.state.get("channels", {})

    def add_to_history(self, search_pattern):
        hist = self.state.setdefault("history", [])
        self.MAX_HIST_LEN = int(addon.getSetting("history_length"))
        # Remove previous position
        try:
            hist.remove(search_pattern)
            hist.remove(search_pattern)
        except:
            pass
        hist.insert(0, search_pattern)
        self.state.set_changed()
        if len(hist) > self.MAX_HIST_LEN:
            hist = hist[-self.MAX_HIST_LEN:]

    def get_history(self):
        # if "history" not in self.state:
        #     self.update_state({"history": []}, False)
        return self.state.get("history", [])

    def get_search_results(self):
        # Available after search, only.
        # Note: Could be outdated
        return self.state.get("latest_search", {})

    def get_pattern_title(self, pattern):
        return pattern.get("title", "")

    def get_pattern_date(self, pattern):
        if "iday_range" in pattern:
            days = pattern["iday_range"]
            date0 = datetime.date.fromtimestamp(min(days))
            date1 = datetime.date.fromtimestamp(max(days))
            day_range = "%s–%s" % (
                date0.strftime("%d.%b"),
                date1.strftime("%d.%b"),
            )
            return day_range

        return search_ranges_str["day"][pattern.get("iday", -1)]

    def get_pattern_time(self, pattern):
        return search_ranges_str["time"][pattern.get("itime", -1)]

    def get_pattern_duration(self, pattern):
        s_duration = search_ranges_str[
            "duration"][pattern.get("iduration", -1)]
        s_direction = search_ranges_str[
            "direction"][pattern.get("iduration_dir", 0)]
        if len(s_duration) == 0:
            return ""

        return u"%s %s" % (s_direction, s_duration)

    def get_pattern_channel(self, pattern):
        c = pattern.get("channel", None)  # str, not int
        channels = self.state.get("channels", {})
        if c in channels.viewvalues():
            return str(c)
        else:
            return ""
        # return channels.get(pattern.get("ichannel", -1), "").upper()

    def get_channel_number(self, channel_name):
        channels = self.state.get("channels", {})
        for (k, v) in channels.viewitems():
            if v == channel_name:
                return int(k)
        return -1

    def get_channel_index(self, channel_name):
        # Just differs from get_channel_number
        # if channels_dict.viewkeys() != [1, len(channel_dict)]
        channels = self.state.get("channels", {})
        try:
            return int(channels.viewvalues().index(channel_name))
        except ValueError:
            pass

        return -1

    def sprint_search_pattern(self, pattern):
        title = self.get_pattern_title(pattern)
        sday = self.get_pattern_date(pattern)
        sduration = self.get_pattern_duration(pattern)
        stime = self.get_pattern_time(pattern)
        channel = self.get_pattern_channel(pattern).upper()

        s = u"%s%s%s%s%s" % (
            (title + " | ") if len(title) else "",
            (sday + " | ") if len(sday) else "",
            (stime + ", ") if len(stime) else "",
            (sduration + " ") if len(sduration) else "",
            (u' (%s)' % (channel)) if len(channel) else "",
        )
        return s

    def create_search_params(self, pattern=None):
        if pattern is None:
            pattern = self.get_current_pattern()

        largs = ["--search"]
        if len(pattern.get("title", "")) > 0:
            largs.append("--title")
            largs.append("%s" % (pattern["title"],))

        chan_name = self.get_pattern_channel(pattern)
        if chan_name:
            largs.append("-C")
            largs.append("%s" % (chan_name,))

        if len(pattern.get("iday_range", [])) > 0:
            days = pattern.get("iday_range")
            date0 = datetime.date.fromtimestamp(min(days))
            date1 = datetime.date.fromtimestamp(max(days))
            today = datetime.date.today()
            largs.append("--dayMin")
            largs.append("%i" % ((today-date0).days,))
            largs.append("--dayMax")
            largs.append("%i" % ((today-date1).days,))
        elif pattern.get("iday", -1) > -1:
            # Here, iday is index of day number
            largs.append("--dayMax")
            largs.append("%i" % (search_ranges["day"][pattern["iday"]+1],))

        if pattern.get("iduration", -1) > -1:
            d = pattern.get("iduration", -1)
            """
            d0 = search_ranges["duration"][d]
            d1 = search_ranges["duration"][d+1]-1
            """
            d_dir = pattern.get("iduration_dir", 0)
            if d_dir:
                # [X-1, infty)
                d0 = search_ranges["duration"][d+1]-1
                d1 = search_ranges["duration"][-2]
            else:
                # [0, X]
                d0 = search_ranges["duration"][0]
                d1 = search_ranges["duration"][d+1]

            largs.append("--durationMin")
            largs.append("%i" % (d0,))
            largs.append("--durationMax")
            largs.append("%i" % (d1,))

        if pattern.get("itime", -1) > -1:
            d = pattern.get("itime", -1)
            d0 = search_ranges["time"][d]
            d1 = search_ranges["time"][d+1]
            largs.append("--beginMin")
            largs.append("%i" % (d0,))
            largs.append("--beginMax")
            largs.append("%i" % (d1,))

        return largs

    def update_db(self, bForceFullUpdate=False):
        largs = ["update"]
        diff = mediathek.try_diff_update() and not bForceFullUpdate
        if diff:
            largs.append("diff")

        starttime = datetime.datetime.today()
        (exit_code, data) = call_binary(largs)
        if exit_code == 0:
            endtime = datetime.datetime.today()
            js = json.loads(data)
            # Update channel list, etc.
            self.update_state(js, True)
            return (True, starttime, endtime, diff)

        return (False, None, None, diff)


def store_volatile_state(state):
    """ Save data as window property to
        unload the item url. This reduces the
        number of urls for the (virtually) same page.

        Thus, after the navigation chain [page A] => [page B] => [page A again]
        where page A is the main page of the addon,
        the back button leaves the plugin and go not back to 'page B'.
    """
    if save_state_in_url:
        return

    s_state = json.dumps(state)
    wirts_id = 10025  # WINDOW_VIDEO_NAV, xbmcgui.getCurrentWindowId()
    wirt = xbmcgui.Window(wirts_id)
    wirt.setProperty("simple_mediathek_de_state", s_state)


def load_volatile_state():
    wirts_id = 10025  # WINDOW_VIDEO_NAV, xbmcgui.getCurrentWindowId()
    wirt = xbmcgui.Window(wirts_id)
    s_state = str(wirt.getProperty("simple_mediathek_de_state"))
    if len(s_state) < 2:
        return {}
    return json.loads(s_state)


def build_url(query, state=None):
    if save_state_in_url and state:
        query.update({"state": state})

    query["prev_mode"] = str(mode)
    sj = {"j": json.dumps(query)}  # easier to decode
    return base_url + "?" + urllib.urlencode(sj)


def unpack_url(squery):
    dsj = urlparse.parse_qs(squery)
    sj = dsj.get("j", ["{}"])[0]
    return json.loads(sj)


def create_addon_data_folder(path):
    try:
        if not os.path.isdir(path):
            os.mkdir(path)
    except OSError:
        err = 'Can\'t create folder for addon data.'
        xbmcgui.Dialog().notification(
            addon_name, err,
            xbmcgui.NOTIFICATION_ERROR, 5000)


def get_history_file_path():
    """ Return (path, filename) """
    # .kodi/userdata/addon_data/[addon name]
    path = xbmc.translatePath(
        addon.getAddonInfo("profile")).decode("utf-8")
    name = "search_history.json"

    create_addon_data_folder(path)
    return (path, name)


def read_state_file():
    try:
        (path, name) = get_history_file_path()
        fin = open(os.path.join(path, name), "r")
        state = json.load(fin)
        fin.close()
    except IOError:
        state = {}
    except TypeError:
        state = {}

    return state


def write_state_file(state):
    if not isinstance(state, __builtins__.dict):
        return False

    try:
        (path, name) = get_history_file_path()
        fout = open(os.path.join(path, name), "w")
        json.dump(state, fout)
        fout.close()
    except IOError as e:
        raise e
        return False

    return True


def call_binary(largs):
    # Returns (exit_code, str(stdout))

    # Add folder for results
    (path, name) = get_history_file_path()
    largs.append("--folder")
    largs.append(path)

    path = addon.getAddonInfo("path").decode("utf-8")
    script = os.path.join(path, "root", "bin", "simple_mediathek")
    largs.insert(0, script)
    try:
        ret = 0
        p = subprocess.Popen(
            largs,
            bufsize=-1,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        (stdout, stderr) = p.communicate()
        # ret = p.wait()

        # str_stdout = stdout.read(-1)
        # stdout.close()
        str_stdout = str(stdout)

        # Debug, store stderr stuff
        import random
        x = int(10000*random.random())
        fout = file("/dev/shm/addon.stderr.%i" % (x,), "w")
        # fout.write(stderr.read(-1))  # .encode("utf-8"))
        fout.write(str(stderr))  # .encode("utf-8"))
        fout.write(str(stdout))  # .encode("utf-8"))
        fout.close()

        # stderr.close()
        # ret = p.wait()

        return (ret, str_stdout)
    except ValueError:
        pass

    return (-1, "")


def call_binary_v1(largs):
    import os
    import os.path
    # Returns (exit_code, str(stdout))

    # Add folder for results
    (path, name) = get_history_file_path()
    largs.append("--folder")
    largs.append(path)

    path = addon.getAddonInfo("path").decode("utf-8")
    script = os.path.join(path, "root", "bin", "simple_mediathek")
    largs.insert(0, script)
    try:
        (c_stdin, c_stdout, c_stderr) = os.popen3(" ".join(largs), "r")
        out = c_stdout.read()
        c_stdin.close()
        c_stdout.close()
        c_stderr.close()

        return (0, out)
    except ValueError:
        pass

    return (-1, "")


def is_searchable(pattern):
    i_crit_used = 0
    if len(pattern.get("title", "")) > 0:
        i_crit_used += 1

    if len(pattern.get("channel", "")) > 0:
        i_crit_used += 1

    if pattern.get("iduration", -1) > -1:
        i_crit_used += 1

    if(pattern.get("iday_range", -1) > -1
       or pattern.get("iday", -1) > -1):
        i_crit_used += 1

    if pattern.get("ibegin", -1) > -1:
        i_crit_used += 1

    return (i_crit_used > 0)


def gen_search_categories(mediathek):
    pattern = mediathek.get_current_pattern()
    last_update = mediathek.get_last_update_time()
    allow_update = mediathek.is_index_outtated()
    if last_update[0] == -1:
        update_str = ""
    else:
        update_str = "%s %s %s" % (
            "| Stand", last_update[1], ("" if allow_update else ""))

    categories = []
    categories.append({"name": "Sender",
                       "selection": mediathek.get_pattern_channel(
                           pattern).upper(),
                       "mode": "select_channel",
                       # "thumbnail": item.findtext("thumbnail"),
                       # "fanart": item.findtext("fanart"),
                       })
    categories.append({"name": "Datum",
                       "selection": mediathek.get_pattern_date(pattern),
                       "mode": "select_day",
                       })
    categories.append({"name": "Uhrzeit",
                       "selection": mediathek.get_pattern_time(pattern),
                       "mode": "select_time",
                       })
    if duration_separate_minmax:
        categories.append({"name": "Dauer",
                           "selection":
                           mediathek.get_pattern_duration(pattern),
                           "mode": "select_duration_dir",
                           })
    else:
        categories.append({"name": "Dauer",
                           "selection":
                           mediathek.get_pattern_duration(pattern),
                           "mode": "select_duration",
                           })
    categories.append({"name": "Titel/Thema",
                       "selection": mediathek.get_pattern_title(pattern),
                       "mode": "select_title",
                       "id": state_diff.get("input_request_id", 0) + 1
                       })
    categories.append({"name": "Suchmuster leeren",
                       "selection": "",
                       "mode": "clear_pattern",
                       })
#    categories.append({"name": "Suchmuster speichern",
#                       "selection": "",
#                       "mode": "main",
#                       })
    categories.append({"name": "Filmliste aktualisieren",
                       "selection": update_str,
                       "mode": "update_db",
                       # "IsPlayable": allow_update,
                       })
    categories.append({"name": "Vorherige Suchen",
                       "selection": "",
                       "mode": "show_history",
                       })

    return categories


def listing_add_list_names(listing, state, item, names, i_selected=-1):
    for i in xrange(len(names)-1):  # Skip "" at list end
        url = build_url({"mode": "update_pattern",
                         "item": item, "value": i}, state)
        li = xbmcgui.ListItem(names[i], iconImage="DefaultFolder.png")
        li.setProperty("IsPlayable", "true")
        is_folder = True
        if i == i_selected:
            li.setLabel2("(selected)")
            li.select(True)
        listing.append((url, li, is_folder))

# Like listing_add_list_names, but use keys of dict as values.


def listing_add_dict_names(listing, state, item, names, v_selected=None):
    # 0. Sort copy of keys and compare int value of
    # stringified channel number. (json drawback)
    def cmp_int(a, b):
        return (int(a) - int(b))

    keys = names.keys()
    keys.sort(cmp=cmp_int)

    # for (k, name) in names.viewitems():
    for k in keys:
        name = names[k]
        title = "%s %s" % (k, name.upper())
        if name == "":
            continue
        url = build_url({"mode": "update_pattern",
                         "item": item, "value": name}, state)
        li = xbmcgui.ListItem(title, iconImage="DefaultFolder.png")
        li.setProperty("IsPlayable", "true")
        is_folder = True
        if name == v_selected:
            li.setLabel2("(selected)")
            li.select(True)
        listing.append((url, li, is_folder))


def listing_add_min_max_entries(listing, state, mode,  i_selected=-1):
    for i in [0, 1]:
        name = search_ranges_str["direction"][i]
        url = build_url({"mode": mode, "dir": i}, state)
        li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
        li.setProperty("IsPlayable", "true")
        is_folder = True
        if i_selected == i:
            li.setLabel2("(selected)")
            li.select(True)
        listing.append((url, li, is_folder))


def listing_direction_toggle(listing, state, mode):
    i_other = 1 - mediathek.get_current_pattern().get("iduration_dir", 0)
    other_dir_name = search_ranges_str["direction_b"][i_other]
    url = build_url({"mode": mode, "dir": i_other}, state)
    li = xbmcgui.ListItem(other_dir_name, iconImage="DefaultFolder.png")
    li.setProperty("IsPlayable", "true")
    is_folder = True
    listing.append((url, li, is_folder))


def listing_add_calendar_entry(listing, state, b_selected=False):
    name = search_ranges_str["day_range"][0]
    url = build_url({"mode": "select_calendar"}, state)
    # "item": "iday_range", "value": -1})
    li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
    li.setProperty("IsPlayable", "true")
    is_folder = True
    if b_selected:
        li.setLabel2("(selected)")
        li.select(True)
    listing.append((url, li, is_folder))


# Favorites had same structure as history entries, but extra name key.
def listing_add_history(listing, state):
    history = mediathek.get_history()
    for h in history:
        name = h.get("name", mediathek.sprint_search_pattern(h))
        url = build_url({"mode": "select_history",
                         "pattern": h}, state)
        li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
        li.setProperty("IsPlayable", "true")
        is_folder = True
        listing.append((url, li, is_folder))


def listing_add_remove_entry(listing, state, item):
    if item not in state.get("current_pattern", {}):
        return
    name = "Suchkriterum entfernen"
    url = build_url({"mode": "update_pattern",
                     "item": item}, state)
    li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
    li.setProperty("IsPlayable", "true")
    is_folder = True
    listing.append((url, li, is_folder))


def listing_add_back_entry(listing, state):
    name = "%-20s" % ("Zurück",)
    url = build_url({"mode": "main"}, state)
    li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
    li.setProperty("IsPlayable", "true")
    is_folder = True
    listing.append((url, li, is_folder))


def listing_add_search(listing, mediathek, state):
    pattern = mediathek.get_current_pattern()
    if is_searchable(pattern):
        name = "Suche starten"
        url = build_url({"mode": "start_search"}, state)
        li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
        li.setProperty("IsPlayable", "true")
        is_folder = True
        listing.append((url, li, is_folder))
    else:
        name = "Suche starten    [Nur mit Suchkriterium möglich]"
        url = build_url({"mode": "main"}, state)
        li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
        li.setProperty("IsPlayable", "false")
        is_folder = False
        listing.append((url, li, is_folder))


def listing_add_test(listing, state):
    # foo = xbmcgui.getCurrentWindowId()  # 10025
    # wirt = xbmcgui.Window(foo)
    # wirt.clearProperty("simple_mediathek_de_state")
    # wirt.setProperty("test", "gespeichert %i" % (foo,))
    # xbmc.executebuiltin("UpdateLocalAddons")
    """
    if mediathek.update_channel_list():
        xbmcgui.Dialog().notification(
            addon_name, "ok %i %i" % (
                len(mediathek.get_channel_list()),
                    mediathek.state.changed),
            xbmcgui.NOTIFICATION_ERROR, 5000)
    """

    xxx = mediathek.get_search_results().get("found", [])
    name = "%-20s %s %i" % ("Test", str(prev_mode), len(xxx))
    url = build_url({"mode": "update_db"}, state)
    li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
    li.setProperty("IsPlayable", "true")
    is_folder = True
    listing.append((url, li, is_folder))


def listing_add_search_results(listing, state, results):
    """ Example line:
        {"id": 154344, "title": "...",
                        "ibegin": 1486244700, "begin": "04. Feb. 2017 22:45",
                        "iduration": 816, "ichannel": 19,
                        "channel": "zdf", "anchor": 51792695},

    """
    i = 0
    for f in results:  # .get("found", []):
        name = "%s | %s | %s" % (
            f.get("channel").upper(),
            f.get("begin"),
            f.get("title", "???"),
        )

        url = build_url({"mode": "select_result", "iresult": i}, state)
        li = xbmcgui.ListItem(name, iconImage="DefaultVideo.png")
        li.setProperty("IsPlayable", "true")
        is_folder = True
        listing.append((url, li, is_folder))
        i += 1
        if i >= max_num_entries_per_page:
            break


def listing_search_page_link(listing, state, results, ipage):
    if ipage < 0:
        ipage = -ipage - 1
        name = "Vorherige Seite (%i)" % (ipage+1)
    else:
        name = "Nächste Seite (%i)" % (ipage+1)

    # pattern = mediathek.get_current_pattern()
    if True:  # is_searchable(pattern):
        url = build_url({"mode": "start_search",
                         "page": ipage}, state)
        li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
        li.setProperty("IsPlayable", "true")
        is_folder = True
        listing.append((url, li, is_folder))


def list_urls(state, urls, search_result):
    squality = [(u"Geringe Auflösung", u""),
                (u"Geringe Auflösung", u"  (RTMP)"),
                (u"Mittlere Auflösung", u""),
                (u"Mittlere Auflösung", u"  (RTMP)"),
                (u"Hohe Auflösung", u""),
                (u"Hohe Auflösung", u"  (RTMP)")]
    listing = []
    for i in xrange(6):
        if len(urls[i]) == 0:
            continue
        name = u"%-22s | %s... %s" % (
            squality[i][0], urls[i][:30], squality[i][1])
        """
        url = build_url({"mode": "play_url",
                         "video_url": urls[i]}, state)
        """
        url = urls[i]  # Direct url seems more stable
        li = xbmcgui.ListItem(name, iconImage="DefaultVideo.png")
        li.setProperty("IsPlayable", "true")
        li.setProperty("mimetype", "video")
        is_folder = False
        li.setProperty("title", search_result.get("title", ""))
        li.setProperty("duration",
                       str(int(search_result.get("iduration", 0)) / 60))
        listing.append((url, li, is_folder))

    # Entry to go back to list of search results. The state variable
    # still holds the array with the results.
    url = build_url({"mode": "show_search_result"}, state)
    li = xbmcgui.ListItem("Zurück", iconImage="DefaultFolder.png")
    li.setProperty("IsPlayable", "true")
    is_folder = True
    listing.append((url, li, is_folder))

    xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
    xbmcplugin.endOfDirectory(
        addon_handle, updateListing=sameFolder,
        cacheToDisc=directory_cache)


def play_url(addon_handle, state, url, b_add_to_history=False):
    # xbmc.executebuiltin('PlayMedia("%s")' % (url,))
    xbmc.Player().play(item=("%s" % (url,)))  # , listitem=listItem)
    """
    item = xbmcgui.ListItem(path=url)
    xbmcplugin.setResolvedUrl(handle=addon_handle,
                              succeeded=True, listitem=item)
    """


def blankScreen():
    """
    Black screen to reduce cpu usage (for RPi1).
    Usefulness depends on selected skin. In my default
    setting the cpu usage on RPI fall from 40% to 20%
    and the update time from 1m40 to 1m0.
    (Disabled because the 40% were caused by the rotating
    update icon and this would still be displayed..
    """
    # xbmc.executebuiltin("FullScreen")  # do not work
    pass


# ==============================================
# Main code

addon = xbmcaddon.Addon()
addon_name = addon.getAddonInfo("name")

with SimpleMediathek() as mediathek:
    state_diff = {}  # Unsaved changes

    # RunScript handling
    if sys.argv[1] == "update_db":
        mode = "background_script_call"
        xbmcgui.Dialog().notification(
            addon_name, "Update gestartet", xbmcgui.NOTIFICATION_INFO)
        ok, tdiff = False, 0
        try:
            (ok, start, end, diff) = mediathek.update_db(bForceFullUpdate=True)
            tdiff = (end-start).seconds
        except:
            ok = False

        if ok:
            xbmcgui.Dialog().notification(
                addon_name,
                "Update erfolgreich. Dauer %is" % (tdiff),
                xbmcgui.NOTIFICATION_INFO)
        else:
            xbmcgui.Dialog().notification(addon_name, "Update fehlgeschlagen",
                                          xbmcgui.NOTIFICATION_ERROR, 5000)

    elif sys.argv[1] == "delete_local_data":
        mode = "background_script_call"
        try:
            import os
            (path, name) = get_history_file_path()
            rm_files = []
            for root, dirs, files in os.walk(path):
                for f in files:
                    ext = f[f.rfind("."):]
                    if ext in [".json", ".br", ".index"]:
                        os.unlink(os.path.join(path, f))
                        rm_files.append(os.path.join(path, f))

                break  # Top dir only
        except IOError:
            pass

        xbmcgui.Dialog().notification(
            addon_name, "%i Dateien entfernt" % (len(rm_files)),
            xbmcgui.NOTIFICATION_INFO, 5000)

    elif sys.argv[1] == "XXX":
        mode = "not used"
    else:
        addon_handle = int(sys.argv[1])
        xbmcplugin.setContent(addon_handle, "movies")

    # Set default view
    skin_used = xbmc.getSkinDir()
    if skin_used in SKINS_LIST:
        xbmc.executebuiltin('Container.SetViewMode(50)')
    elif skin_used in SKINS_WIDE_LIST:
        xbmc.executebuiltin('Container.SetViewMode(51)')
    else:
        # View for other skins
        xbmc.executebuiltin('Container.SetViewMode(50)')

        # args = urlparse.parse_qs(sys.argv[2][1:])
        args = unpack_url(sys.argv[2][1:])
        mode = args.get("mode", None)
        prev_mode = args.get("prev_mode", "None")  # str
        sameFolder = (prev_mode != "None")  # do NOT use 'is not'

        # Update with unsaved changes, but not force
        # write of changes at end of script
        if save_state_in_url:
            state_diff = args.get("state", {})
        else:
            state_diff = load_volatile_state()

        mediathek.update_state(state_diff, False)

    # Handle modes
    if mode == "update_db":
        ok, tdiff = False, 0
        if True:  # or try...
            (ok, start, end, diff) = mediathek.update_db(
                bForceFullUpdate=False)
            tdiff = (end-start).seconds
        else:
            ok = False

        if ok:
            xbmcgui.Dialog().notification(
                addon_name,
                "Update erfolgreich. Dauer %is" % (tdiff),
                xbmcgui.NOTIFICATION_INFO)
        else:
            xbmcgui.Dialog().notification(addon_name, "Update fehlgeschlagen",
                                          xbmcgui.NOTIFICATION_ERROR, 5000)

    if mode == "select_calendar":
        # Not implemented
        mode = "main"

    if mode == "select_day":
        names = search_ranges_str["day"]
        i_sel = mediathek.get_current_pattern().get("iday", -1)
        listing = []
        listing_add_list_names(listing, state_diff, "iday", names, i_sel)
        listing_add_calendar_entry(listing, state_diff, i_sel == len(names))
        listing_add_remove_entry(listing, mediathek.state, "iday")
        listing_add_back_entry(listing, state_diff)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=sameFolder,
            cacheToDisc=directory_cache)

    if mode == "select_time":
        names = search_ranges_str["time"]
        i_sel = mediathek.get_current_pattern().get("itime", -1)
        listing = []
        listing_add_list_names(listing, state_diff, "itime", names, i_sel)
        listing_add_remove_entry(listing, mediathek.state, "itime")
        listing_add_back_entry(listing, state_diff)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=sameFolder,
            cacheToDisc=directory_cache)

    if mode == "select_duration_dir":
        listing = []
        i_sel = mediathek.get_current_pattern().get("iduration_dir", -1)
        listing_add_min_max_entries(listing, state_diff,
                                    "select_duration", i_sel)
        # listing_add_back_entry(listing, state_diff)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=sameFolder,
            cacheToDisc=directory_cache)

    if mode == "select_duration":
        # 0. Handle return value of select_duration_dir step
        if "dir" in args:
            "iduration_dir"
            pattern = mediathek.get_current_pattern()
            pattern.update({"iduration_dir": args["dir"]})
            changes = {"current_pattern": pattern}
            mediathek.update_state(changes, False)  # Unwritten update
            state_diff.update(changes)  # propagated by Uri

        # 1. Generate list
        names = search_ranges_str["duration"]
        i_sel = mediathek.get_current_pattern().get("iduration", -1)
        listing = []
        listing_add_list_names(listing, state_diff, "iduration", names, i_sel)
        listing_add_remove_entry(listing, mediathek.state, "iduration")

        # 0b. Add toggle for direction flag
        if not duration_separate_minmax:
            listing_direction_toggle(listing, state_diff, "select_duration")

        listing_add_back_entry(listing, state_diff)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=sameFolder,
            cacheToDisc=directory_cache)

    if mode == "select_channel":
        channels = mediathek.get_channel_list()
        current_pattern = mediathek.get_current_pattern()
        listing = []
        listing_add_dict_names(listing, state_diff, "channel", channels,
                               current_pattern.get("channel", ""))
        listing_add_remove_entry(listing, mediathek.state, "channel")
        listing_add_back_entry(listing, state_diff)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=sameFolder,
            cacheToDisc=directory_cache)

    if mode == "select_history":
        if "pattern" in args:
            new_pattern = args.get("pattern")
            # Clean up unset values. Note that this
            # made the state writing mandatory to prevent mixing of
            # old (readed state) and new (state_diff) pattern.
            new_pattern = {k: v for k, v in new_pattern.viewitems()
                           if v != -1 and v != ""}
            changes = {"current_pattern": new_pattern}
            mediathek.update_state(changes, True)  # Write update
            state_diff = {}

        # Value applied. Go back and show main menu
        mode = "main"

    if mode == "select_title":
        # Check if this url/mode was open again after leaving the
        # plugin. If yes, go to main page.
        last = state_diff.setdefault("input_request_id", -1)
        arg_id = int(args.get("id", 0))
        if arg_id == last:
            mode = "main"
        else:
            pattern = mediathek.get_current_pattern()
            result, query = keyboard.keyboard_input(
                "Titel / Thema (No RegEx)",
                mediathek.get_pattern_title(pattern)
            )
            if result:
                pattern["title"] = query
                changes = {"current_pattern": pattern}
                mediathek.update_state(changes, False)  # Unwritten update
                state_diff.update(changes)  # propagated by Uri

            state_diff["input_request_id"] = arg_id
            mode = "main"

    if mode == "select_result":
        results = mediathek.get_search_results().get("found", [])
        # if True:
        try:
            result = results[args.get("iresult", -1)]
            # "Nachfragen", "Beste verfügbare", "Geringste verfügbare",
            # "Niedrig", "Mittel", "Hoch"
            quali = int(addon.getSetting("video_quality"))

            # Fetch urls
            s_anchor = str(result["anchor"])
            url_args = ["--payload", s_anchor]
            (exit_code, data) = call_binary(url_args)
            if exit_code == 0:
                js = json.loads(data)
                urls = js.get(s_anchor, [])
                urls.extend(["", "", "", "", "", ""])  # len(urls) >= 6
                # Resort by video quality, 2xmid, 2xlow, 2xhigh
                qualities = [2, 3, 0, 1, 4, 5]
                urls = [urls[q] for q in qualities]

                non_empty_urls = [u for u in urls if len(u) > 0]
                if len(non_empty_urls) == 0:
                    xbmcgui.Dialog().notification(
                        addon_name, "Keine URL gefunden",
                        xbmcgui.NOTIFICATION_ERROR, 5000)
                else:
                    if quali == 0:
                        # Show list
                        list_urls(state_diff, urls, result)
                    elif quali in [1, 2]:
                        url = non_empty_urls[1-quali]  # [0] or [-1]
                        # Play file
                        play_url(addon_handle, state_diff, url)
                    else:
                        # Reduce on urls of quality low, mid or high
                        urls2 = urls[2*(quali-3): 2*(quali-3)+1]
                        non_empty2 = [u for u in urls2 if len(u) > 0]
                        if len(non_empty2) > 0:
                            # Play file
                            play_url(addon_handle, state_diff, non_empty2[0])
                        else:
                            # Show list
                            list_urls(state_diff, urls, result)

        except IndexError:
            # results incomplete...
            mode = "main"
        except KeyError:
            # Args incomplete...
            mode = "main"
        # else:
        #     mode = "main"

    if mode == "update_pattern":
        if "item" in args:
            pattern = mediathek.get_current_pattern()
            if "value" in args:
                try:
                    value = int(args["value"])
                except ValueError:
                    value = args["value"]

                pattern.update({args["item"]: value})
            elif args["item"] in pattern:
                pattern.pop(args["item"])

            changes = {"current_pattern": pattern}
            mediathek.update_state(changes, False)  # Unwritten update
            state_diff.update(changes)  # propagated by Uri

        # Value applied. Go back and show main menu
        mode = "main"

    if mode == "clear_pattern":
        pattern = mediathek.get_current_pattern()
        new_pattern = {"iduration_dir": pattern.get("iduration_dir", 0)}
        changes = {"current_pattern": new_pattern}
        mediathek.update_state(changes, False)  # Unwritten update
        state_diff.update(changes)  # propagated by Uri
        mode = "main"

    if mode == "start_search":
        pattern = mediathek.get_current_pattern()
        search_args = mediathek.create_search_params(pattern)

        # Add page flag
        page = "%u,%u" % (max_num_entries_per_page + 1,
                          args.get("page", 0)*max_num_entries_per_page)
        search_args.append("--num")
        search_args.append(page)

        results = {"pattern": pattern, "found": []}
        (exit_code, data) = call_binary(search_args)
        if exit_code == 0:
            js = json.loads(data)
            results.update(js)
            # TODO: Remove .replace()
            mediathek.add_to_history(pattern)  # TODO: testing, remove later
            # mode = "show_search_result"
        else:
            xbmcgui.Dialog().notification(addon_name, "Suche fehlgeschlagen",
                                          xbmcgui.NOTIFICATION_ERROR, 5000)
            # mode = "main"

        mode = "show_search_result"
        state_diff["latest_search"] = results

    if mode == "show_search_result":
        if "latest_search" in state_diff:
            results = state_diff["latest_search"].get("found", [])
            listing = []
            prev_page = args.get("page", 0) - 1
            next_page = (prev_page + 2) if (
                len(results) > max_num_entries_per_page) else -1

            if prev_page > -1:
                listing_search_page_link(listing, state_diff,
                                         results, -prev_page-1)

            listing_add_search_results(listing, state_diff, results)
            if next_page > -1:
                listing_search_page_link(listing, state_diff,
                                         results, next_page)

            # listing_add_back_entry(listing, state_diff)
            xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
            if False:
                # Had strange effects...
                xbmcplugin.endOfDirectory(
                    addon_handle, updateListing=(prev_page >= 0),
                    cacheToDisc=directory_cache)
            else:
                xbmcplugin.endOfDirectory(
                    addon_handle, updateListing=sameFolder,
                    cacheToDisc=directory_cache)
        else:
            mode = "main"

    if mode == "show_history":
        listing = []
        listing_add_history(listing, state_diff)
        listing_add_back_entry(listing, state_diff)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(addon_handle, updateListing=sameFolder)

    if mode == "play_url":
        if "video_url" in args:
            play_url(addon_handle, state_diff, args["video_url"])

    if mode is None or mode == "main":
        #  Top level page of plugin
        search_categories = gen_search_categories(mediathek)

        if mode is None and mediathek.is_index_outtated():
            # --info call to check if data was externally updated
            mediathek.update_channel_list()
            # To omit multiple calls of above line raise time stamp
            state_diff["icreation"] = int(time.time())
            # xbmcgui.Dialog().notification(
            #    addon_name, "Kanalliste aktualisiert %i" %(
            # len(mediathek.get_channel_list())),
            #    xbmcgui.NOTIFICATION_INFO, 5000)

        listing = []
        for cat in search_categories:
            name = "%-20s %s" % (
                cat.get("name", "?"), cat.get("selection", "!"))
            # name += str(build_url({}, state_diff))[45:]
            new_args = {"mode": cat.get("mode")}
            if "id" in cat:
                new_args["id"] = cat["id"]

            url = build_url(new_args, state_diff)
            li = xbmcgui.ListItem(name, iconImage="DefaultFolder.png")
            if cat.get("IsPlayable", True):
                li.setProperty("IsPlayable", "true")
                is_folder = True
            else:
                li.setProperty("IsPlayable", "false")
                is_folder = False

            listing.append((url, li, is_folder))

        # listing_add_test(listing, state_diff)
        listing_add_search(listing, mediathek, state_diff)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=sameFolder,
            cacheToDisc=directory_cache)

    if not save_state_in_url:
        store_volatile_state(state_diff)
