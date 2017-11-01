# -*- coding: utf-8 -*-

from __future__ import unicode_literals

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

sys.path.append(os.path.join(os.path.dirname(__file__), 'lib'))
import mediathekviewweb as MVWeb

import keyboard
from chmod_binaries import binary_setup

from constants import search_ranges_locale, search_ranges_str, search_ranges

# Old translation variant
addon = xbmcaddon.Addon()
getLocalizedString = addon.getLocalizedString

#def getLocalizedString(inum):
#	return unicode(inum)

"""
Kodi uses Python 2.7.1. It follows an incomplete list of changes
 if the addon need to changed onto Python 3.x
- viewkeys() -> keys()
- viewvalues() -> values()
- view/iteritems() -> items()
  …
"""

# If False, expanded_state will be saved with setProperty.
# False is recommended
save_state_in_url = False

"""
Combination of saved (mediathek.state) and unsaved changes.
Use this variable to read current values.

Updates of expanded_state will not be saved permanently. (Thus,
adding values does not force any writing to disk.)
"""
expanded_state = {}


# If True, duration menu is prepended by min/max-selection
duration_separate_minmax = False

# Caching leads shows old values in main menu
directory_cache = False

max_num_entries_per_page = 15
# SKINS_LIST = [u"skin.confluenceu", u"skin.estuary", u"skin.estuary.480"]
# SKINS_WIDE_LIST = [u"skin.confluence.480"]

base_url = sys.argv[0]


class Dict(dict):
    "Wrapper to track if dict entry was changed."
    changed = False
    parent_Dict = None

    def __init__(self, *args, **kwargs):
        super(Dict, self).__init__(*args, **kwargs)
        """
        # Replace added dicts (values only) by Dict, too.
        for (k, v) in self.viewitems():
            if isinstance(v, __builtins__.dict):
                self[k] = Dict(v)
        """

    def __setitem__(self, item, value):
        # Assume that value has no parent_Dict...
        if self.__contains__(item):
            self.changed = (value != self.__getitem__(item))
        else:
            self.changed = True

        super(Dict, self).__setitem__(item, value)
        """
        if isinstance(value, Dict):
            super(Dict, self).__setitem__(item, value)
            value.parent_Dict = self
            self.set_changed()
        elif isinstance(value, dict):
            copy = Dict(value)
            super(Dict, self).__setitem__(item, copy)
            copy.parent_Dict = self
            self.set_changed()
        else:
            super(Dict, self).__setitem__(item, value)
            self.set_changed()
        """

    """
    def update(self, udict):
        super(Dict, self).update(udict)
        if len(udict) > 0:
            self.set_changed()
    """

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

        if u"current_pattern" not in self.state:
            self.add_default_pattern()
        self.state.changed = False

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
        if u"current_pattern" not in self.state:
            self.update_state({u"current_pattern": {}}, False)

    def get_current_pattern(self):
        return self.state.get(u"current_pattern", {})

    def get_livestream_pattern(self):
        return {u"title": u"Livestream", u"args": [
            u"--durationMax", u"0",
            u"--beginMax", u"7260",  # 1. Jan 1970, 1 Uhr, Winterzeit.
        ]}

    def update_state(self, changes, b_update_changed_flag=False):
        flag_backup = self.state.changed
        if isinstance(changes, __builtins__.dict):
            self.state.update(changes)
            if not b_update_changed_flag:
                self.state.changed = flag_backup

            # Also update volatile values to prevent
            # overwrite with outdates values.
            expanded_state.update(changes)

    def is_mvw_outdated(self):
        ctime_wmv = self.state.get(u"ilast_mvw_update", -1)
        now = int(time.time())
        if (now - ctime_wmv) > 2 * 86400:
            return True
        return False

    def update_mvw(self):
        if 0 == len(self.get_channel_list()) and b_mvweb:
            # from channel_list import channels
            (exit_code, channels) = MVWeb.fetch_channel_list()
            self.update_state(channels, True)
            write_state_file(self.state)
            # xbmcgui.Dialog().notification(
            #    addon_name, u"Init channel list",
            #    xbmcgui.NOTIFICATION_INFO, 5000)
            return True

    def update_channel_list(self):
        (exit_code, data) = call_binary([u"--info"])
        if exit_code == 0:
            try:
                js = json.loads(data)
                self.update_state(js, True)
                # Note: Above line could updates more keys
                # as just channels. Alternative:
                # self.update_state(
                #    {u"channels": js.get(u"channels", [])}, False)
            except:
                # On error or without initial update
                pass

            return True
        return False

    def get_last_update_time(self):
        cstr = self.state.get(u"listcreation", u"")
        ctime = self.state.get(u"ilistcreation", -1)
        return (ctime, cstr)

    def is_index_outdated(self):
        ctime_index = self.state.get(u"icreation", -1)
        ctime_list = self.state.get(u"ilistcreation", -1)
        if ctime_index == -1 or ctime_list == -1:
            return True
        now = int(time.time())
        if (now - ctime_index) < 1 * 3600:
            return False
        if (now - ctime_list) > 3 * 3600:
            return True
        return False

    def try_diff_update(self):
        ctime_list = self.state.get(u"ilistcreation", -1)
        now = int(time.time())
        if (now - ctime_list) < 86400:
            return True
        return False

    def get_channel_list(self):
        # Note value is a dict, not a list
        return self.state.get(u"channels", {})

    def add_to_history(self, search_pattern):
        if b_livestream:
            return
        hist = self.state.setdefault(u"history", [])
        self.MAX_HIST_LEN = int(addon.getSetting(u"history_length"))
        # Remove previous position
        try:
            hist.remove(search_pattern)
            hist.remove(search_pattern)
        except:
            pass
        hist.insert(0, search_pattern)
        if len(hist) > self.MAX_HIST_LEN:
            hist = hist[-self.MAX_HIST_LEN:]
        self.state.set_changed()

    def get_history(self):
        # if u"history" not in self.state:
        #     self.update_state({u"history": []}, False)
        return self.state.get(u"history", [])

    def get_search_results(self):
        # Available after search, only.
        # Note: Could be outdated
        return self.state.get(u"latest_search", {})

    def get_pattern_title(self, pattern):
        return pattern.get(u"title", u"")

    def get_pattern_desc(self, pattern):
        return pattern.get(u"description", u"")

    def get_pattern_date(self, pattern):
        if u"iday_range" in pattern:
            days = pattern[u"iday_range"]
            date0 = datetime.date.fromtimestamp(min(days))
            date1 = datetime.date.fromtimestamp(max(days))
            if date1 != date0:
                day_range = u"%s–%s" % (
                    date0.strftime(u"%d.%b"),
                    date1.strftime(u"%d.%b"),
                )
            else:
                day_range = u"%s" % (date0.strftime(u"%d.%b"),)

            return day_range

        return search_ranges_str[u"day"][pattern.get(u"iday", -1)]

    def get_pattern_time(self, pattern):
        return search_ranges_str[u"time"][pattern.get(u"itime", -1)]

    def get_pattern_duration(self, pattern):
        s_duration = search_ranges_str[
            u"duration"][pattern.get(u"iduration", -1)]
        s_direction = search_ranges_str[
            u"direction"][pattern.get(u"iduration_dir", 0)]
        if len(s_duration) == 0:
            return u""

        return u"%s %s" % (s_direction, s_duration)

    def get_pattern_channel(self, pattern):
        c = pattern.get(u"channel", None)  # str, not int
        channels = self.state.get(u"channels", {})
        if c in channels.viewvalues():
            return str(c)
        else:
            return u""
        # return channels.get(pattern.get(u"ichannel", -1), u"").upper()

    def get_channel_number(self, channel_name):
        channels = self.state.get(u"channels", {})
        for (k, v) in channels.viewitems():
            if v == channel_name:
                return int(k)
        return -1

    def get_channel_index(self, channel_name):
        # Just differs from get_channel_number
        # if channels_dict.viewkeys() != [1, len(channel_dict)]
        channels = self.state.get(u"channels", {})
        try:
            return int(channels.viewvalues().index(channel_name))
        except ValueError:
            pass

        return -1

    def sprint_search_pattern(self, pattern):
        title = self.get_pattern_title(pattern)
        channel = self.get_pattern_channel(pattern).upper()
        sday = self.get_pattern_date(pattern)
        if b_mvweb:
            desc = self.get_pattern_desc(pattern)
            s = u"%s%s%s%s" % (
                (title + u" | ") if len(title) else u"",
                (sday + u" | ") if len(sday) else u"",
                (desc + u" | ") if len(desc) else u"",
                (u" (%s)" % (channel)) if len(channel) else u"",
            )
        else:
            sduration = self.get_pattern_duration(pattern)
            stime = self.get_pattern_time(pattern)
            s = u"%s%s%s%s%s" % (
                (title + u" | ") if len(title) else u"",
                (sday + u" | ") if len(sday) else u"",
                (stime + u", ") if len(stime) else u"",
                (sduration + u" ") if len(sduration) else u"",
                (u" (%s)" % (channel,)) if len(channel) else u"",
            )

        return s

    def create_search_params(self, pattern=None):
        if pattern is None:
            pattern = self.get_current_pattern()

        largs = [u"--search"]
        if len(pattern.get(u"title", u"")) > 0:
            largs.append(u"--title")
            largs.append(u"%s" % (pattern[u"title"],))

        chan_name = self.get_pattern_channel(pattern)
        if chan_name:
            largs.append(u"-C")
            largs.append(u"%s" % (chan_name,))

        if pattern.get(u"iday_range"):
            days = pattern.get(u"iday_range")
            date0 = datetime.date.fromtimestamp(min(days))
            date1 = datetime.date.fromtimestamp(max(days))
            today = datetime.date.today()
            largs.append(u"--dayMin")
            largs.append(u"%i" % ((today-date0).days,))
            largs.append(u"--dayMax")
            largs.append(u"%i" % ((today-date1).days,))
        elif pattern.get(u"iday", -1) > -1:
            # Here, iday is index of day number
            largs.append(u"--dayMax")
            largs.append(u"%i" % (search_ranges[u"day"][pattern[u"iday"]+1],))

        if pattern.get(u"iduration", -1) > -1:
            d = pattern.get(u"iduration", -1)
            """
            d0 = search_ranges[u"duration"][d]
            d1 = search_ranges[u"duration"][d+1]-1
            """
            d_dir = pattern.get(u"iduration_dir", 0)
            if d_dir:
                # [X-1, infty)
                d0 = search_ranges[u"duration"][d+1]-1
                d1 = search_ranges[u"duration"][-2]
            else:
                # [0, X]
                d0 = search_ranges[u"duration"][0]
                d1 = search_ranges[u"duration"][d+1]

            largs.append(u"--durationMin")
            largs.append(u"%i" % (d0,))
            largs.append(u"--durationMax")
            largs.append(u"%i" % (d1,))

        if pattern.get(u"itime", -1) > -1:
            d = pattern.get(u"itime", -1)
            d0 = search_ranges[u"time"][d]
            d1 = search_ranges[u"time"][d+1]
            largs.append(u"--beginMin")
            largs.append(u"%i" % (d0,))
            largs.append(u"--beginMax")
            largs.append(u"%i" % (d1,))

        for other_arg in pattern.get(u"args", []):
            largs.append(other_arg)

        return largs

    def update_db(self, bForceFullUpdate=False):
        largs = [u"update"]
        diff = mediathek.try_diff_update() and not bForceFullUpdate
        if diff:
            largs.append(u"diff")

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
    wirt.setProperty(u"simple_mediathek_de_state", s_state)


def load_volatile_state():
    wirts_id = 10025  # WINDOW_VIDEO_NAV, xbmcgui.getCurrentWindowId()
    wirt = xbmcgui.Window(wirts_id)
    s_state = str(wirt.getProperty(u"simple_mediathek_de_state"))
    if len(s_state) < 2:
        return {}
    return json.loads(s_state)


def build_url(query, state=None):
    if save_state_in_url and state:
        query.update({u"state": state})

    query[u"prev_mode"] = str(mode)
    sj = {u"j": json.dumps(query)}  # easier to decode
    return base_url + u"?" + urllib.urlencode(sj)


def unpack_url(squery):
    dsj = urlparse.parse_qs(squery)
    sj = dsj.get(u"j", [u"{}"])[0]
    return json.loads(sj)


def create_addon_data_folder(path):
    try:
        if not os.path.isdir(path):
            os.mkdir(path)
    except OSError:
        xbmcgui.Dialog().notification(
            addon_name,  # u"Can't create folder for addon data.",
            getLocalizedString(32340),
            xbmcgui.NOTIFICATION_ERROR, 5000)


def get_history_file_path():
    """ Return (path, filename) """
    # .kodi/userdata/addon_data/[addon name]
    path = xbmc.translatePath(
        addon.getAddonInfo(u"profile")).decode('utf-8')
    name = u"search_history.json"

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
    largs.append(u"--folder")
    largs.append(path)

    path = addon.getAddonInfo(u"path").decode('utf-8')
    script = os.path.join(path, u"root", u"bin", u"simple_mediathek")
    largs.insert(0, script)

    # Convert args into unicode strings
    # (Check if Kore app sends latin encoded?!)
    largs_uni = []
    for l in largs:
        if isinstance(l, __builtins__.unicode):
            largs_uni.append(l)
        else:
            try:
                largs_uni.append(l.decode('utf-8'))
            except UnicodeDecodeError:
                largs_uni.append(l.decode(u"latin1"))
    largs = largs_uni

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
        if u"true" == addon.getSetting(u"debug_binary"):
            import random
            x = int(10000*random.random())
            fout = file(u"/dev/shm/addon.stderr.%i" % (x,), "w")
            fout.write(str(stderr))  # .encode('utf-8'))
            fout.write(str(stdout))  # .encode('utf-8'))
            fout.close()

        ret = p.wait()

        return (ret, str_stdout)
    except ValueError:
        pass

    return (-1, u"")


def call_binary_v1(largs):
    import os
    import os.path
    # Returns (exit_code, str(stdout))

    # Add folder for results
    (path, name) = get_history_file_path()
    largs.append(u"--folder")
    largs.append(path)

    path = addon.getAddonInfo(u"path").decode('utf-8')
    script = os.path.join(path, "root", "bin", "simple_mediathek")
    largs.insert(0, script)
    try:
        (c_stdin, c_stdout, c_stderr) = os.popen3(str(" ".join(largs)), "r")
        out = c_stdout.read()
        c_stdin.close()
        c_stdout.close()
        c_stderr.close()

        return (0, out.decode('utf-8'))  # Or ascii?
    except ValueError:
        pass

    return (-1, u"")


def check_addon_status():
    # Made bash script executable
    # (After fresh addon installation in Krypthon 17.1 required.)
    if not b_mvweb:
        path = addon.getAddonInfo(u"path").decode('utf-8')
        binary_setup(path)


def is_searchable(pattern):
    i_crit_used = 0
    if len(pattern.get(u"title", u"")) > 0:
        i_crit_used += 1

    if len(pattern.get(u"channel", u"")) > 0:
        i_crit_used += 1

    if pattern.get(u"iduration", -1) > -1:
        i_crit_used += 1

    if (pattern.get(u"iday_range") or pattern.get(u"iday", -1) > -1):
        i_crit_used += 1

    if pattern.get(u"ibegin", -1) > -1:
        i_crit_used += 1

    return (i_crit_used > 0)


def gen_search_categories(mediathek):
    pattern = mediathek.get_current_pattern()
    last_update = mediathek.get_last_update_time()
    allow_update = mediathek.is_index_outdated()
    if last_update[0] == -1:
        update_str = u""
    else:
        update_str = u"%s %s %s" % (
            u"| Stand", last_update[1], (u"" if allow_update else u""))

    categories = []
    categories.append({u"name": getLocalizedString(32341), # u"Sender",
                       u"selection": mediathek.get_pattern_channel(
                           pattern).upper(),
                       u"mode": u"select_channel",
                       # u"thumbnail": item.findtext(u"thumbnail"),
                       # u"fanart": item.findtext(u"fanart"),
                       })

    categories.append({u"name": getLocalizedString(32342),  # u"Titel",
                       u"selection": mediathek.get_pattern_title(pattern),
                       u"mode": u"select_title",
                       u"id": expanded_state.get(u"input_request_id", 0) + 1
                       })
    categories.append({u"name": getLocalizedString(32343),  # u"Datum",
                        u"selection": mediathek.get_pattern_date(pattern),
                        u"mode": u"select_day",
                        })
    if not b_mvweb:
        categories.append({u"name": getLocalizedString(32344),  # u"Uhrzeit",
                           u"selection": mediathek.get_pattern_time(pattern),
                           u"mode": u"select_time",
                           })
        if duration_separate_minmax:
            categories.append({u"name": getLocalizedString(32345),  # u"Dauer",
                               u"selection":
                               mediathek.get_pattern_duration(pattern),
                               u"mode": u"select_duration_dir",
                               })
        else:
            categories.append({u"name": getLocalizedString(32345),  # u"Dauer",
                               u"selection":
                               mediathek.get_pattern_duration(pattern),
                               u"mode": u"select_duration",
                               })
    else:
        categories.append({u"name": getLocalizedString(32346),  # u"Beschreibung",
                           u"selection": mediathek.get_pattern_desc(pattern),
                           u"mode": u"select_desc",
                           u"id": expanded_state.get(u"input_request_id", 0) + 1
                           })

    categories.append({u"name": getLocalizedString(32347),  # u"Suchmuster leeren",
                       u"selection": u"",
                       u"mode": u"clear_pattern",
                       })
#    categories.append({u"name": getLocalizedString(32348),  # u"Suchmuster speichern",
#                       u"selection": u"",
#                       u"mode": u"main",
#                       })
    if not b_mvweb:
        categories.append({u"name": getLocalizedString(32349),  # u"Filmliste aktualisieren",
                           u"selection": update_str,
                           u"mode": u"update_db_over_gui",
                           # u"IsPlayable": allow_update,
                           })
    categories.append({u"name": getLocalizedString(32350),  # u"Vorherige Suchen",
                       u"selection": u"",
                       u"mode": u"show_history",
                       })

    return categories


def listing_add_list_names(listing, state, item, names, i_selected=-1):
    for i in xrange(len(names)-1):  # Skip "" at list end
        url = build_url({u"mode": u"update_pattern",
                         u"item": item, u"value": i}, state)
        li = xbmcgui.ListItem(names[i], iconImage=u"DefaultFolder.png")
        li.setProperty(u"IsPlayable", u"true")
        is_folder = True
        if i == i_selected:
            li.setLabel2(u"(selected)")
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
        title = u"%s %s" % (k, name.upper())
        if name == u"":
            continue
        url = build_url({u"mode": u"update_pattern",
                         u"item": item, u"value": name}, state)
        li = xbmcgui.ListItem(title, iconImage=u"DefaultFolder.png")
        li.setProperty(u"IsPlayable", u"true")
        is_folder = True
        if name == v_selected:
            li.setLabel2(u"(selected)")
            li.select(True)
        listing.append((url, li, is_folder))


def listing_add_min_max_entries(listing, state, mode,  i_selected=-1):
    for i in [0, 1]:
        name = search_ranges_str[u"direction"][i]
        url = build_url({u"mode": mode, u"dir": i}, state)
        li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
        li.setProperty(u"IsPlayable", u"true")
        is_folder = True
        if i_selected == i:
            li.setLabel2(u"(selected)")
            li.select(True)
        listing.append((url, li, is_folder))


def listing_direction_toggle(listing, state, mode):
    i_other = 1 - mediathek.get_current_pattern().get(u"iduration_dir", 0)
    other_dir_name = search_ranges_str[u"direction_b"][i_other]
    url = build_url({u"mode": mode, u"dir": i_other}, state)
    li = xbmcgui.ListItem(other_dir_name, iconImage=u"DefaultFolder.png")
    li.setProperty(u"IsPlayable", u"true")
    is_folder = True
    listing.append((url, li, is_folder))


def listing_add_calendar_entry(listing, state, b_selected=False):
    name = search_ranges_str[u"day_range"][0]
    url = build_url({u"mode": u"select_calendar"}, state)
    # u"item": u"iday_range", u"value": -1})
    li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
    li.setProperty(u"IsPlayable", u"true")
    is_folder = True
    if b_selected:
        li.setLabel2(u"(selected)")
        li.select(True)
    listing.append((url, li, is_folder))


# Favorites had same structure as history entries, but extra name key.
def listing_add_history(listing, state):
    history = mediathek.get_history()
    for h in history:
        name = h.get(u"name", mediathek.sprint_search_pattern(h))
        url = build_url({u"mode": u"select_history",
                         u"pattern": h}, state)
        li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
        li.setProperty(u"IsPlayable", u"true")
        is_folder = True
        listing.append((url, li, is_folder))


def listing_add_remove_entry(listing, state, item):
    if item not in state.get(u"current_pattern", {}):
        return
    name = getLocalizedString(32351)  # u"Suchkriterum entfernen"
    url = build_url({u"mode": u"update_pattern",
                     u"item": item}, state)
    li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
    li.setProperty(u"IsPlayable", u"true")
    is_folder = True
    listing.append((url, li, is_folder))


def listing_add_back_entry(listing, state):
    name = u"%-20s" % getLocalizedString(32318)  # (u"Zurück",)
    url = build_url({u"mode": u"main"}, state)
    li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
    li.setProperty(u"IsPlayable", u"true")
    is_folder = True
    listing.append((url, li, is_folder))


def listing_add_search(listing, mediathek, state):
    pattern = mediathek.get_current_pattern()
    if is_searchable(pattern):
        name = getLocalizedString(32352)  # u"Suche starten"
        url = build_url({u"mode": u"start_search"}, state)
        li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
        li.setProperty(u"IsPlayable", u"true")
        is_folder = True
        listing.append((url, li, is_folder))
    else:
        # name = u"Suche starten    [Nur mit Suchkriterium möglich]"
        name = u"%s    [%s]" % (getLocalizedString(32352),
                                getLocalizedString(32353))
        url = build_url({u"mode": u"main"}, state)
        li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
        li.setProperty(u"IsPlayable", u"false")
        is_folder = False
        listing.append((url, li, is_folder))


def listing_add_test(listing, state):
    # foo = xbmcgui.getCurrentWindowId()  # 10025
    # wirt = xbmcgui.Window(foo)
    # wirt.clearProperty(u"simple_mediathek_de_state")
    # wirt.setProperty(u"test", u"gespeichert %i" % (foo,))
    # xbmc.executebuiltin(u"UpdateLocalAddons")
    pass


def listing_add_livestreams(listing, state):
    name = u"%s" % (u"Livestreams")
    url = build_url({u"mode": u"start_search_livestreams"}, state)
    li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
    li.setProperty(u"IsPlayable", u"true")
    is_folder = True
    listing.append((url, li, is_folder))


def listing_add_search_results(listing, state, results):
    """ Example line:
        {"id": 154344, "topic": "...", "title": "...",
                        "ibegin": 1486244700, "begin": "04. Feb. 2017 22:45",
                        "iduration": 816, "ichannel": 19,
                        "channel": "zdf", "anchor": 51792695},

    """
    i = 0
    for f in results:  # .get(u"found", []):
        hasBegin = (int(f.get(u"ibegin", -1)) > 8000)
        name = u"%s %s | %s" % (
            f.get(u"channel").upper(),
            u"| " + f.get(u"begin") if hasBegin else u"",
            f.get(u"title", u"???"),
        )

        url = build_url({u"mode": u"select_result", u"iresult": i}, state)
        li = xbmcgui.ListItem(name, iconImage=u"DefaultVideo.png")
        li.setProperty(u"IsPlayable", u"true")
        # It is no folder if quality will be auto selected.
        is_folder = (0 == int(addon.getSetting(u"video_quality")))
        listing.append((url, li, is_folder))
        i += 1
        if i >= max_num_entries_per_page:
            break


def listing_search_page_link(listing, state, results, ipage):
    if ipage < 0:
        ipage = -ipage - 1
        # name = u"Vorherige Seite (%i)" % (ipage+1)
        name = u"%s (%i)" % (getLocalizedString(32354), ipage+1)
    else:
        # name = u"Nächste Seite (%i)" % (ipage+1)
        name = u"%s (%i)" % (getLocalizedString(32355), ipage+1)

    # pattern = mediathek.get_current_pattern()
        url = build_url(
            state)
    if True:  # is_searchable(pattern):
        url = build_url({u"mode": (u"start_search_livestreams" if
                                   b_livestream else u"start_search"),
                         u"page": ipage}, state)
        li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
        li.setProperty(u"IsPlayable", u"true")
        is_folder = True
        listing.append((url, li, is_folder))


def list_urls(state, urls, search_result):
    """
    squality = [(u"Geringe Auflösung", u""),
                (u"Geringe Auflösung", u"  (RTMP)"),
                (u"Mittlere Auflösung", u""),
                (u"Mittlere Auflösung", u"  (RTMP)"),
                (u"Hohe Auflösung", u""),
                (u"Hohe Auflösung", u"  (RTMP)")]
    """
    iquality = [(32357, 32300), (32357, 32356),
                (32358, 32300), (32358, 32356),
                (32359, 32300), (32359, 32356)]
    listing = []
    for i in xrange(6):
        if len(urls[i]) == 0:
            continue
        name = u"%-22s | %s... %s" % (
            # squality[i][0], urls[i][:30], squality[i][1]
            getLocalizedString(iquality[i][0]),
            urls[i][:30],
            getLocalizedString(iquality[i][1]),
        )
        """
        url = build_url({u"mode": u"play_url",
                         u"video_url": urls[i]}, state)
        """
        url = urls[i]  # Direct url seems more stable
        li = xbmcgui.ListItem(name, iconImage=u"DefaultVideo.png")
        li.setProperty(u"IsPlayable", u"true")
        li.setProperty(u"mimetype", u"video")
        is_folder = False
        li.setProperty(u"duration",
                       str(int(search_result.get(u"iduration", 0)) / 60))
        li.setProperty(u"title", search_result.get(u"title", u""))  # Deprecated?
        li.setInfo(u"video", {u"title": search_result.get(u"title", u""),
                              u"genre": u"MediathekView"})
        listing.append((url, li, is_folder))

    # Entry to go back to list of search results. The state variable
    # still holds the array with the results.
    url = build_url({u"mode": u"show_search_result"}, state)
    li = xbmcgui.ListItem(u"Zurück", iconImage=u"DefaultFolder.png")
    li.setProperty(u"IsPlayable", u"true")
    is_folder = True
    listing.append((url, li, is_folder))

    xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
    xbmcplugin.endOfDirectory(
        addon_handle, updateListing=same_folder,
        cacheToDisc=directory_cache)


def play_url(addon_handle, state, url, result=None, b_add_to_history=False):
    # xbmc.Player().play(item=(u"%s" % (url,))) # more reliable?!
    # or...
    item = xbmcgui.ListItem(path=url)
    item.setInfo(type=u"Video", infoLabels={
        u"Title": url if result is None else result.get(u"title", u"")})
    xbmcplugin.setResolvedUrl(handle=addon_handle, succeeded=True,
                              listitem=item)


def blankScreen():
    """
    Black screen to reduce cpu usage (for RPi1).
    Usefulness depends on selected skin. In my default
    setting the cpu usage on RPI fall from 40% to 20%
    and the update time from 1m40 to 1m0.
    (Disabled because the 40% were caused by the rotating
    update icon and this would still be displayed..
    """
    # xbmc.executebuiltin(u"FullScreen")  # do not work
    pass


def handle_update_side_effects(args):
    """
    Simple updates rewrite a key-value pair.
    More complex relations could be placed here.
    Return True if the normal update of the key should be avoided.
    """
    if args.get(u"item") == u"iday":
        pattern = mediathek.get_current_pattern()
        if u"iday_range" in pattern:
            pattern.pop(u"iday_range")

    return False

# ==============================================
# Main code

addon_name = addon.getAddonInfo(u"name")

with SimpleMediathek() as mediathek:
    # RunScript handling
    if sys.argv[1] == u"update_db":
        mode = u"background_script_call"
        xbmcgui.Dialog().notification(
            addon_name,  # u"Update gestartet",
            getLocalizedString(32369),
            xbmcgui.NOTIFICATION_INFO)
        ok, tdiff = False, 0
        try:
            (ok, start, end, diff) = mediathek.update_db(bForceFullUpdate=True)
            tdiff = (end-start).seconds
        except:
            ok = False

        if ok:
            xbmcgui.Dialog().notification(
                addon_name,  # u"Update erfolgreich. Dauer %is" % (tdiff),
                getLocalizedString(32360).format(sec=tdiff),
                xbmcgui.NOTIFICATION_INFO)
        else:
            xbmcgui.Dialog().notification(
                addon_name,  # u"Update fehlgeschlagen",
                getLocalizedString(32361),
                xbmcgui.NOTIFICATION_ERROR, 5000)

    elif sys.argv[1] == u"delete_local_data":
        mode = u"background_script_call"
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
            addon_name,  # u"%i Dateien entfernt" % (len(rm_files)),
            getLocalizedString(32362).format(num=len(rm_files)),
            xbmcgui.NOTIFICATION_INFO, 5000)

    elif sys.argv[1] == u"XXX":
        mode = u"not used"
    else:
        addon_handle = int(sys.argv[1])
        xbmcplugin.setContent(addon_handle, u"movies")

        # Set default view
        force_view = int(addon.getSetting(u"force_view"))
        if force_view:
            # 50 (List) or 51 (Wide List)
            xbmc.executebuiltin(u"Container.SetViewMode(%i)" %
                                (force_view + 49))
        else:
            # View for other skins
            #xbmc.executebuiltin(u"Container.SetViewMode(50)")
            pass

        """
        skin_used = xbmc.getSkinDir()
        if(skin_used in SKINS_WIDE_LIST
            xbmc.executebuiltin(u"Container.SetViewMode(51)")
        elif skin_used in SKINS_LIST:
            xbmc.executebuiltin(u"Container.SetViewMode(50)")
        """

        # args = urlparse.parse_qs(sys.argv[2][1:])
        args = unpack_url(sys.argv[2][1:])
        mode = args.get(u"mode", None)
        prev_mode = args.get(u"prev_mode", u"None")  # str
        same_folder = (prev_mode != u"None")  # do NOT compare with 'is not'
        b_livestream = False
        b_mvweb = (u"true" == addon.getSetting(u"use_mediathekviewweb"))

        # Update with unsaved changes, but not force
        # write of changes at end of script
        if save_state_in_url:
            expanded_state = args.get(u"state", {})
        else:
            expanded_state = load_volatile_state()

        mediathek.update_state(expanded_state, False)

    # Handle modes
    if mode == u"update_db_over_gui":
        ok, tdiff = False, 0
        (ok, start, end, diff) = mediathek.update_db(
            bForceFullUpdate=False)
        if ok:
            tdiff = (end-start).seconds

        if ok:
            xbmcgui.Dialog().notification(
                addon_name,  # u"Update erfolgreich. Dauer %is" % (tdiff),
                getLocalizedString(32360).format(sec=tdiff),
                xbmcgui.NOTIFICATION_INFO)
        else:
            xbmcgui.Dialog().notification(
                addon_name,  # u"Update fehlgeschlagen",
                getLocalizedString(32361),
                xbmcgui.NOTIFICATION_ERROR, 5000)

    if mode == u"select_calendar":
        # Not implemented
        dialog = xbmcgui.Dialog()
        result1 = dialog.input(
            # u"Erstes Datum eingeben",
            getLocalizedString(32363),
            type=xbmcgui.INPUT_DATE)
        if len(result1) > 0:
            # Split and check u"DD/MM/YYYY" format. Extra spaces, i.e.
            # '22/ 1/2000' had to be free'd by the spaces.
            result1 = result1.replace(u" ", u"")
            try:
                t = time.strptime(result1, u"%d/%m/%Y")
                sec1 = int(time.strftime(u"%s", t))

                result2 = dialog.input(
                    # u"Zweites Datum eingeben",
                    getLocalizedString(32370),
                    result1,
                    type=xbmcgui.INPUT_DATE)
                if len(result2) > 0:
                    result2 = result2.replace(u" ", u"")
                    t = time.strptime(result2, u"%d/%m/%Y")
                    sec2 = int(time.strftime(u"%s", t))
                else:
                    sec2 = sec1

                iday_range = [sec1, sec2]
                iday_range.sort()
                pattern = mediathek.get_current_pattern()
                if u"iday" in pattern:
                    pattern.pop(u"iday")
                pattern.update({u"iday_range": iday_range})

                changes = {u"current_pattern": pattern}
                mediathek.update_state(changes, False)  # Unwritten update
                expanded_state.update(changes)  # propagated by Uri

                mode = u"main"
            # except (ValueError, TypeError) as e:
            except (RuntimeError,) as e:
                xbmcgui.Dialog().notification(
                    addon_name,  # u"Error during parsing of date strings.",
                    getLocalizedString(32364),
                    xbmcgui.NOTIFICATION_ERROR, 5000)
                mode = u"select_day"

    if mode == u"select_day":
        names = search_ranges_str[u"day"]
        i_sel = mediathek.get_current_pattern().get(u"iday", -1)
        b_day_range = u"iday_range" in mediathek.get_current_pattern()
        listing = []
        listing_add_list_names(listing, expanded_state, u"iday", names, i_sel)
        listing_add_calendar_entry(listing, expanded_state, b_day_range)
        listing_add_remove_entry(listing, mediathek.state,
                                 "iday_range" if b_day_range else u"iday")
        listing_add_back_entry(listing, expanded_state)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=same_folder,
            cacheToDisc=directory_cache)

    if mode == u"select_time":
        names = search_ranges_str[u"time"]
        i_sel = mediathek.get_current_pattern().get(u"itime", -1)
        listing = []
        listing_add_list_names(listing, expanded_state, u"itime", names, i_sel)
        listing_add_remove_entry(listing, mediathek.state, u"itime")
        listing_add_back_entry(listing, expanded_state)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=same_folder,
            cacheToDisc=directory_cache)

    if mode == u"select_duration_dir":
        listing = []
        i_sel = mediathek.get_current_pattern().get(u"iduration_dir", -1)
        listing_add_min_max_entries(listing, expanded_state,
                                    u"select_duration", i_sel)
        # listing_add_back_entry(listing, expanded_state)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=same_folder,
            cacheToDisc=directory_cache)

    if mode == u"select_duration":
        # 0. Handle return value of select_duration_dir step
        if u"dir" in args:
            u"iduration_dir"
            pattern = mediathek.get_current_pattern()
            pattern.update({u"iduration_dir": args[u"dir"]})
            changes = {u"current_pattern": pattern}
            mediathek.update_state(changes, False)  # Unwritten update
            expanded_state.update(changes)  # propagated by Uri

        # 1. Generate list
        names = search_ranges_str[u"duration"]
        i_sel = mediathek.get_current_pattern().get(u"iduration", -1)
        listing = []
        listing_add_list_names(listing, expanded_state, u"iduration", names, i_sel)
        listing_add_remove_entry(listing, mediathek.state, u"iduration")

        # 0b. Add toggle for direction flag
        if not duration_separate_minmax:
            listing_direction_toggle(listing, expanded_state, u"select_duration")

        listing_add_back_entry(listing, expanded_state)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=same_folder,
            cacheToDisc=directory_cache)

    if mode == u"select_channel":
        channels = mediathek.get_channel_list()
        current_pattern = mediathek.get_current_pattern()
        listing = []
        listing_add_dict_names(listing, expanded_state, u"channel", channels,
                               current_pattern.get(u"channel", u""))
        listing_add_remove_entry(listing, mediathek.state, u"channel")
        listing_add_back_entry(listing, expanded_state)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=same_folder,
            cacheToDisc=directory_cache)

    if mode == u"select_history":
        if u"pattern" in args:
            new_pattern = args.get(u"pattern")
            # Clean up unset values. Note that this
            # made the state writing mandatory to prevent mixing of
            # old (readed state) and new (expanded_state) pattern.
            new_pattern = {k: v for k, v in new_pattern.viewitems()
                           if v != -1 and v != u""}
            changes = {u"current_pattern": new_pattern}
            mediathek.update_state(changes, True)  # Write update
            expanded_state.update(changes)

        # Value applied. Go back and show main menu
        mode = u"main"

    if mode in [u"select_title", u"select_desc"]:
        # Check if this url/mode was open again after leaving the
        # plugin. If yes, go to main page.

        def _get_string(key, header, pattern, start_text):
            result, query = keyboard.keyboard_input( header, start_text)
            if result:
                pattern[key] = query
                changes = {u"current_pattern": pattern}
                mediathek.update_state(changes, False)  # Unwritten update
                expanded_state.update(changes)  # propagated by Uri

        last = expanded_state.setdefault(u"input_request_id", -1)
        arg_id = int(args.get(u"id", 0))
        if arg_id == last:
            mode = u"main"
        else:
            pattern = mediathek.get_current_pattern()
            if mode == u"select_desc":
                _get_string(u"description",  # u"Beschreibung")
                            getLocalizedString(32346),
                            pattern,
                            mediathek.get_pattern_desc(pattern))
            else:
                _get_string(u"title",  # u"Titel / Thema (No RegEx)")
                            getLocalizedString(32366),
                            pattern,
                            mediathek.get_pattern_title(pattern))

            expanded_state[u"input_request_id"] = arg_id
            mode = u"main"

    if mode == u"select_result":
        results = mediathek.get_search_results().get(u"found", [])

        if u"true" != addon.getSetting(u"early_history_update"):
            pattern = mediathek.get_current_pattern()
            mediathek.add_to_history(pattern)

        try:
            result = results[args.get(u"iresult", -1)]
            # u"Nachfragen", u"Beste verfügbare", u"Geringste verfügbare",
            # u"Niedrig", u"Mittel", u"Hoch"
            quali = int(addon.getSetting(u"video_quality"))

            if result.get(u"payload"):
                # Webrequest already contain the data
                urls = result.get(u"payload")
            else:
                # Fetch urls
                s_anchor = str(result[u"anchor"])
                url_args = [u"--payload", s_anchor]
                (exit_code, data) = call_binary(url_args)
                if exit_code == 0:
                    js = json.loads(data)
                    urls = js.get(s_anchor, [])

            urls.extend([u"", u"", u"", u"", u"", u""])  # len(urls) >= 6
            # Resort by video quality, 2xmid, 2xlow, 2xhigh
            qualities = [2, 3, 0, 1, 4, 5]
            urls = [urls[q] for q in qualities]
            # Now, its ordered by quality, but with empty entries.

            non_empty_urls = [u for u in urls if len(u) > 0]
            if len(non_empty_urls) == 0:
                xbmcgui.Dialog().notification(
                    addon_name,   # u"Keine URL gefunden",
                    getLocalizedString(32367),
                    xbmcgui.NOTIFICATION_ERROR, 5000)
            else:
                if quali == 0:
                    # Show list
                    list_urls(expanded_state, urls, result)
                elif quali in [1, 2]:
                    url = non_empty_urls[1-quali]  # [0] or [-1]
                    # Play file
                    play_url(addon_handle, expanded_state, url, result)
                else:
                    # Reduce on urls of quality low, mid or high
                    urls2 = urls[2*(quali-3): 2*(quali-3)+1]
                    non_empty2 = [u for u in urls2 if len(u) > 0]
                    if len(non_empty2) > 0:
                        # Play file
                        play_url(addon_handle, expanded_state, non_empty2[0], result)
                    else:
                        # Show list
                        list_urls(expanded_state, urls, result)

        except IndexError:
            # results incomplete...
            mode = u"main"
        except KeyError:
            # Args incomplete...
            mode = u"main"
        # else:
        #     mode = u"main"

    if mode == u"update_pattern":
        if u"item" in args:
            pattern = mediathek.get_current_pattern()
            if u"value" in args:
                try:
                    value = int(args[u"value"])
                    handle_update_side_effects(args)
                except ValueError:
                    value = args[u"value"]

                pattern.update({args[u"item"]: value})
            elif args[u"item"] in pattern:
                pattern.pop(args[u"item"])

            changes = {u"current_pattern": pattern}
            mediathek.update_state(changes, False)  # Unwritten update
            expanded_state.update(changes)  # propagated by Uri

        # Value applied. Go back and show main menu
        mode = u"main"

    if mode == u"clear_pattern":
        pattern = mediathek.get_current_pattern()
        new_pattern = {u"iduration_dir": pattern.get(u"iduration_dir", 0)}
        changes = {u"current_pattern": new_pattern}
        mediathek.update_state(changes, False)  # Unwritten update
        expanded_state.update(changes)  # propagated by Uri
        mode = u"main"

    if mode in [u"start_search", u"start_search_livestreams"]:
        if mode == u"start_search_livestreams":
            pattern = mediathek.get_livestream_pattern()
            b_livestream = True
        else:
            pattern = mediathek.get_current_pattern()

        results = {u"pattern": pattern, u"found": []}
        if b_mvweb:
            (exit_code, data) = MVWeb.fetch2(pattern, args.get(u"page", 0),
                                            max_num_entries_per_page + 1)
            if exit_code == 0:
                js = MVWeb.convert_results2(data)
        else:
            search_args = mediathek.create_search_params(pattern)

            # Add page flag
            page = u"%u,%u" % (max_num_entries_per_page + 1,
                               args.get(u"page", 0)*max_num_entries_per_page)
            search_args.append(u"--num")
            search_args.append(page)

            # Add sorting flag
            sorting = int(addon.getSetting(u"sorting"))
            if sorting:
                search_args.append(u"--sort")
                if sorting == 1:
                    search_args.append(u"date")
                elif sorting == 2:
                    search_args.append(u"dateAsc")
                else:
                    search_args.pop(-1)

            (exit_code, data) = call_binary(search_args)
            if exit_code == 0:
                js = json.loads(data)

        if exit_code == 0:  # Continue for both above branches
            results.update(js)
            if u"true" == addon.getSetting(u"early_history_update"):
                mediathek.add_to_history(pattern)
        else:
            xbmcgui.Dialog().notification(
                addon_name,  # u"Suche fehlgeschlagen",
                getLocalizedString(32368),
                xbmcgui.NOTIFICATION_ERROR, 5000)
            # mode = u"main"

        changes = {u"latest_search": results}
        expanded_state.update(changes)   # Held until kodi exits
        mode = u"show_search_result"

    if mode == u"show_search_result":
        if u"latest_search" in expanded_state:
            results = expanded_state[u"latest_search"].get(u"found", [])
            listing = []
            prev_page = args.get(u"page", 0) - 1
            next_page = (prev_page + 2) if (
                len(results) > max_num_entries_per_page) else -1

            if prev_page > -1:
                listing_search_page_link(listing, expanded_state,
                                         results, -prev_page-1)

            listing_add_search_results(listing, expanded_state, results)
            if next_page > -1:
                listing_search_page_link(listing, expanded_state,
                                         results, next_page)

            # listing_add_back_entry(listing, expanded_state)
            xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
            if False:
                # Had strange effects...
                xbmcplugin.endOfDirectory(
                    addon_handle, updateListing=(prev_page >= 0),
                    cacheToDisc=directory_cache)
            else:
                xbmcplugin.endOfDirectory(
                    addon_handle, updateListing=same_folder,
                    cacheToDisc=directory_cache)
        else:
            mode = u"main"

    if mode == u"show_history":
        listing = []
        listing_add_history(listing, expanded_state)
        listing_add_back_entry(listing, expanded_state)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(addon_handle, updateListing=same_folder)

    if mode == u"play_url":
        if u"video_url" in args:
            play_url(addon_handle, expanded_state, args[u"video_url"])

    if mode is None or mode == u"main":
        xbmc.log(msg=u"Plugin|"+sys.argv[0]+sys.argv[2],
                 level=xbmc.LOGERROR)
        check_addon_status()

        #  Top level page of plugin
        search_categories = gen_search_categories(mediathek)

        if mode is None:
            if b_mvweb:
                if mediathek.is_mvw_outdated():
                    mediathek.update_mvw()
                    expanded_state[u"ilast_mvw_update"] = int(time.time())
            elif mediathek.is_index_outdated():
                # --info call to check if data was externally updated
                mediathek.update_channel_list()
                # Update creation timestamp to omit multiple updates.
                expanded_state[u"icreation"] = int(time.time())

        listing = []
        for cat in search_categories:
            name = u"%-20s %s" % (
                cat.get(u"name", u"?"), cat.get(u"selection", u"!"))
            # name += str(build_url({}, expanded_state))[45:]
            new_args = {u"mode": cat.get(u"mode")}
            if u"id" in cat:
                new_args[u"id"] = cat[u"id"]

            url = build_url(new_args, expanded_state)
            li = xbmcgui.ListItem(name, iconImage=u"DefaultFolder.png")
            if cat.get(u"IsPlayable", True):
                li.setProperty(u"IsPlayable", u"true")
                is_folder = True
            else:
                li.setProperty(u"IsPlayable", u"false")
                is_folder = False

            listing.append((url, li, is_folder))

        # listing_add_test(listing, expanded_state)
        listing_add_livestreams(listing, expanded_state)

        listing_add_search(listing, mediathek, expanded_state)
        xbmcplugin.addDirectoryItems(addon_handle, listing, len(listing))
        xbmcplugin.endOfDirectory(
            addon_handle, updateListing=same_folder,
            cacheToDisc=directory_cache)

    if not save_state_in_url:
        store_volatile_state(expanded_state)
