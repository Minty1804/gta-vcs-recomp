#!/usr/bin/env python3
"""Cutscene characters in gameplay: each story character's cutscene model on its gameplay skeleton, as a mod.

    python tools/csped.py list                       the characters that have both models on your disc
    python tools/csped.py build [NAMES...] [--out DIR] [--iso ISO]
                                                     the mod (default: modloader/Cutscene characters): for each
                                                     character (all, or the names given: plr, phil, plr*), <name>.mdl
                                                     and <name>.xtx replacing the gameplay entries
    python tools/csped.py check FILE.mdl             a converted model read back: bones, weights, bounds

VCS keeps two models of Vic and of the story characters (docs/game-internals.md 12i): the gameplay one (`plr.mdl`,
`phil.mdl`: one finger bone a hand, a simple head) and the cutscene one (`csplr`, `csphil`: about 1.4 times the
vertices, the fingers apart, a face rig, textures 3-5 times larger). The cutscene models are not in the streaming
table: each sits in the gap after its texture dictionary (`csplr.xtx`) in GTA3PS2.IMG. They cannot be put in a
gameplay entry as they are: their skeleton is another (49 bones, other local axes, the arms in a T-pose where Vic's
gameplay model has them lowered, the whole model turned 180 degrees about z, some with the origin at the feet), so the
gameplay animations would twist them. So the cutscene *mesh* goes onto the gameplay *model*:

- the gameplay file stays (its node tree, its 25-bone hierarchy and skin, its collision spheres, its render callback)
  and its geometry is replaced by the cutscene geometry, re-bound to the gameplay skeleton;
- re-binding: the cutscene mesh is turned / moved onto the gameplay model's space (a fit of the trunk and legs'
  joints: a turn about z and a shift), then each limb whose direction differs (Vic's arms) is swung about its own
  joint, chain by chain, until its bones point the way the gameplay bones do - each vertex blended by its own weights;
- bones: every cutscene bone is the gameplay bone with its id, or that of its nearest ancestor which has one (the
  face rig -> the head, the second finger joint -> the finger); the weights of bones that merge are added;
- the positions quantised again (the geometry's scale / position and every mesh's box recomputed), the normals turned
  with their vertices; the strips, UVs and DMA layout unchanged.

Textures: the cutscene dictionary as it is (it holds every texture the cutscene mesh names). Everything written is
derived from your disc: it stays on this PC (a mod folder, never git).
"""
import argparse
import fnmatch
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import img  # noqa: E402
import mdlinfo  # noqa: E402
import toolchain as tc  # noqa: E402

MOD_NAME = "Cutscene characters"
PUSH, POP = 2, 1
# trunk and legs: the joints the two skeletons place alike (the fit that turns the cutscene mesh onto the gameplay one)
FIT_IDS = (0, 1, 2, 3, 41, 42, 43, 51, 52, 53)
MIN_SWING = math.radians(2.0)  # a limb closer than this to the gameplay direction is left as it is
MAX_SWING = math.radians(100.0)  # past this the two bones are not the same limb (a pose this far apart is not seen)
MIN_SEGMENT = 0.03  # m: a bone shorter than this (root -> pelvis, both at one point) has no direction to match


# ---- small linear algebra (3x4 affine: rows of [r0 r1 r2 t]) ----------------------------------------------------------
def rw_matrix(f):
    """A RenderWare matrix (right, up, at, pos; 16 floats) as 3x4 rows: p' = x right + y up + z at + pos."""
    return [[f[0], f[4], f[8], f[12]], [f[1], f[5], f[9], f[13]], [f[2], f[6], f[10], f[14]]]


def mul(a, b):
    return [[a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c] + (a[r][3] if c == 3 else 0.0) for c in range(4)]
            for r in range(3)]


def apply(a, p):
    return tuple(a[r][0] * p[0] + a[r][1] * p[1] + a[r][2] * p[2] + a[r][3] for r in range(3))


def rotate(a, v):
    return tuple(a[r][0] * v[0] + a[r][1] * v[1] + a[r][2] * v[2] for r in range(3))


def inverse(a):
    m = [row[:3] for row in a]
    det = (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))
    inv = [[(m[(c + 1) % 3][(r + 1) % 3] * m[(c + 2) % 3][(r + 2) % 3] -
             m[(c + 1) % 3][(r + 2) % 3] * m[(c + 2) % 3][(r + 1) % 3]) / det for c in range(3)] for r in range(3)]
    t = [-(inv[r][0] * a[0][3] + inv[r][1] * a[1][3] + inv[r][2] * a[2][3]) for r in range(3)]
    return [inv[r] + [t[r]] for r in range(3)]


def translation(t):
    return [[1.0, 0.0, 0.0, t[0]], [0.0, 1.0, 0.0, t[1]], [0.0, 0.0, 1.0, t[2]]]


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def norm(v):
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return (v[0] / n, v[1] / n, v[2] / n) if n > 1e-12 else (0.0, 0.0, 0.0)


def rotation_between(a, b):
    """The smallest rotation taking direction a to direction b (3x4, no translation), and its angle."""
    a, b = norm(a), norm(b)
    c = max(-1.0, min(1.0, a[0] * b[0] + a[1] * b[1] + a[2] * b[2]))
    axis = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    s = math.sqrt(axis[0] ** 2 + axis[1] ** 2 + axis[2] ** 2)
    angle = math.atan2(s, c)
    if s < 1e-9:
        if c > 0:
            return translation((0, 0, 0)), 0.0
        axis = norm((1.0, 0.0, 0.0) if abs(a[0]) < 0.9 else (0.0, 1.0, 0.0))
        axis = norm((a[1] * axis[2] - a[2] * axis[1], a[2] * axis[0] - a[0] * axis[2], a[0] * axis[1] - a[1] * axis[0]))
    else:
        axis = (axis[0] / s, axis[1] / s, axis[2] / s)
    x, y, z = axis
    cs, sn, k = math.cos(angle), math.sin(angle), 1 - math.cos(angle)
    return [[cs + x * x * k, x * y * k - z * sn, x * z * k + y * sn, 0.0],
            [y * x * k + z * sn, cs + y * y * k, y * z * k - x * sn, 0.0],
            [z * x * k - y * sn, z * y * k + x * sn, cs + z * z * k, 0.0]], angle


def about(point, rot):
    """rot applied about a point."""
    return mul(translation(point), mul(rot, translation((-point[0], -point[1], -point[2]))))


# ---- the models -------------------------------------------------------------------------------------------------------
class Ped:
    """A skinned ped model (gameplay: data at 0x20 = {collision, element group}; cutscene: the element group at 0x20)."""

    def __init__(self, data, name):
        self.name = name
        self.m = m = mdlinfo.Mdl(data)
        self.gameplay = m.num_funcs > 0
        self.group = m.ptr(0x24) if self.gameplay else 0x20
        if m.d[self.group] != 2:
            raise ValueError("%s: no element group at the model's data" % name)
        els = m.elements(self.group)
        if len(els) != 1:
            raise ValueError("%s: %d elements (a ped has one)" % (name, len(els)))
        self.element = els[0]
        self.geom = m.ptr(self.element + 0x14)
        self.skin = m.ptr(self.geom + 0x18)
        hier = m.ptr(self.element + 0x2C)
        if not (self.geom and self.skin and hier):
            raise ValueError("%s: not a skinned model" % name)
        count, bones = m.u32(hier + 4), m.ptr(hier + 0x30)
        if m.u32(self.skin) != count:
            raise ValueError("%s: skin of %d bones, hierarchy of %d" % (name, m.u32(self.skin), count))
        recs = [struct.unpack_from("<4B", m.d, bones + i * 8) for i in range(count)]
        self.ids = [r[0] for r in recs]
        # the hierarchy's tree (RenderWare's HAnim flags: PUSH keeps the parent for a later sibling, POP returns to it)
        self.parent, stack, parent = [], [], -1
        for i, r in enumerate(recs):
            self.parent.append(parent)
            if r[2] & PUSH:
                stack.append(parent)
            parent = i
            if r[2] & POP:
                parent = stack.pop() if stack else -1
        ibm_at = m.ptr(self.skin + 0x0C)
        self.ibm = [rw_matrix(struct.unpack_from("<16f", m.d, ibm_at + i * 64)) for i in range(count)]
        self.bind = [inverse(x) for x in self.ibm]  # bone -> model
        self.joint = [tuple(b[r][3] for r in range(3)) for b in self.bind]
        self.index_of = {}
        for i, bid in enumerate(self.ids):
            if bid != 0xFF and bid not in self.index_of:
                self.index_of[bid] = i
        self.children = [[] for _ in self.ids]
        for i, p in enumerate(self.parent):
            if p >= 0:
                self.children[p].append(i)
        # the geometry
        h = self.geom + 32
        size = m.u32(h + 16)
        self.num_meshes = size >> 20
        self.meshes_at = h + 0x40
        self.dma_base = self.meshes_at + self.num_meshes * 0x30
        last = max(m.u32(self.meshes_at + i * 0x30 + 28) for i in range(self.num_meshes))
        self.geom_end = self.dma_base + last + ((m.u32(self.dma_base + last) & 0xFFFF) + 1) * 16
        self.scale = struct.unpack_from("<3f", m.d, h + 0x28)
        self.pos = struct.unpack_from("<3f", m.d, h + 0x34)

    def batches(self, data=None):
        """Every batch of every mesh: (mesh index, {vnvl: (data offset, count)}) - offsets into this model's file."""
        d = self.m.d if data is None else data
        for mi in range(self.num_meshes):
            at = self.dma_base + struct.unpack_from("<I", d, self.meshes_at + mi * 0x30 + 28)[0]
            end, w = at + ((struct.unpack_from("<I", d, at)[0] & 0xFFFF) + 1) * 16, at + 16
            while w < end:
                if struct.unpack_from("<I", d, w)[0] != 0x6C018000:
                    break
                w += 20
                batch = {}
                while True:
                    code = struct.unpack_from("<I", d, w)[0]
                    cmd = code >> 24
                    if cmd == 0x14:
                        w += 4
                        while w < end and struct.unpack_from("<I", d, w)[0] == 0:
                            w += 4
                        break
                    if cmd == 0x20:
                        w += 8
                        continue
                    if cmd == 0x30:
                        w += 20
                        continue
                    if cmd & 0x60 != 0x60:
                        raise ValueError("%s: mesh %d: VIF code %08x in a batch" % (self.name, mi, code))
                    vnvl, num = cmd & 0x0F, (code >> 16) & 0xFF
                    size = {0x9: 6, 0x6: 2, 0xF: 2, 0xA: 3, 0xC: 16}[vnvl]
                    batch[vnvl] = (w + 4, num)
                    w += 4 + (num * size + 3) // 4 * 4
                for k in (0x9, 0xA, 0xC):
                    if k not in batch:
                        raise ValueError("%s: mesh %d: a batch without %s" % (self.name, mi, {9: "positions", 10: "normals", 12: "weights"}[k]))
                yield mi, batch


def correspondence(cs, ig):
    """Per cutscene bone: the gameplay bone it becomes and the rigid move (3x4, cutscene model -> gameplay model)."""
    target = []
    for i in range(len(cs.ids)):
        j = i
        while j >= 0 and cs.ids[j] not in ig.index_of:
            j = cs.parent[j]
        if j < 0:
            raise ValueError("%s: bone %d has no ancestor the gameplay skeleton knows" % (cs.name, i))
        target.append(ig.index_of[cs.ids[j]])
    # the fit: a turn about z (both stand on z) and a shift, from the trunk and the legs
    pairs = [(cs.joint[cs.index_of[b]], ig.joint[ig.index_of[b]]) for b in FIT_IDS if b in cs.index_of and b in ig.index_of]
    if len(pairs) < 4:
        raise ValueError("%s: too few shared joints to place the mesh" % cs.name)
    ca = tuple(sum(p[0][k] for p in pairs) / len(pairs) for k in range(3))
    cb = tuple(sum(p[1][k] for p in pairs) / len(pairs) for k in range(3))
    sxy = sum((a[0] - ca[0]) * (b[1] - cb[1]) - (a[1] - ca[1]) * (b[0] - cb[0]) for a, b in pairs)
    sxx = sum((a[0] - ca[0]) * (b[0] - cb[0]) + (a[1] - ca[1]) * (b[1] - cb[1]) for a, b in pairs)
    yaw = math.atan2(sxy, sxx)
    c, s = math.cos(yaw), math.sin(yaw)
    turn = [[c, -s, 0.0, 0.0], [s, c, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0]]
    shift = sub(cb, rotate(turn, ca))
    fit = mul(translation(shift), turn)
    residual = max(math.dist(apply(fit, a), b) for a, b in pairs)
    # the limbs, parent before child: a bone with exactly one child the gameplay skeleton shares is swung about its own
    # joint until it points as the gameplay bone does; its descendants move with it
    move = [None] * len(cs.ids)
    swings = []
    for i in range(len(cs.ids)):
        move[i] = fit if cs.parent[i] < 0 else move[cs.parent[i]]
        if cs.ids[i] not in ig.index_of:
            continue
        kids = [k for k in cs.children[i] if cs.ids[k] in ig.index_of]
        if len(kids) != 1:
            continue
        a, b = ig.index_of[cs.ids[i]], ig.index_of[cs.ids[kids[0]]]
        if ig.parent[b] != a:
            continue
        p, q = apply(move[i], cs.joint[i]), apply(move[i], cs.joint[kids[0]])
        if math.dist(p, q) < MIN_SEGMENT or math.dist(ig.joint[a], ig.joint[b]) < MIN_SEGMENT:
            continue
        rot, angle = rotation_between(sub(q, p), sub(ig.joint[b], ig.joint[a]))
        if angle > MAX_SWING:
            raise ValueError("bone %d points %.0f degrees away from the gameplay one" % (cs.ids[i], math.degrees(angle)))
        if angle >= MIN_SWING:
            move[i] = mul(about(p, rot), move[i])
            swings.append((cs.ids[i], math.degrees(angle)))
    return target, move, math.degrees(yaw), residual, swings


def q16(x):
    return max(-32768, min(32767, int(round(x * 32767.5))))


def rebind(cs, ig, block, base):
    """The cutscene geometry (copied into `block`, its geometry at `base` there) re-bound to the gameplay skeleton."""
    target, move, yaw, residual, swings = correspondence(cs, ig)
    d = cs.m.d
    sc, ps = cs.scale, cs.pos
    shift = base - cs.geom

    # every batch's vertices, in order (a later batch repeats the strip's last two; each copy is done alike)
    work = []
    bad_weights = 0
    for mi, b in cs.batches():
        (pa, n), (na, nn), (wa, wn) = b[0x9], b[0xA], b[0xC]
        if not (n == nn == wn):
            raise ValueError("%s: mesh %d: %d positions, %d normals, %d weights" % (cs.name, mi, n, nn, wn))
        for i in range(n):
            x, y, z = struct.unpack_from("<3h", d, pa + i * 6)
            p = (x / 32767.5 * sc[0] + ps[0], y / 32767.5 * sc[1] + ps[1], z / 32767.5 * sc[2] + ps[2])
            nrm = struct.unpack_from("<3b", d, na + i * 3)
            raw = struct.unpack_from("<4I", d, wa + i * 16)
            ws = struct.unpack_from("<4f", d, wa + i * 16)
            weights = [((r & 0x3FF) >> 2, w) for r, w in zip(raw, ws) if w > 0.0]
            if any(bone >= len(cs.ids) for bone, _ in weights):
                # a few cutscene vertices name bones past the skin's (csmarty: 10 of 3,400, indices 134-214 of 49 - on
                # the PS2 they read whatever lies past the matrices): the valid weights kept, else the nearest joint
                bad_weights += 1
                weights = [(bone, w) for bone, w in weights if bone < len(cs.ids)]
                if not weights:
                    weights = [(min(range(len(cs.ids)), key=lambda j: math.dist(cs.joint[j], p)), 1.0)]
            if not weights:
                raise ValueError("%s: a vertex with no weight" % cs.name)
            total = sum(w for _, w in weights)
            np_ = [0.0, 0.0, 0.0]
            nn_ = [0.0, 0.0, 0.0]
            for bone, w in weights:
                q = apply(move[bone], p)
                r = rotate(move[bone], nrm)
                for k in range(3):
                    np_[k] += q[k] * w / total
                    nn_[k] += r[k] * w / total
            merged = {}
            for bone, w in weights:
                merged[target[bone]] = merged.get(target[bone], 0.0) + w
            work.append((mi, pa + i * 6, na + i * 3, wa + i * 16, tuple(np_), norm(nn_), sorted(merged.items(), key=lambda t: -t[1])))

    xs = [w[4][0] for w in work]
    ys = [w[4][1] for w in work]
    zs = [w[4][2] for w in work]
    centre = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, (min(zs) + max(zs)) / 2)
    scale = tuple(max(1e-3, (max(c) - min(c)) / 2 * 1.001) for c in (xs, ys, zs))
    per_mesh = {}
    for mi, pa, na, wa, p, nrm, merged in work:
        qp = tuple(q16((p[k] - centre[k]) / scale[k]) for k in range(3))
        struct.pack_into("<3h", block, pa + shift, *qp)
        struct.pack_into("<3b", block, na + shift, *(max(-127, min(127, int(round(c * 127)))) for c in nrm))
        slots = []
        for bone, w in merged[:4]:
            bits = struct.unpack("<I", struct.pack("<f", w))[0]
            slots.append((bits & ~0x3FF) | (bone * 4))
        while len(slots) < 4:
            slots.append(merged[0][0] * 4)  # weight 0 on the first bone
        struct.pack_into("<4I", block, wa + shift, *slots)
        per_mesh.setdefault(mi, []).append((qp, p))

    # the meshes' bounds (a sphere about the vertices' mean; the box two units out, as the game's own) and the
    # geometry's (sphere, box, scale, position)
    boxes = []
    for mi in range(cs.num_meshes):
        vs = per_mesh.get(mi)
        rec = cs.meshes_at + mi * 0x30 + shift
        if not vs:
            continue
        mc = tuple(sum(v[1][k] for v in vs) / len(vs) for k in range(3))
        mr = max(math.dist(v[1], mc) for v in vs)
        box = tuple(max(-32768, min(v[0][k] for v in vs) - 2) for k in range(3)) + \
            tuple(min(32767, max(v[0][k] for v in vs) + 2) for k in range(3))
        boxes.append(box)
        struct.pack_into("<4f", block, rec, *mc, mr)
        struct.pack_into("<6h", block, rec + 36, *box)
    h = base + 32
    radius = max(math.dist(w[4], centre) for w in work)
    gbox = tuple(min(b[k] for b in boxes) for k in range(3)) + tuple(max(b[3 + k] for b in boxes) for k in range(3))
    struct.pack_into("<4f", block, h, *centre, radius)
    struct.pack_into("<6h", block, h + 28, *gbox)
    struct.pack_into("<3f", block, h + 0x28, *scale)
    struct.pack_into("<3f", block, h + 0x34, *centre)
    return {"vertices": len(work), "bad_weights": bad_weights, "yaw": yaw, "fit_error": residual, "swings": swings,
            "merged": sorted({cs.ids[i] for i in range(len(cs.ids)) if ig.ids[target[i]] != cs.ids[i]})}


def combine(ig, cs):
    """The gameplay model with the cutscene geometry (re-bound) in place of its own; returns the file's bytes."""
    m, c = ig.m, cs.m
    # the gameplay geometry goes: only its element points at it (checked), so the data after it moves down
    pointers_to_geom = [r for r in m.relocs if m.u32(r) == ig.geom]
    if pointers_to_geom != [ig.element + 0x14]:
        raise ValueError("%s: the geometry is pointed at from %s" % (ig.name, [hex(r) for r in pointers_to_geom]))
    for r in m.relocs:
        if ig.geom < m.u32(r) < ig.geom_end or (ig.geom <= r < ig.geom_end and r not in (ig.geom + 0x0C, ig.geom + 0x18)):
            raise ValueError("%s: a pointer into / inside the geometry at %x" % (ig.name, r))
    cut = (ig.geom_end - ig.geom) // 128 * 128
    D, G, R, F = m.data_size, m.global_tab, m.reloc_tab, m.file_size
    R_end = R + 4 * m.num_relocs
    if not (G == D and R == G + 4 * m.num_funcs and R_end <= F):
        raise ValueError("%s: an unexpected tail (global table %x, relocations %x, data %x)" % (ig.name, G, R, D))

    def moved(o):
        return o if o <= ig.geom else o - cut

    data = bytearray(m.d[:ig.geom] + m.d[ig.geom + cut:D])
    relocs = set()
    for r in m.relocs:
        if r >= G:
            continue
        if ig.geom <= r < ig.geom_end:
            continue  # the old geometry's own pointers
        v = m.u32(r)
        relocs.add(moved(r))
        if v < D:
            struct.pack_into("<I", data, moved(r), moved(v))
    # the cutscene geometry, at the same alignment (mod 128) as on the disc
    at = len(data)
    at += (cs.geom - at) % 128
    data += b"\0" * (at - len(data))
    block = bytearray(c.d[cs.geom:cs.geom_end])
    data += block
    shift = at - cs.geom
    info = rebind(cs, ig, data, at)
    # its pointers: materials list (+0x0C) -> copied below; skin (+0x18) -> the gameplay skin
    for r in c.relocs:
        if cs.geom <= r < cs.geom_end and r not in (cs.geom + 0x0C, cs.geom + 0x18):
            raise ValueError("%s: an unexpected pointer in the geometry at %x" % (cs.name, r))
    struct.pack_into("<I", data, at + 0x18, moved(ig.skin))
    relocs.add(at + 0x18)
    struct.pack_into("<I", data, moved(ig.element + 0x14), at)  # the element draws the new geometry
    # the materials (16 bytes each: texture name, colour, flags, effects) and their list
    tail_strings = []  # (offset in the new tail, bytes)
    cs_tail = c.reloc_tab + 4 * c.num_relocs
    ig_tail_len = F - R_end
    data += b"\0" * (-len(data) % 16)
    count = c.u32(cs.geom + 0x10)
    mats = [c.ptr(c.ptr(cs.geom + 0x0C) + i * 4) for i in range(count)]
    mat_at = {}
    for ma in mats:
        if ma in mat_at:
            continue
        if (ma + 0x0C) in c.relocs or (ma + 0x08) in c.relocs or (ma + 0x04) in c.relocs:
            raise ValueError("%s: a material with effects (not handled)" % cs.name)
        mat_at[ma] = len(data)
        data += c.d[ma:ma + 16]
    list_at = len(data)
    for ma in mats:
        data += struct.pack("<I", mat_at[ma])
    data += b"\0" * (-len(data) % 16)
    struct.pack_into("<I", data, at + 0x0C, list_at)
    relocs.add(at + 0x0C)
    relocs.update(list_at + 4 * i for i in range(count))
    name_fields = []
    for ma, new in mat_at.items():
        name = c.ptr(ma)
        if name:
            name_fields.append((new, name))
            relocs.add(new)
    # the tail: global table, relocation table, the gameplay model's strings, then the cutscene's (texture names)
    data_size = len(data)
    gtab = bytearray(m.d[G:R])
    for i in range(m.num_funcs):
        struct.pack_into("<I", gtab, i * 4, moved(struct.unpack_from("<I", gtab, i * 4)[0]))
    relocs.update(data_size + i * 4 for i in range(m.num_funcs) if G + i * 4 in m.relocs)
    reloc_tab = data_size + len(gtab)
    ig_tail_at = reloc_tab + 4 * len(relocs)
    cs_tail_at = ig_tail_at + ig_tail_len
    for r in m.relocs:
        if r < G and not (ig.geom <= r < ig.geom_end):
            v = m.u32(r)
            if R_end <= v < F:
                struct.pack_into("<I", data, moved(r), v - R_end + ig_tail_at)
            elif D <= v < R_end:
                raise ValueError("%s: a pointer into the relocation table" % ig.name)
    for field, name in name_fields:
        if not (cs_tail <= name < c.file_size):
            raise ValueError("%s: a texture name outside the tail" % cs.name)
        struct.pack_into("<I", data, field, name - cs_tail + cs_tail_at)
    rel = sorted(relocs)
    out = data + gtab + struct.pack("<%dI" % len(rel), *rel) + m.d[R_end:F] + c.d[cs_tail:c.file_size]
    struct.pack_into("<7I2H", out, 0, m.ident, m.shrink, len(out), data_size, reloc_tab, len(rel), data_size,
                     m.num_classes, m.num_funcs)
    info.update(size=len(out), was=m.file_size)
    return bytes(out), info


# ---- the disc ---------------------------------------------------------------------------------------------------------
def characters(table):
    """(name, gameplay model entry, gameplay dictionary entry, cutscene dictionary entry, cutscene model's sectors)."""
    ents = sorted(table["entries"], key=lambda e: e["offset"])
    named = {}
    for e in ents:
        if "name" in e:
            named[(e["name"], e.get("kind") == "txd")] = e
    out = []
    for i, e in enumerate(ents):
        name = e.get("name", "")
        if e.get("kind") != "txd" or not name.startswith("cs") or i + 1 >= len(ents):
            continue
        base = name[2:]
        mdl, txd = named.get((base, False)), named.get((base, True))
        if not (mdl and txd):
            continue
        start = e["offset"] + e["sectors"]
        gap = ents[i + 1]["offset"] - start
        if gap > 0:
            out.append((base, mdl, txd, e, (start, gap)))
    return sorted(out, key=lambda t: t[0])


def read_sectors(iso, table, offset, count):
    iso.seek((table["img"]["lsn"] + offset) * 2048)
    return iso.read(count * 2048)


def table_or_make():
    if not os.path.isfile(img.TABLE):
        print("no archive table yet: reading it from a hidden run (tools/img.py table, about a minute)")
        img.table(None)
    return img.load_table()


def build(names, out, iso_given):
    t = table_or_make()
    chars = [c for c in characters(t) if not names or any(fnmatch.fnmatch(c[0], n.lower()) for n in names)]
    if not chars:
        raise SystemExit("no character matches %s" % (" ".join(names) or "(all)"))
    os.makedirs(out, exist_ok=True)
    done, failed = 0, []
    with open(img.iso_path(iso_given), "rb") as iso:
        for base, mdl, txd, cstxd, (start, gap) in chars:
            try:
                raw = read_sectors(iso, t, start, gap)
                if raw[:4] != b"ldm\0":
                    raise ValueError("no model after cs%s.xtx" % base)
                csize = struct.unpack_from("<I", raw, 8)[0]
                cs = Ped(raw[:csize], "cs" + base)
                ig = Ped(read_sectors(iso, t, mdl["offset"], mdl["sectors"]), base)
                if cs.gameplay or not ig.gameplay:
                    raise ValueError("unexpected model kinds")
                data, info = combine(ig, cs)
                Ped(data, base + " (converted)")  # reads back
                open(os.path.join(out, base + ".mdl"), "wb").write(data)
                open(os.path.join(out, base + ".xtx"), "wb").write(read_sectors(iso, t, cstxd["offset"], cstxd["sectors"]))
                swings = ", ".join("%d:%.0f" % s for s in info["swings"]) or "none"
                print("%-8s %5d vertices%s, turned %.0f deg (fit %.3f m), limbs swung (bone:deg) %s; %d -> %d bytes" % (
                    base, info["vertices"], " (%d with bones past the skin)" % info["bad_weights"] if info["bad_weights"] else "",
                    abs(info["yaw"]) if abs(info["yaw"]) < 0.5 else info["yaw"], info["fit_error"], swings, info["was"], info["size"]))
                done += 1
            except (ValueError, KeyError, IndexError, ZeroDivisionError) as e:
                failed.append("%s (%s)" % (base, e))
    readme = os.path.join(out, "README.txt")
    open(readme, "w").write("Made by tools/csped.py from your own disc: the story characters' cutscene models on their\n"
                            "gameplay skeletons. Derived from the game's files - keep it on this PC.\n")
    print("%d characters -> %s" % (done, out))
    if failed:
        print("not converted: " + "; ".join(failed))
    tc.event(tool="csped", converted=done, failed=len(failed), out=out)
    return 0 if done else 1


def check(path):
    p = Ped(open(path, "rb").read(), os.path.basename(path))
    bones, verts = set(), 0
    for mi, b in p.batches():
        wa, n = b[0xC]
        verts += n
        for i in range(n):
            for r, w in zip(struct.unpack_from("<4I", p.m.d, wa + i * 16), struct.unpack_from("<4f", p.m.d, wa + i * 16)):
                if w > 0:
                    bones.add((r & 0x3FF) >> 2)
    print("%s: %s model, %d bones (%s), %d meshes, %d vertices in batches, weights on bones %s, scale %s, position %s" % (
        path, "gameplay" if p.gameplay else "cutscene", len(p.ids), " ".join(str(i) for i in p.ids), p.num_meshes, verts,
        sorted(bones), ["%.3f" % s for s in p.scale], ["%.3f" % s for s in p.pos]))
    return 0 if max(bones) < len(p.ids) else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub_ = ap.add_subparsers(dest="cmd", required=True)
    sub_.add_parser("list")
    b = sub_.add_parser("build")
    b.add_argument("names", nargs="*")
    b.add_argument("--out", default=os.path.join(tc.ROOT, "modloader", MOD_NAME))
    b.add_argument("--iso")
    c = sub_.add_parser("check")
    c.add_argument("file")
    a = ap.parse_args()
    if a.cmd == "list":
        for base, mdl, txd, cstxd, (start, gap) in characters(table_or_make()):
            print("%-8s gameplay %s.mdl %3d + %s.xtx %3d sectors; cutscene cs%s.xtx %3d + model %3d sectors" % (
                base, base, mdl["sectors"], base, txd["sectors"], base, cstxd["sectors"], gap))
        return 0
    if a.cmd == "build":
        return build(a.names, a.out, a.iso)
    return check(a.file)


if __name__ == "__main__":
    sys.exit(main())
