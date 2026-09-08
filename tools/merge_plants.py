#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PlantMap 植物数据合并引擎。

用法:
    python merge_plants.py data_batch1.py data_batch2.py ... -o bin/data/plantmap.json

每个批次文件是一个 Python 模块，导出 `PLANTS` 列表，每项形如:
    {
      "name": "蒙古栎",                      # 中文名（种级）
      "scientific_name": "Quercus mongolica",# 拉丁学名
      "rank": "species",                     # 可省，默认 species
      "path": {                               # 分类路径（中文名逐级）
        "kingdom": "植物界",
        "phylum": "被子植物门",
        "class": "双子叶植物纲",
        "order": "壳斗目",
        "family": "壳斗科",
        "genus": "栎属",
      },
      # 可选 info 字段（不填的用安全默认值）
      "info": {
        "aliases": ["..."],
        "habit": "tree",            # tree|shrub|herb|vine|aquatic|succulent|fern|other
        "light": "full_sun",        # full_sun|half_sun|half_shade|shade
        "water": "moderate",        # dry|moderate|moist|aquatic
        "life_cycle": "perennial",  # annual|biennial|perennial
        "foliage": "deciduous",     # evergreen|semi_evergreen|deciduous
        "growth_rate": "medium",    # slow|medium|fast
        "height_min_cm": 2000, "height_max_cm": 3000,
        "temperature_min_c": -100, "temperature_max_c": -100,  # -100 = 未填
        "humidity_min_pct": 0, "humidity_max_pct": 0,
        "ph_min": 0, "ph_max": 0,
        "hardiness_zone_low": 0, "hardiness_zone_high": 0,
        "bloom_months": [5, 6], "fruit_months": [],
        "description": "...", "habitat": "...",
        "soil_types": [], "propagation_methods": [], "usage_tags": [],
        "native_regions": [], "custom": {},
      },
      "note": "内部备注，不写入",
    }

路径规则:
- kingdom/phylum/class/order/family/genus 任意层已存在则复用（按“父节点下同名”匹配），
  不存在则新建。若同级出现重名会合并到第一个节点。
- 只允许挂载植物的层级（rank in {species, subspecies, variety, form, cultivar}）。
- 同一父节点下中文名必须唯一；拉丁学名全库唯一（脚本会检查）。
"""

import json
import sys
import os

VALID_PLANT_RANKS = {"species", "subspecies", "variety", "form", "cultivar"}

KEY_ORDER = ["kingdom", "phylum", "class", "order", "family", "genus"]


def load_db(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def save_db(db, path):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(db, f, ensure_ascii=False, indent=1)
    os.replace(tmp, path)


def norm(s):
    return " ".join(str(s).strip().split())


class Doc:
    """封装对嵌套树的查找/创建。"""

    def __init__(self, db):
        self.db = db
        self.roots = db["taxonomy"]["roots"]
        self.next_id = db["taxonomy"]["next_id"]
        # 索引: (父id, 子名) -> 子节点id
        self.by_parent_name = {}
        self.species_names = {}   # 全库种级中文名 -> id
        self.sci_names = {}       # 小写学名 -> id
        self._index()

    def _index(self):
        def walk(node):
            pid = node["id"]
            for c in node.get("children", []):
                self.by_parent_name[(pid, c["name"])] = c["id"]
                if c.get("info") and c["info"].get("scientific_name"):
                    sci = c["info"]["scientific_name"].strip().lower()
                    self.sci_names.setdefault(sci, c["id"])
                    self.species_names.setdefault(c["name"], c["id"])
                walk(c)
        for r in self.roots:
            walk(r)

    def alloc(self):
        i = self.next_id
        self.next_id += 1
        return i

    def find_child(self, parent, name):
        nid = self.by_parent_name.get((parent["id"], name))
        if nid is not None:
            # 返回节点
            return self._find_node(self.roots, nid)
        return None

    def _find_node(self, roots, nid):
        def walk(node):
            if node["id"] == nid:
                return node
            for c in node.get("children", []):
                r = walk(c)
                if r:
                    return r
            return None
        for r in roots:
            n = walk(r)
            if n:
                return n
        return None

    def get_or_create(self, parent, name, rank):
        """在 parent 下找 name；没有则新建 rank 节点。返回 (node, created)。"""
        if parent is None:
            # 顶层 kingdom 查找
            for r in self.roots:
                if r["name"] == name:
                    return r, False
            nid = self.alloc()
            node = {"id": nid, "rank": rank, "name": name, "children": []}
            self.roots.append(node)
            self.by_parent_name[(0, name)] = nid
            return node, True
        existing = self.find_child(parent, name)
        if existing is not None:
            return existing, False
        nid = self.alloc()
        node = {"id": nid, "rank": rank, "name": name, "children": []}
        parent.setdefault("children", []).append(node)
        self.by_parent_name[(parent["id"], name)] = nid
        return node, True


def default_info():
    return {
        "aliases": [], "bloom_months": [], "custom": {},
        "description": "", "foliage": "unknown", "fruit_months": [],
        "growth_rate": "unknown", "habit": "unknown", "habitat": "",
        "hardiness_zone_high": 0, "hardiness_zone_low": 0,
        "height_max_cm": 0, "height_min_cm": 0,
        "humidity_max_pct": 0, "humidity_min_pct": 0,
        "life_cycle": "unknown", "light": "unknown",
        "native_regions": [], "ph_max": 0, "ph_min": 0,
        "photos": [], "propagation_methods": [],
        "scientific_name": "", "soil_types": [],
        "spread_max_cm": 0, "spread_min_cm": 0,
        "temperature_max_c": -100, "temperature_min_c": -100,
        "usage_tags": [], "water": "unknown",
    }


def add_plant(doc, item, stats):
    name = norm(item["name"])
    sci = norm(item.get("scientific_name", ""))
    path = item.get("path", {})
    rank = item.get("rank", "species")
    if rank not in VALID_PLANT_RANKS:
        raise ValueError(f"非法等级 {rank} 用于 {name}")

    if not name:
        raise ValueError("植物中文名为空")
    if not sci:
        raise ValueError(f"{name} 缺少拉丁学名")
    sci_l = sci.lower()
    if sci_l in doc.sci_names:
        other = doc.sci_names[sci_l]
        stats["dup_sci"].append(f"{name} 学名冲突: {sci} == id{other}")
        return
    if name in doc.species_names:
        stats["dup_name"].append(f"{name} 中文名冲突: id{doc.species_names[name]}")
        return

    # 逐级创建分类路径
    parent = None
    created_any = False
    for key in KEY_ORDER:
        nm = norm(path.get(key, ""))
        if not nm:
            raise ValueError(f"{name} 的 {key} 路径缺失")
        rank_map = {"kingdom": "kingdom", "phylum": "phylum", "class": "class",
                    "order": "order", "family": "family", "genus": "genus"}
        node, created = doc.get_or_create(parent, nm, rank_map[key])
        if created:
            created_any = True
        parent = node

    # 建物种节点
    nid = doc.alloc()
    info = default_info()
    info.update(item.get("info", {}))
    info["scientific_name"] = sci
    node = {"id": nid, "rank": rank, "name": name, "info": info, "children": []}
    parent.setdefault("children", []).append(node)
    doc.by_parent_name[(parent["id"], name)] = nid
    doc.sci_names[sci_l] = nid
    doc.species_names[name] = nid
    stats["added"] += 1
    if created_any:
        stats["new_path"] += 1


def main():
    args = sys.argv[1:]
    out_path = None
    batch_files = []
    i = 0
    while i < len(args):
        if args[i] == "-o" and i + 1 < len(args):
            out_path = args[i + 1]
            i += 2
        else:
            batch_files.append(args[i])
            i += 1
    if not out_path or not batch_files:
        print(__doc__)
        sys.exit(2)

    db = load_db(out_path)
    doc = Doc(db)
    stats = {"added": 0, "dup_sci": [], "dup_name": [], "new_path": 0}

    for bf in batch_files:
        ns = {}
        code = compile(open(bf, encoding="utf-8").read(), bf, "exec")
        exec(code, ns)
        plants = ns["PLANTS"]
        print(f"批次 {os.path.basename(bf)}: {len(plants)} 条")
        for item in plants:
            add_plant(doc, item, stats)

    db["taxonomy"]["next_id"] = doc.next_id
    save_db(db, out_path)

    print("=== 结果 ===")
    print(f"新增植物: {stats['added']}")
    print(f"新建分类路径(含重复合并): {stats['new_path']}")
    if stats["dup_name"]:
        print(f"跳过重名 {len(stats['dup_name'])}:")
        for s in stats["dup_name"]:
            print("  " + s)
    if stats["dup_sci"]:
        print(f"跳过学名冲突 {len(stats['dup_sci'])}:")
        for s in stats["dup_sci"]:
            print("  " + s)


if __name__ == "__main__":
    main()
