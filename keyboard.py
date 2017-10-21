# -*- coding: utf-8 -*-

# import sys
# import xbmc
import xbmcgui
# import xbmcplugin
# import xbmcaddon


def keyboard_input(title, default="", hidden=False):
    """ The kodi documentation shows no way to distict
    between cancelling (empty string) and clearing of input field.

    Thus, I add an extra space and and handle " " as clearing of the field...
    """
    # Starting with Gotham (13.X ...)
    dialog = xbmcgui.Dialog()
    result = dialog.input(title, u" "+str(default.encode('utf-8')),
                          type=xbmcgui.INPUT_ALPHANUM)
    if result:
        text = result.decode('utf-8').strip()
        return True, text

    return False, u""
