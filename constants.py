# -*- coding: utf-8 -*-

# (Extra empty entries for list[-1]-access )
search_ranges_str = {
    u"duration": [u"10 min", u"30 min", u"60 min", u"1,5 h",
                  u"2 h", u""],
    u"direction": [u"Höchstens", u"Mindestens"],
    u"direction_b": [u"Suche auf Maximallänge umstellen...",
                     u"Suche auf Mindestlänge umstellen..."],
    u"time": [u"0-10 Uhr", u"10-16 Uhr", u"16-20 Uhr", u"20-24 Uhr", u""],
    u"day": [u"Heute und gestern", u"2 Tage", u"5 Tage", u"7 Tage", u"14 Tage",
             u""],  # u"Kalender", u""],
    u"day_range": [u"Kalender"],
    u"channel": [u"Kanal %s"],
}

# 323xx, 32300 is ""
search_ranges_locale = {
    u"duration": [32301, 32302, 32303, 32304, 32305, 32300],
    u"direction": [32306, 32307],
    u"direction_b": [32308, 32309],
	u"time": [32310, 32311, 32312, 32313, 32300],
    u"day": [32314, 32315, 32315, 32315, 32315, 32300],
    u"day_range": [32316],
	u"channel": [32317],
}
"""
Matching arguments for the search program.
search_ranges_str[u"duration"][i] corresponends with
[search_range[u"duration"][i], search_range[u"duration"][i+1] - 1]
"""
search_ranges = {
    u"duration": [0, 600, 1800, 3600, 5400, 7200, -1],
    u"time": [0, 36000, 57600, 72000, 86400, -1],
    u"day": [0, 1, 2, 5, 7, 14, -1, ],  # -1],
}
