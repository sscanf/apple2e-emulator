# Settings for dmgbuild: the macOS disk image with the app and a shortcut to
# Applications, laid out to drag one onto the other.
#   dmgbuild -s tools/dmg_settings.py -D app="Apple IIe.app" "Apple IIe" out.dmg
import os.path

app = defines.get("app", "Apple IIe.app")  # noqa: F821 (set by dmgbuild)
app_name = os.path.basename(app)

format = "UDZO"
files = [app]
symlinks = {"Applications": "/Applications"}

background = "builtin-arrow"
window_rect = ((200, 160), (640, 400))
default_view = "icon-view"
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
icon_size = 128
text_size = 13
icon_locations = {app_name: (165, 120), "Applications": (475, 120)}
