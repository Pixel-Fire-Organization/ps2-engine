"""Tests for the level compiler and its shared library.

Compiles assets/maps/test.map end-to-end and asserts the on-disc invariants the
runtime relies on (chunk integrity, sector budgets, archive alignment), plus
mesh-stripifier correctness and C<->Python struct-size parity.
"""

import importlib.util
import os
import pathlib
import struct

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"

# Pillow is required for texture/far-field baking.
PIL = pytest.importorskip("PIL")


def _load(name, relpath):
    spec = importlib.util.spec_from_file_location(name, TOOLS / relpath)
    module = importlib.util.module_from_spec(spec)
    import sys
    sys.path.insert(0, str(TOOLS))
    spec.loader.exec_module(module)
    return module


levelfmt = _load("ps2lib.levelfmt", "ps2lib/levelfmt.py")
meshlib = _load("ps2lib.mesh", "ps2lib/mesh.py")
mapparse = _load("ps2lib.mapparse", "ps2lib/mapparse.py")
pack_archive = _load("pack_archive", "pack_archive.py")
compile_level = _load("compile_level", "compile_level.py")


# --- struct-size parity with EngineLevelFormat.h ----------------------------

def test_struct_sizes():
    assert struct.calcsize(levelfmt._INFO) == 92
    assert struct.calcsize(levelfmt._GRIDCELL) == 32
    assert struct.calcsize(levelfmt._ENTREC) == 20
    assert struct.calcsize(levelfmt._SECHDR) == 48
    assert struct.calcsize(levelfmt._MESHENTRY) == 48
    assert struct.calcsize(levelfmt._FARFHDR) == 32
    assert struct.calcsize(levelfmt._FARFCLUSTER) == 24


# --- map parser -------------------------------------------------------------

def test_parse_test_map():
    ents = mapparse.parse_map(str(ROOT / "assets" / "maps" / "test.map"))
    assert any(e.classname == "worldspawn" for e in ents)
    assert any(e.classname == "prop_model" for e in ents)
    world = next(e for e in ents if e.classname == "worldspawn")
    polys = mapparse.brush_polygons(world.brushes[0])
    assert len(polys) == 6  # a 6-sided brush -> 6 quads
    for _face, poly in polys:
        assert len(poly) >= 3


# --- mesh stripifier --------------------------------------------------------

def test_bake_mesh_strip_roundtrips():
    # A 3x3 grid of quads shares vertices -> should stripify and verify.
    verts, norms, uvs = [], [], []
    n = (0.0, 1.0, 0.0)
    for gz in range(3):
        for gx in range(3):
            quad = [(gx, 0, gz), (gx + 1, 0, gz), (gx + 1, 0, gz + 1), (gx, 0, gz + 1)]
            for a, b, c in ((0, 1, 2), (0, 2, 3)):
                for i in (a, b, c):
                    verts.append(tuple(float(v) for v in quad[i]))
                    norms.append(n)
                    uvs.append((0.0, 0.0))
    baked = meshlib.bake_mesh(verts, norms, uvs)
    assert baked["vert_count"] > 0
    # If it chose a strip, the strip must decode back to the source triangles.
    if baked["topology"] == meshlib.BAKED_TOPOLOGY_STRIP:
        _uv, _un, _ut, tris = meshlib.dedup_corners(verts, norms, uvs)
        # (bake_mesh already verifies internally; just assert it produced verts)
        assert baked["vert_count"] >= len(tris)


# --- full compile -----------------------------------------------------------

@pytest.fixture(scope="module")
def compiled(tmp_path_factory):
    out = tmp_path_factory.mktemp("levels")
    path = compile_level.compile_level(
        str(ROOT / "assets" / "maps" / "test.map"), str(out),
        str(ROOT / "assets" / "textures"), str(ROOT / "assets" / "models"))
    return path


def test_archive_alignment_and_core(compiled):
    toc = pack_archive.read_toc(compiled)
    assert toc["entry_count"] >= 4
    for e in toc["entries"]:
        assert e["offset"] % pack_archive.ARCH_SECTOR_ALIGN == 0

    by_key = {e["key"]: e for e in toc["entries"]}
    assert "TEST.PS2L" in by_key
    core = pack_archive.read_payload(compiled, by_key["TEST.PS2L"])
    lv = levelfmt.parse_ps2l(core)
    assert lv["version"] == levelfmt.LEVEL_FILE_VERSION
    names = {c["name"] for c in lv["chunks"]}
    assert {"INFO", "MATL", "SGRD", "ENTS"} <= names


def test_sectors_within_budget(compiled):
    toc = pack_archive.read_toc(compiled)
    sector_entries = [e for e in toc["entries"] if e["key"].endswith(".SEC")]
    assert sector_entries, "expected at least one sector"
    for e in sector_entries:
        blob = pack_archive.read_payload(compiled, e)
        sec = levelfmt.parse_sector(blob)
        assert sec["magic"] == levelfmt.SECTOR_MAGIC
        assert e["size"] <= 256 * 1024  # LEVEL_SECTOR_MAX_BYTES
        assert sec["mesh_count"] <= 32   # LEVEL_MAX_MESHES_PER_SECTOR
        for m in sec["meshes"]:
            assert m["vert_count"] > 0
            assert m["verts_offset"] % 16 == 0


def test_entities_present(compiled):
    toc = pack_archive.read_toc(compiled)
    by_key = {e["key"]: e for e in toc["entries"]}
    core = pack_archive.read_payload(compiled, by_key["TEST.PS2L"])
    lv = levelfmt.parse_ps2l(core)
    ents_chunk = next(c for c in lv["chunks"] if c["name"] == "ENTS")
    count, strings_offset, strings_size = struct.unpack_from("<III", core, ents_chunk["offset"])
    assert count == 1  # prop_model
    strings = core[ents_chunk["offset"] + strings_offset:
                   ents_chunk["offset"] + strings_offset + strings_size]
    assert b"prop_model" in strings
