"""Editor script part 2: audio import. Executed from make_level.py."""
import unreal

mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
MAT_PATH = "/Game/Materials"


def import_audio():
    base = unreal.Paths.project_dir() + "Import/Audio/"
    names = ["wind_loop", "music_loop", "gate_chime", "gate_miss", "close_pass", "jump", "beep", "crash", "land", "flare"]
    tasks = []
    for n in names:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f"{base}{n}.wav")
        t.set_editor_property("destination_path", "/Game/Audio")
        t.set_editor_property("destination_name", n)
        t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", True)
        tasks.append(t)
    at.import_asset_tasks(tasks)
    for n in names:
        w = unreal.load_asset(f"/Game/Audio/{n}")
        if not w:
            unreal.log_error("missing sound " + n); continue
        w.set_editor_property("looping", n in ("wind_loop", "music_loop"))
        unreal.EditorAssetLibrary.save_loaded_asset(w)
    unreal.log("imported audio")


import_audio()
