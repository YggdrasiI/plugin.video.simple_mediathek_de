from socketIO_client import SocketIO, BaseNamespace
from datetime import datetime

pattern =  {"iduration_dir": 1, "iday": 4, "iduration": 1,
            "title": "Die Anstalt",
            "description": None,
            "ichannel": -1,
            "channel": "ZDF"}



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
        print( "Error: Input is no 1-tuple as expected.")
        return {"found": found}

    dResult = tResult[0]
    if dResult.get("err"):
        print( u"Error: %s\n" % dResult.get("err"))
        return {"found": found}

    for r in dResult.get("result",{}).get("results",[]):
        # print(u"%s - %s" % (r.get("topic"), r.get("title")))
        x = {
            "topic": r.get("topic"),
            "title": r.get("title"),
            "ibegin": int(r.get("timestamp")), # or?
            # "ibegin": int(r.get("filmlisteTimestamp")),
            "iduration": r.get("duration"),
            "channel": r.get("channel").lower(),
            "ichannel": -1,  # Different enummeration
        }
        # Parse ibegin
        d = datetime.fromtimestamp(x.get("ibegin", 0))
        x["begin"] = d.strftime("%d. %b. %Y %R")

        # Add urls in same order as simple_mediathek --payload returns.
        x["payload"] = [r.get("url_video"), "",
                        r.get("url_video_low"), "",
                        r.get("url_video_hd"), ""]

        found.append(x)

    return {"found": found}

def fetch(pattern, page=0, entries_per_page=10):

    class FilmNamespace(BaseNamespace):
        response = None

        """
        def on_connect(self): print('[Connected]')
        def on_reconnect(self): print('[Reconnected]')
        def on_disconnect(self): print('[Disconnected]')

        """
        def on_film_response(self, *args):
            self.response = args

    ## Build query
    def get_string(d, key):
        # Just to avoid str-cast of None
        r = d.get(key)
        return r.strip() if r else ""

    queries = []
    title = get_string(pattern, "title")
    if len(title):
           queries.append({"fields": ["title", "topic"], "query": title})

    description = get_string(pattern, "description")
    if len(description):
           queries.append({"fields": ["description"], "query": description})

    channel = get_string(pattern, "channel")
    if len(channel):
           queries.append({"fields": ["channel"], "query": channel})

    sortProps = pattern.get("sortBy", ("timestamp", "desc"))
    queryObj = {"queries": queries,
                "sortBy": sortProps[0],
                "sortOrder": sortProps[1],
                "future": True,
                "offset": page*entries_per_page,
                "size": entries_per_page
               }

    # Send query
    with SocketIO("https://mediathekviewweb.de", 443, verify=False) as socketIO:

        film_namespace = socketIO.define(FilmNamespace)
        socketIO.emit('queryEntries', queryObj, film_namespace.on_film_response)

        try:
            #socketIO.wait(2.0)
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
