# -*- coding: utf-8 -*-

from socketIO_client import SocketIO, BaseNamespace
from datetime import datetime

def convert_results(tResult):
    """
    Input data example
    ({u'result':
        {u'queryInfo':
            {u'filmlisteTimestamp': u'1508505240', u'totalResults': 72,
            u'searchEngineTime': u'8.86', u'resultCount': 1},
         u'results':
             [{u'url_video': u'', u'url_video_hd': u'', u'url_video_low': u'',
             u'description': u'', u'title': u'', u'timestamp': 1508271300,
             u'filmlisteTimestamp': u'1508317980',
             u'id': u'jwnNEpXxbx7eoH0WCo49NoTBWh1jFI3eIham6IDuEbk=',
             u'topic': u'', u'url_website': u'', u'url_subtitle': u'',
             u'duration': 467, u'channel': u'ZDF', u'size': 140509184}]},
      u'err': None},)

    Output data example
    {"found":
        [{"id": 154344, "topic": "...", "title": "...",
        "ibegin": 1486244700, "begin": "04. Feb. 2017 22:45",
        "iduration": 816, "ichannel": 19,
        "channel": "zdf", "anchor": 51792695
        }, ...
        ] }
    """
    found = []

    if len(tResult) < 1:
        print( u"Error: Input is no 1-tuple as expected.")
        return {u"found": found}

    dResult = tResult[0]
    if dResult.get(u"err"):
        print( u"Error: %s\n" % dResult.get(u"err"))
        return {u"found": found}

    for r in dResult.get(u"result",{}).get(u"results",[]):
        # print(u"%s - %s" % (r.get(u"topic"), r.get(u"title")))
        x = {
            u"topic": r.get(u"topic"),
            u"title": r.get(u"title"),
            u"ibegin": int(r.get(u"timestamp")), # or?
            # u"ibegin": int(r.get(u"filmlisteTimestamp")),
            u"iduration": r.get(u"duration"),
            u"channel": r.get(u"channel").lower(),
            u"ichannel": -1,  # Different enummeration
        }
        # Parse ibegin
        d = datetime.fromtimestamp(x.get(u"ibegin", 0))
        x[u"begin"] = d.strftime("%d. %b. %Y %R").decode('utf-8')

        # Add urls in same order as simple_mediathek --payload returns.
        x[u"payload"] = [r.get(u"url_video"), u"",
                        r.get(u"url_video_low"), u"",
                        r.get(u"url_video_hd"), u""]

        found.append(x)

    return {u"found": found}

def fetch(pattern, page=0, entries_per_page=10):

    class FilmNamespace(BaseNamespace):
        response = None

        """
        def on_connect(self): print("[Connected]")
        def on_reconnect(self): print("[Reconnected]")
        def on_disconnect(self): print("[Disconnected]")

        """
        def on_film_response(self, *args):
            self.response = args

    ## Build query
    def get_string(d, key):
        # Just to avoid str-cast of None
        r = d.get(key)
        return r.strip() if r else u""

    queries = []
    title = get_string(pattern, u"title")
    if len(title):
           queries.append({u"fields": [u"title", u"topic"], u"query": title})

    description = get_string(pattern, u"description")
    if len(description):
           queries.append({u"fields": [u"description"], u"query": description})

    channel = get_string(pattern, u"channel")
    if len(channel):
           queries.append({u"fields": [u"channel"], u"query": channel})

    sortProps = pattern.get(u"sortBy", (u"timestamp", u"desc"))
    queryObj = {u"queries": queries,
                u"sortBy": sortProps[0],
                u"sortOrder": sortProps[1],
                u"future": True,
                u"offset": page*entries_per_page,
                u"size": entries_per_page
               }

    # Send query
    with SocketIO(u"https://mediathekviewweb.de", 443, verify=True) as socketIO:

        film_namespace = socketIO.define(FilmNamespace)
        socketIO.emit(u"queryEntries", queryObj, film_namespace.on_film_response)

        try:
            # Sync threads
            socketIO.wait_for_callbacks(seconds=5.0)
            if film_namespace.response:
                return (0, film_namespace.response)
            else:
                return (-1, None)
        except Exception as e:
            print(e)


    # Request failed.
    return (-2, None)

def fetch_channel_list():
    import requests
    import json

    source_url = u"https://mediathekviewweb.de/api/channels"
    try:
        r = requests.get(source_url)
        if r.status_code != 200:
            raise requests.RequestException(response=r)

        sChannels = r.text.encode(r.encoding)

        args = json.loads(sChannels)
        lChannels = args.get("channels", [])

        # Normalise channel names and put them into dict
        dChannels = {unicode(k): lChannels[k].lower() for k in range(len(lChannels))}

        # Wrap with keyword
        dChannels = {u"channels": dChannels}
        return (0, dChannels)

    except requests.RequestException as e:
        err = u"Can't fetch channel list"
        xbmcgui.Dialog().notification(addon_name, err,
                                      xbmcgui.NOTIFICATION_ERROR, 5000)
    except ValueError:
        err = u"Can't parse json channel list"
        xbmcgui.Dialog().notification(addon_name, err,
                                      xbmcgui.NOTIFICATION_ERROR, 5000)

    # Fallback
    from channel_list import channels
    return (-1, channels)
