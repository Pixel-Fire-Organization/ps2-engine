"""Valve-220 .map parsing: entities, brushes, and brush -> convex face polygons.

A brush is an intersection of half-spaces (one per face plane). Each face's
polygon is found by starting with a huge quad on the face plane and clipping it
against every other face plane of the brush (Sutherland-Hodgman). UVs use the
Valve-220 per-face U/V axes.

Plane convention (verified against a floor brush whose top face must point +Z):
    normal = normalize(cross(p3 - p1, p2 - p1)),  dist = dot(normal, p1)
The normal points OUT of the solid; the brush interior is dot(n, X) - dist < 0.
"""

import math
import re


# --- small vector helpers (plain (x, y, z) tuples) --------------------------

def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def _normalize(a):
    length = math.sqrt(_dot(a, a))
    if length <= 1e-12:
        return (0.0, 0.0, 0.0)
    return (a[0] / length, a[1] / length, a[2] / length)


class Face:
    def __init__(self, points, texture, u_axis, u_off, v_axis, v_off, rotation, u_scale, v_scale):
        self.points = points  # 3 defining points
        self.texture = texture
        self.u_axis = u_axis
        self.u_off = u_off
        self.v_axis = v_axis
        self.v_off = v_off
        self.rotation = rotation
        self.u_scale = u_scale if u_scale else 1.0
        self.v_scale = v_scale if v_scale else 1.0
        p1, p2, p3 = points
        self.normal = _normalize(_cross(_sub(p3, p1), _sub(p2, p1)))
        self.dist = _dot(self.normal, p1)

    def uv(self, point, tex_w, tex_h):
        """Valve-220 planar UV in [0..1]-ish texture space (may tile outside)."""
        u = (_dot(point, self.u_axis) / self.u_scale + self.u_off) / float(tex_w)
        v = (_dot(point, self.v_axis) / self.v_scale + self.v_off) / float(tex_h)
        return (u, v)


class Brush:
    def __init__(self, faces):
        self.faces = faces


class Entity:
    def __init__(self, props, brushes):
        self.props = props
        self.brushes = brushes

    @property
    def classname(self):
        return self.props.get("classname", "")


# --- parsing ----------------------------------------------------------------

_FACE_RE = re.compile(r"\(|\)|\[|\]|\S+")


def _parse_face(line):
    toks = _FACE_RE.findall(line)
    i = 0

    def expect(sym):
        nonlocal i
        if toks[i] != sym:
            raise ValueError(f"expected '{sym}' got '{toks[i]}' in: {line}")
        i += 1

    def num():
        nonlocal i
        v = float(toks[i])
        i += 1
        return v

    points = []
    for _ in range(3):
        expect("(")
        points.append((num(), num(), num()))
        expect(")")
    texture = toks[i]
    i += 1
    expect("[")
    u_axis = (num(), num(), num())
    u_off = num()
    expect("]")
    expect("[")
    v_axis = (num(), num(), num())
    v_off = num()
    expect("]")
    rotation = num()
    u_scale = num()
    v_scale = num()
    return Face(points, texture, u_axis, u_off, v_axis, v_off, rotation, u_scale, v_scale)


def parse_map(path):
    """Parse a .map into a list of Entity. Entity 0 is worldspawn."""
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        lines = []
        for raw in fh:
            s = raw.strip()
            if s.startswith("//"):
                s = ""
            lines.append(s)

    entities = []
    i, n = 0, len(lines)
    while i < n:
        if lines[i] == "{":
            ent, i = _parse_entity(lines, i + 1)
            entities.append(ent)
        else:
            i += 1
    return entities


_KV_RE = re.compile(r'"(.*?)"\s+"(.*?)"')


def _parse_entity(lines, i):
    props = {}
    brushes = []
    while i < len(lines):
        line = lines[i]
        if line == "}":
            return Entity(props, brushes), i + 1
        if line == "{":
            brush, i = _parse_brush(lines, i + 1)
            brushes.append(brush)
            continue
        m = _KV_RE.match(line)
        if m:
            props[m.group(1)] = m.group(2)
        i += 1
    return Entity(props, brushes), i


def _parse_brush(lines, i):
    faces = []
    while i < len(lines):
        line = lines[i]
        if line == "}":
            return Brush(faces), i + 1
        if line.startswith("("):
            faces.append(_parse_face(line))
        i += 1
    return Brush(faces), i


# --- brush -> polygons ------------------------------------------------------

def _base_polygon(normal, dist, size=1.0e5):
    """A large quad lying on the plane, wound CCW about `normal`."""
    ax, ay, az = abs(normal[0]), abs(normal[1]), abs(normal[2])
    if ax <= ay and ax <= az:
        up = (1.0, 0.0, 0.0)
    elif ay <= az:
        up = (0.0, 1.0, 0.0)
    else:
        up = (0.0, 0.0, 1.0)
    u = _normalize(_cross(up, normal))
    v = _normalize(_cross(normal, u))
    origin = _scale(normal, dist)
    return [
        _add(origin, _add(_scale(u, -size), _scale(v, -size))),
        _add(origin, _add(_scale(u, -size), _scale(v, size))),
        _add(origin, _add(_scale(u, size), _scale(v, size))),
        _add(origin, _add(_scale(u, size), _scale(v, -size))),
    ]


def _clip(poly, normal, dist, eps):
    """Sutherland-Hodgman: keep the half-space dot(n, X) - dist <= eps (inside)."""
    out = []
    count = len(poly)
    for idx in range(count):
        a = poly[idx]
        b = poly[(idx + 1) % count]
        da = _dot(normal, a) - dist
        db = _dot(normal, b) - dist
        if da <= eps:
            out.append(a)
            if db > eps:
                t = da / (da - db)
                out.append(_add(a, _scale(_sub(b, a), t)))
        elif db <= eps:
            t = da / (da - db)
            out.append(_add(a, _scale(_sub(b, a), t)))
    return out


def _newell_normal(poly):
    nx = ny = nz = 0.0
    count = len(poly)
    for idx in range(count):
        a = poly[idx]
        b = poly[(idx + 1) % count]
        nx += (a[1] - b[1]) * (a[2] + b[2])
        ny += (a[2] - b[2]) * (a[0] + b[0])
        nz += (a[0] - b[0]) * (a[1] + b[1])
    return (nx, ny, nz)


def _dedup(poly, eps=1e-4):
    out = []
    for p in poly:
        if not out or _dot(_sub(p, out[-1]), _sub(p, out[-1])) > eps * eps:
            out.append(p)
    if len(out) > 1 and _dot(_sub(out[0], out[-1]), _sub(out[0], out[-1])) <= eps * eps:
        out.pop()
    return out


def brush_polygons(brush, eps=0.05):
    """Return [(face, [vertices])] for a brush. Vertices are wound CCW about the
    face's outward normal (front-facing for a CCW = front convention)."""
    result = []
    for face in brush.faces:
        poly = _base_polygon(face.normal, face.dist)
        for other in brush.faces:
            if other is face:
                continue
            poly = _clip(poly, other.normal, other.dist, eps)
            if len(poly) < 3:
                break
        poly = _dedup(poly)
        if len(poly) < 3:
            continue
        if _dot(_normalize(_newell_normal(poly)), face.normal) < 0:
            poly = poly[::-1]
        result.append((face, poly))
    return result
