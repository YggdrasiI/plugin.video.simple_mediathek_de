# -*- coding: utf-8 -*-

#from socketIO_client import SocketIO, BaseNamespace
from datetime import datetime


def get_string(d, key):
    # Helper  to avoid str-cast of None
    r = d.get(key)
    return r.strip() if r else u""


def get_int(d, key, default=0):
    # Helper  to avoid int-cast of None
    r = d.get(key)
    if r is None: return default
    return int(r)


# For  MVW API 2.x
def convert_results2(dResult):
    """
    Input data example (subset, which shows path to urls)
    {"result": { "items": [{"document": { "media" : {...} } }]}}

    See mediathekviewweb/server/src/elasticsearch-definitions/mapping.ts
    for full doc.

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

    if len(dResult) < 1:
        print( u"Error: Input is no 1-tuple as expected.")
        return {u"found": found}

    #if dResult.get(u"err"):
    #    print( u"Error: %s\n" % dResult.get(u"err"))
    #    return {u"found": found}

    lItems = dResult.get("result", {}).get("items",[])
    for dItem in lItems:
        dDoc = dItem.get("document", {})
        x = {
            u"id": unicode(dItem.get("id", dDoc.get("id", ""))),
            u"topic": unicode(dDoc.get(u"topic")),
            u"title": unicode(dDoc.get(u"title")),
            u"ibegin": get_int(dDoc, u"timestamp", 0),
            u"iduration": get_int(dDoc, u"duration", -1),
            u"channel": unicode(dDoc.get(u"channel")).lower(),
            u"ichannel": -1,  # Different enummeration
        }
        # Parse ibegin
        d = datetime.fromtimestamp(x.get(u"ibegin", 0))
        x[u"begin"] = d.strftime("%d. %b. %Y %R").decode('utf-8')

        # Add urls in same order as simple_mediathek --payload returns.
        # Currently, its fixed on 0-2 urls for thee quality levels.
        urls = [u""] * 6
        # The input media dict had the keys type(?), url, size, quality
        # Quality range low=2, mid=3, high=4 (interpretation of 0,1?!)
        lMedia = dDoc.get("media", [])
        # Restore required ordering of video quality, 2xmid, 2xlow, 2xhigh
        _lmap = [0, 1, 1, 0, 2, 2]
        for dM in lMedia:
            if dM.get("type", -1) != 0:  # Video types: [0]
                continue

            q = _lmap[dM.get("quality", 0)]
            pos = 2*q if len(urls[2*q]) == 0 else 2*q+1
            urls[pos] = dM.get("url")

        x[u"payload"] = urls

        found.append(x)

    return {u"found": found}


def fetch2(pattern, page=0, entries_per_page=10):

    ## Build query
    def get_string(d, key):
        # Just to avoid str-cast of None
        r = d.get(key)
        return r.strip() if r else u""

    # Fill in subqueries of BoolQuery:
    # {"body": { "bool": { "must": [XXX]}}}

    lQueries = []
    title = get_string(pattern, u"title")
    if len(title):
        # TextQuery (type noted by 'text' key)
        lQueries.append({u"text":
                        { u"fields": [u"title", u"topic"],
                         u"text": title,
                         u"operator": "and"}})
    else:
        topic = get_string(pattern, u"topic")
        lQueries.append({u"text":
                        { u"fields": [u"topic"],
                         u"text": topic,
                         u"operator": "and"}})

    description = get_string(pattern, u"description")
    if len(description):
        lQueries.append({u"text":
                        { u"fields": [u"description"],
                         u"text": description,
                         u"operator": "and"}})

    channel = get_string(pattern, u"channel")
    if len(channel):
        lQueries.append({u"text":
                        { u"fields": [u"channel"],
                         u"text": channel,
                         u"operator": "and"}})

    lFilter = []
    if pattern.get(u"iday_range"):
        days = pattern.get(u"iday_range")
        date0 = datetime.fromtimestamp(min(days))
        date1 = datetime.fromtimestamp(max(days))
        today = datetime.today()
        lFilter.append({ "range": {
            "field": u"timestamp",
            "gte": u"now-%id/d" % ((today-date0).days,),
            "lt": u"now-%id/d" % ((today-date1).days-1,),
        }})

    elif pattern.get(u"iday", -1) > -1:
        # Transfer index into number of days
        from constants import search_ranges
        iday = search_ranges[u"day"][pattern[u"iday"]+1]
        lFilter.append({ "range": {
            "field": u"timestamp",
            "gte": u"now-%id/d" % (iday),
        }})

    # TODO: Filter for duration and begin

    # Sorts defined on same layer as body
    queryObj = {"body": { "bool": { "must": lQueries,
                                   "filter": lFilter,
                                   },
                         },
                "skip": page*entries_per_page,
                "limit": entries_per_page,
                "sorts": [
                    {
                        "field": "timestamp",
                        "order": "descending"
                    }, {
                        "field": "duration",
                        "order": "ascending"
                    }],
                }

    if u"web_args" in pattern:
        # TODO update overwrites subentries, not extends them!
        queryObj["body"]["bool"].update(pattern["web_args"])


    import xbmc
    xbmc.log(str(queryObj))
    # Send query
    import requests
    import json
    url = u"https://testing.mediathekviewweb.de/api/v2/query/json"
    try:
        r = requests.post(url, json=queryObj)
        if r.status_code != 200:
            raise requests.RequestException(response=r)

        sResult = r.text.encode(r.encoding)
        xbmc.log(sResult)
        dResult = json.loads(sResult)

        return (0, dResult)
    except requests.RequestException as e:
        xbmc.log("MediathekViewWeb error:"+str(e.message))
        return (-1, None)
    except ValueError:
        xbmc.log("MediathekViewWeb error:"+str(e.message))
        return (-2, None)

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
