"""Run inside the editor (UnrealEditor-Cmd -ExecutePythonScript=...): authors the matte material and the level."""
import unreal

mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

MAT_PATH = "/Game/Materials"
MAT_NAME = "M_Matte"
if unreal.EditorAssetLibrary.does_asset_exist(f"{MAT_PATH}/{MAT_NAME}"):
    unreal.EditorAssetLibrary.delete_asset(f"{MAT_PATH}/{MAT_NAME}")
if True:
    mat = at.create_asset(MAT_NAME, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mat.set_editor_property("two_sided", True)
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.2, 0.2, 0.2, 1.0))
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 250)
    rough.set_editor_property("r", 1.0)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 400)
    spec.set_editor_property("r", 0.0)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -700, 120)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 100)
    mel.connect_material_expressions(color, "", tint, "A")
    mel.connect_material_expressions(vc, "", tint, "B")
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 600)
    k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 600)
    k.set_editor_property("r", 0.10)
    mel.connect_material_expressions(tint, "", glow, "A")
    mel.connect_material_expressions(k, "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log("created M_Matte")

MAP = "/Game/Maps/Wingsuit"
if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
    unreal.EditorLevelLibrary.new_level(MAP)
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("created map")
unreal.log("make_level done")


def make_terrain(name, use_xz):
    """World-space textured terrain material: vertex colour x Color x photo-texture detail at two scales."""
    path = f"{MAT_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    m = at.create_asset(name, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    tex = unreal.load_asset("/Engine/StarterContent/Textures/T_ground_Moss_D")
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    wp = E(unreal.MaterialExpressionWorldPosition, -1500, 0)
    mask = E(unreal.MaterialExpressionComponentMask, -1300, 0)
    mask.set_editor_property("r", True); mask.set_editor_property("g", not use_xz)
    mask.set_editor_property("b", use_xz); mask.set_editor_property("a", False)
    mel.connect_material_expressions(wp, "", mask, "")
    samples = []
    for i, scale in enumerate((1.0 / 1400.0, 1.0 / 11000.0)):
        c = E(unreal.MaterialExpressionConstant, -1300, 200 + i * 200); c.set_editor_property("r", scale)
        uv = E(unreal.MaterialExpressionMultiply, -1100, 100 + i * 200)
        mel.connect_material_expressions(mask, "", uv, "A"); mel.connect_material_expressions(c, "", uv, "B")
        ts = E(unreal.MaterialExpressionTextureSample, -900, 100 + i * 200)
        ts.set_editor_property("texture", tex)
        mel.connect_material_expressions(uv, "", ts, "UVs")
        samples.append(ts)
    add = E(unreal.MaterialExpressionAdd, -650, 200)
    mel.connect_material_expressions(samples[0], "G", add, "A"); mel.connect_material_expressions(samples[1], "G", add, "B")
    k = E(unreal.MaterialExpressionScalarParameter, -650, 400)
    k.set_editor_property("parameter_name", "Detail"); k.set_editor_property("default_value", 1.9)
    det = E(unreal.MaterialExpressionMultiply, -450, 250)
    mel.connect_material_expressions(add, "", det, "A"); mel.connect_material_expressions(k, "", det, "B")
    col = E(unreal.MaterialExpressionVectorParameter, -650, -200)
    col.set_editor_property("parameter_name", "Color"); col.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    vc = E(unreal.MaterialExpressionVertexColor, -650, -50)
    tint = E(unreal.MaterialExpressionMultiply, -450, -100)
    mel.connect_material_expressions(col, "", tint, "A"); mel.connect_material_expressions(vc, "", tint, "B")
    base = E(unreal.MaterialExpressionMultiply, -250, 50)
    mel.connect_material_expressions(tint, "", base, "A"); mel.connect_material_expressions(det, "", base, "B")
    rough = E(unreal.MaterialExpressionConstant, -250, 300); rough.set_editor_property("r", 0.92)
    spec = E(unreal.MaterialExpressionConstant, -250, 400); spec.set_editor_property("r", 0.15)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log(f"created {name} tex={tex is not None}")


make_terrain("M_TerrainTop", False)
make_terrain("M_TerrainCliff", True)


def import_rock():
    """Imports the PBRgen granite set and rebuilds M_TerrainCliff as a full PBR material."""
    src = unreal.Paths.project_dir() + "Import/rock/"
    base = "T_Weathered_Granite_Cliff_"
    dest = "/Game/Textures/Rock"
    tasks = []
    for suf in ("BC", "N", "ORM"):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f"{src}{base}{suf}.png")
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    at.import_asset_tasks(tasks)
    bc = unreal.load_asset(f"{dest}/{base}BC")
    nr = unreal.load_asset(f"{dest}/{base}N")
    orm = unreal.load_asset(f"{dest}/{base}ORM")
    if not (bc and nr and orm):
        unreal.log_error("rock textures missing"); return
    bc.set_editor_property("srgb", True)
    nr.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    nr.set_editor_property("srgb", False)
    orm.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    orm.set_editor_property("srgb", False)
    for t in (bc, nr, orm):
        unreal.EditorAssetLibrary.save_loaded_asset(t)

    name = "M_TerrainCliff"
    path = f"{MAT_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    m = at.create_asset(name, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    wp = E(unreal.MaterialExpressionWorldPosition, -1700, 0)
    mask = E(unreal.MaterialExpressionComponentMask, -1500, 0)
    mask.set_editor_property("r", True); mask.set_editor_property("g", False)
    mask.set_editor_property("b", True); mask.set_editor_property("a", False)
    mel.connect_material_expressions(wp, "", mask, "")

    def sample(tex, scale, kind, y):
        c = E(unreal.MaterialExpressionConstant, -1500, y + 80); c.set_editor_property("r", scale)
        uv = E(unreal.MaterialExpressionMultiply, -1300, y)
        mel.connect_material_expressions(mask, "", uv, "A"); mel.connect_material_expressions(c, "", uv, "B")
        ts = E(unreal.MaterialExpressionTextureSample, -1100, y)
        ts.set_editor_property("texture", tex)
        ts.set_editor_property("sampler_type", kind)
        mel.connect_material_expressions(uv, "", ts, "UVs")
        return ts

    S = unreal.MaterialSamplerType
    col1 = sample(bc, 1.0 / 1000.0, S.SAMPLERTYPE_COLOR, -300)
    col2 = sample(bc, 1.0 / 6500.0, S.SAMPLERTYPE_COLOR, 0)
    nrm = sample(nr, 1.0 / 1000.0, S.SAMPLERTYPE_NORMAL, 300)
    orm_s = sample(orm, 1.0 / 1000.0, S.SAMPLERTYPE_MASKS, 600)
    mix = E(unreal.MaterialExpressionLinearInterpolate, -800, -150)
    half = E(unreal.MaterialExpressionConstant, -1000, -50); half.set_editor_property("r", 0.45)
    mel.connect_material_expressions(col1, "", mix, "A"); mel.connect_material_expressions(col2, "", mix, "B")
    mel.connect_material_expressions(half, "", mix, "Alpha")
    vc = E(unreal.MaterialExpressionVertexColor, -800, 120)
    vk = E(unreal.MaterialExpressionConstant, -800, 220); vk.set_editor_property("r", 1.3)
    vmul = E(unreal.MaterialExpressionMultiply, -600, 160)
    mel.connect_material_expressions(vc, "", vmul, "A"); mel.connect_material_expressions(vk, "", vmul, "B")
    off = E(unreal.MaterialExpressionConstant, -600, 260); off.set_editor_property("r", 0.55)
    tint = E(unreal.MaterialExpressionAdd, -420, 190)
    mel.connect_material_expressions(vmul, "", tint, "A"); mel.connect_material_expressions(off, "", tint, "B")
    base_col = E(unreal.MaterialExpressionMultiply, -250, -50)
    mel.connect_material_expressions(mix, "", base_col, "A"); mel.connect_material_expressions(tint, "", base_col, "B")
    mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(orm_s, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm_s, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm_s, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log("created M_TerrainCliff (PBR rock)")


import_rock()


def make_suit():
    """Plain lit material for the skeletal mannequin (no vertex colour, usable on skeletal meshes)."""
    name = "M_Suit"
    path = f"{MAT_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    m = at.create_asset(name, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_skeletal_mesh", True)
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    col.set_editor_property("parameter_name", "Color"); col.set_editor_property("default_value", unreal.LinearColor(0.01, 0.26, 0.24, 1))
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 200); rough.set_editor_property("r", 0.55)
    spec = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 300); spec.set_editor_property("r", 0.35)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log("created M_Suit")


make_suit()


def make_backdrop():
    """Imports Import/background.png (or the generated fallback) and builds an unlit scrolling material."""
    base = unreal.Paths.project_dir() + "Import/"
    src = base + "background.png" if os.path.exists(base + "background.png") else base + "background_fallback.png"
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", "/Game/Textures/Background")
    t.set_editor_property("destination_name", "T_Background")
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", True)
    at.import_asset_tasks([t])
    tex = unreal.load_asset("/Game/Textures/Background/T_Background")
    if not tex:
        unreal.log_error("background texture missing"); return
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    name = "M_Backdrop"; path = f"{MAT_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    m = at.create_asset(name, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    tc = E(unreal.MaterialExpressionTextureCoordinate, -900, 0)
    sc = E(unreal.MaterialExpressionVectorParameter, -900, 200)
    sc.set_editor_property("parameter_name", "Scroll"); sc.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
    mk = E(unreal.MaterialExpressionComponentMask, -700, 200)
    mk.set_editor_property("r", True); mk.set_editor_property("g", True); mk.set_editor_property("b", False); mk.set_editor_property("a", False)
    mel.connect_material_expressions(sc, "", mk, "")
    add = E(unreal.MaterialExpressionAdd, -500, 100)
    mel.connect_material_expressions(tc, "", add, "A"); mel.connect_material_expressions(mk, "", add, "B")
    ts = E(unreal.MaterialExpressionTextureSample, -300, 100)
    ts.set_editor_property("texture", tex)
    mel.connect_material_expressions(add, "", ts, "UVs")
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log("created M_Backdrop from " + src)


import os
make_backdrop()
exec(open(unreal.Paths.project_dir() + "Tools/make_assets2.py").read())
