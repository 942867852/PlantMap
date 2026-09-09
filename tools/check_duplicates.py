#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PlantMap 数据去重检查工具（只读，不做任何修改）。

检查 bin/data/plantmap.json（或用 --file 指定其它文件）中的重复数据，逐类报告：
  1. 同级节点重名     —— 同一父节点下有两个同名子节点（分类树矛盾）
  2. 拉丁学名重复     —— 全库中两个（或更多）节点写了相同的学名
  3. 种级中文名重复   —— 全库中多个植物节点用了同一个中文名
  4. 别名重复/异常    —— 同植物别名清单内部重复、别名与其拉丁学名相同
  5. 照片文件重复引用 —— 同一照片被多棵植物引用（仅供参考，共享照片可能是有意的）

只提示，不修改任何数据。退出码：
  0 = 未发现重复
  1 = 发现重复
  2 = 文件无法读取/解析

用法:
    python tools/check_duplicates.py
    python tools/check_duplicates.py --file 其它数据.json
    python tools/check_duplicates.py --verbose     # 打印详细信息（默认只打印概要+分组）
"""

import argparse
import json
import os
import re
import sys

VALID_PLANT_RANKS = {"species", "subspecies", "variety", "form", "cultivar"}


def norm_name(s):
    """学名规范化：小写 + 合并连续空白 + 去首尾空白。"""
    return re.sub(r"\s+", " ", str(s).strip().lower())


def display_path(path):
    return " / ".join(path)


class Checker:
    def __init__(self):
        # 结果收集：key = 问题类型，value = 行数组
        self.issues = {
            "同级重名": [],
            "学名重复": [],
            "中文名重复": [],
            "别名异常": [],
            "照片重复引用": [],
        }

    def add(self, kind, line):
        self.issues.setdefault(kind, []).append(line)

    def run(self, db, verbose=False):
        roots = db.get("taxonomy", {}).get("roots", [])

        # 索引
        sibling_names = {}   # (parent_id, 名字) -> 首次出现描述
        sci_index = {}       # 规范化学名 -> [(节点描述, path)]
        name_index = {}      # 中文名 -> [(节点描述, path)]（仅植物级）
        photo_index = {}     # 照片文件名 -> [节点描述]

        def walk(node, path, parent_id):
            name = node.get("name", "")
            full_path = path + [name]
            desc = f"id={node.get('id')}「{name}」({node.get('rank')})"

            # 1) 同级重名
            key = (parent_id, name)
            if key in sibling_names:
                self.add("同级重名",
                         f"{display_path(full_path)} 与 {sibling_names[key]} 同属一个父级")
            else:
                sibling_names[key] = display_path(full_path)

            info = node.get("info")
            if info and isinstance(info, dict):
                # 2) 学名重复
                sci = norm_name(info.get("scientific_name", ""))
                if sci:
                    loc = f"{display_path(full_path)} [{sci}]"
                    sci_index.setdefault(sci, []).append((desc, loc))

                # 4a) 别名内部重复 / 别名等于学名
                aliases = info.get("aliases") or []
                seen_alias = set()
                for a in aliases:
                    a = str(a).strip()
                    if not a:
                        continue
                    if a in seen_alias:
                        self.add("别名异常",
                                 f"{display_path(full_path)} 别名「{a}」在别名清单中重复出现")
                    seen_alias.add(a)
                    if sci and norm_name(a) == sci:
                        self.add("别名异常",
                                 f"{display_path(full_path)} 的别名「{a}」与它的拉丁学名完全相同")

                # 5) 照片重复引用
                for ph in info.get("photos") or []:
                    if ph:
                        photo_index.setdefault(ph, []).append(
                            f"{display_path(full_path)}")

            # 3) 中文名重复（植物级）
            if node.get("rank") in VALID_PLANT_RANKS:
                nk = name
                if nk:
                    name_index.setdefault(nk, []).append((desc, display_path(full_path)))

            for c in node.get("children") or []:
                walk(c, full_path, node.get("id"))

        for r in roots:
            walk(r, [], None)

        # 汇总学名重复
        for sci, entries in sorted(sci_index.items()):
            if len(entries) > 1:
                first = entries[0]
                for desc, loc in entries[1:]:
                    self.add("学名重复",
                             f"{first[1]} 与 {loc}\n        学名「{sci}」被多个节点使用")

        # 汇总中文名重复（只报不同路径的，避免和同级重名重复）
        for name, entries in sorted(name_index.items()):
            if len(entries) > 1:
                paths = [e[1] for e in entries]
                # 去重路径后仍多于 1 才算跨位置重复
                if len(set(paths)) > 1:
                    self.add("中文名重复",
                             f"植物中文名「{name}」出现在 {len(entries)} 处：\n        "
                             + "\n        ".join(paths))

        # 汇总照片重复引用
        for ph, owners in sorted(photo_index.items()):
            if len(set(owners)) > 1:
                self.add("照片重复引用",
                         f"照片「{ph}」被 {len(set(owners))} 个植物引用：\n        "
                         + "\n        ".join(sorted(set(owners))))

    def report(self, verbose=False):
        order = ["同级重名", "学名重复", "中文名重复", "别名异常", "照片重复引用"]
        total = 0
        for kind in order:
            lines = self.issues.get(kind, [])
            if not lines:
                continue
            total += len(lines)
            print(f"\n【{kind}】共 {len(lines)} 条")
            shown = lines if verbose else lines[:20]
            for line in shown:
                print("  - " + line)
            if not verbose and len(lines) > 20:
                print(f"  … 其余 {len(lines) - 20} 条略（加 --verbose 查看全部）")
        return total


def main():
    parser = argparse.ArgumentParser(description="PlantMap 数据去重检查（只读）")
    parser.add_argument("--file", "-f", default=None,
                        help="数据文件路径（默认: 仓库内 bin/data/plantmap.json）")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="打印全部结果行（默认每类最多 20 条）")
    args = parser.parse_args()

    # 定位数据文件：显式 --file 优先；否则按仓库相对位置推断
    if args.file:
        path = args.file
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        repo_root = os.path.dirname(script_dir)  # tools/ 的上一级
        path = os.path.join(repo_root, "bin", "data", "plantmap.json")

    if not os.path.exists(path):
        print(f"找不到数据文件：{path}")
        print("可用 --file 指定其它路径。")
        return 2

    try:
        with open(path, encoding="utf-8-sig") as f:
            db = json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        print(f"无法读取/解析 {path}：{e}")
        return 2

    if "taxonomy" not in db or "roots" not in db.get("taxonomy", {}):
        print(f"{path} 不是有效的 PlantMap 数据文件（缺少 taxonomy.roots）。")
        return 2

    checker = Checker()
    checker.run(db, verbose=args.verbose)

    total = checker.report(verbose=args.verbose)

    if total == 0:
        print("\n未发现重复数据。✓")
        return 0
    print(f"\n共发现 {total} 条重复/异常，请人工核实（本工具未做任何修改）。")
    return 1


if __name__ == "__main__":
    sys.exit(main())
