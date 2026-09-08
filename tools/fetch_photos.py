#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PlantMap 植物配图抓取工具（iNaturalist 图源）。

为 plantmap.json 中"还没有照片"的植物，从 iNaturalist 按拉丁学名搜索真实照片，
下载到 data/photos/ 并注册到 JSON 的 photos 字段。每种植物 1 张。

命名规则沿用项目现有格式：node{id}_{毫秒时间戳}_{植物名}.jpg

搜索策略（依次降级，提高命中率与质量）：
  1. taxa API 查"该物种的 default_photo"（iNaturalist 社区默认代表图，质量最好）
  2. 观察 API 按 votes 排序的研究级照片（多张候选，取第一张成功的）
  3. 去掉变种/品种后缀，用"基础种学名"再查 1 和 2
  4. 用中文名再查 2（极少数情况能命中）

用法:
    python tools/fetch_photos.py                 # 为所有缺图植物抓图
    python tools/fetch_photos.py --limit 8       # 先试 8 种（建议先小批试效果）
    python tools/fetch_photos.py --only 红松,白桦  # 只处理指定中文名（逗号分隔）
    python tools/fetch_photos.py --dry-run       # 只报告能搜到多少，不下载不写盘
    python tools/fetch_photos.py --delay 1.0     # 每次请求间隔秒（默认 0.8）

输出:
    结束打印 成功/失败 统计；失败清单写入 tools/fetch_photos_failed.txt。
"""

import argparse
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request

UA = "PlantMap/1.0 (plant database)"
OBS_API = "https://api.inaturalist.org/v1/observations"
TAX_API = "https://api.inaturalist.org/v1/taxa"

BAD_CHARS = r'[\\/:*?"<>|\r\n\t]'


def sanitize(s):
    s = re.sub(BAD_CHARS, "", str(s)).strip()
    return s or "plant"


def strip_infraspecific(sci):
    """去掉变种/亚种/变型/品种后缀，得到基础种学名。"""
    s = sci.strip()
    s = re.sub(r"'[^']*'", "", s)
    s = re.sub(r'"[^"]*"', "", s)
    s = re.sub(r"\s+(var|subsp|f|cv)\.?\s+.*$", "", s, flags=re.IGNORECASE)
    s = re.sub(r"\s+cv\.?\s*$", "", s, flags=re.IGNORECASE)
    return " ".join(s.split())


def http_get_json(url, timeout=30):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8"))


def upgrade_to_large(url):
    """把 iNaturalist 的 square/small/medium url 换成 large，提高清晰度。"""
    return re.sub(r"/(square|small|medium|large|original)\.(jpe?g|png)$",
                  r"/large.\2", url)


def lookup_taxon_default_photo(name):
    """taxa API 找物种，返回 (medium_url, taxon_name)。优先精确匹配。"""
    url = TAX_API + "?" + urllib.parse.urlencode({"q": name, "per_page": "5"})
    try:
        d = http_get_json(url)
    except Exception:
        return None, None
    name_l = name.strip().lower()
    for t in d.get("results", []):
        if t.get("name", "").strip().lower() == name_l:
            dp = t.get("default_photo")
            if dp and dp.get("medium_url"):
                return dp["medium_url"], t.get("name")
    for t in d.get("results", []):
        if t.get("rank") == "species":
            dp = t.get("default_photo")
            if dp and dp.get("medium_url"):
                return dp["medium_url"], t.get("name")
    return None, None


def api_search_obs(name, per_page=8):
    """观察 API 按 votes 取研究级候选 URL。"""
    params = {
        "taxon_name": name,
        "photos": "true",
        "per_page": str(per_page),
        "order_by": "votes",
        "quality_grade": "research",
    }
    url = OBS_API + "?" + urllib.parse.urlencode(params)
    try:
        d = http_get_json(url)
    except Exception:
        return []
    urls = []
    for obs in d.get("results", []):
        for p in obs.get("photos", []):
            u = p.get("url")
            if not u:
                continue
            urls.append(upgrade_to_large(u))
    return urls


def download(url, dest, timeout=45):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        data = r.read()
    if len(data) < 2000:
        return None
    with open(dest, "wb") as f:
        f.write(data)
    return len(data)


def collect_no_photo_plants(db_path):
    db = json.load(open(db_path, encoding="utf-8-sig"))
    plants = []

    def walk(node, path):
        p = path + [node.get("name", "")]
        info = node.get("info")
        if info and info.get("scientific_name"):
            if not (info.get("photos") or []):
                plants.append({
                    "id": node.get("id"),
                    "name": node.get("name", ""),
                    "sci": info["scientific_name"],
                    "path": " / ".join(p),
                })
        for c in node.get("children") or []:
            walk(c, p)

    for r in db.get("taxonomy", {}).get("roots", []):
        walk(r, [])
    return db, plants


def attach_photo(db, plant, fname):
    def walk(node):
        if (node.get("id") == plant["id"]
                and node.get("info")
                and node.get("info").get("scientific_name") == plant["sci"]):
            node["info"].setdefault("photos", []).append(fname)
            return True
        for c in node.get("children") or []:
            if walk(c):
                return True
        return False

    for r in db["taxonomy"]["roots"]:
        if walk(r):
            return True
    return False


def main():
    ap = argparse.ArgumentParser(description="PlantMap 植物配图抓取（iNaturalist）")
    ap.add_argument("--file", default=None)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--only", default="")
    ap.add_argument("--delay", type=float, default=0.8)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    if args.file:
        db_path = args.file
    else:
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        db_path = os.path.join(repo_root, "bin", "data", "plantmap.json")
    data_dir = os.path.join(os.path.dirname(db_path), "photos")
    os.makedirs(data_dir, exist_ok=True)

    db, plants = collect_no_photo_plants(db_path)
    if args.only:
        wanted = set(x.strip() for x in args.only.split(",") if x.strip())
        plants = [p for p in plants if p["name"] in wanted]
    if args.limit:
        plants = plants[:args.limit]

    print(f"待配图植物：{len(plants)} 种" + ("（dry-run，不下载）" if args.dry_run else ""))
    print("-" * 64)

    ok, skipped, failed = [], [], []
    dirty = False

    for i, pl in enumerate(plants, 1):
        sci = pl["sci"]
        candidates = [sci]
        base = strip_infraspecific(sci)
        if base and base.lower() != sci.lower():
            candidates.append(base)
        candidates.append(pl["name"])

        url, source = None, None
        # 1) taxa default_photo（按物种精确或基础种学名）
        for q in [sci, base] if base else [sci]:
            if not q:
                continue
            u, taxon = lookup_taxon_default_photo(q)
            if u:
                url, source = u, f"taxa:{taxon}"
                break
            time.sleep(args.delay)
        # 2) 观察 API（按完整/基础/中文名）
        if not url:
            cands = []
            for q in candidates:
                if q:
                    cands += api_search_obs(q)
            if cands:
                url, source = cands[0], "obs"

        if not url:
            skipped.append(pl)
            print(f"[{i}/{len(plants)}] ✗ 搜不到  {pl['name']} ({sci})")
            continue

        if args.dry_run:
            ok.append(pl)
            print(f"[{i}/{len(plants)}] ✓ 候选   {pl['name']} <- {source}")
            time.sleep(args.delay * 0.2)
            continue

        ts = int(time.time() * 1000)
        fname = f"node{pl['id']}_{ts}_{sanitize(pl['name'])}.jpg"
        dest = os.path.join(data_dir, fname)

        # 优先下载 large / medium；失败时逐张 fallback
        success = False
        if source.startswith("obs"):
            # obs 候选已是 large；若无候选 large，下载 medium
            try:
                size = download(url, dest)
                if size:
                    success = True
            except Exception:
                pass
        else:
            # taxa default 优先 medium_url（已是 medium），可尝试 large
            for u in [upgrade_to_large(url), url]:
                try:
                    size = download(u, dest)
                    if size:
                        success = True
                        break
                except Exception:
                    continue

        if not success:
            failed.append(pl)
            print(f"[{i}/{len(plants)}] ✗ 下载失败 {pl['name']}")
            continue

        if attach_photo(db, pl, fname):
            ok.append(pl)
            dirty = True
            sz = os.path.getsize(dest) // 1024
            print(f"[{i}/{len(plants)}] ✓ {pl['name']} <- {source} [{sz} KB]")
        else:
            failed.append(pl)
            print(f"[{i}/{len(plants)}] ✗ 写回失败 {pl['name']}")

        if dirty and i % 20 == 0:
            tmp = db_path + ".tmp"
            json.dump(db, open(tmp, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
            os.replace(tmp, db_path)

        time.sleep(args.delay)

    if dirty and not args.dry_run:
        tmp = db_path + ".tmp"
        json.dump(db, open(tmp, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        os.replace(tmp, db_path)

    print("-" * 64)
    print(f"成功配图：{len(ok)}")
    print(f"搜不到图：{len(skipped)}")
    print(f"下载/写回失败：{len(failed)}")

    if (skipped or failed) and not args.dry_run:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "fetch_photos_failed.txt")
        with open(out, "w", encoding="utf-8") as f:
            for p in skipped + failed:
                f.write(f"{p['id']}\t{p['name']}\t{p['sci']}\t{p['path']}\n")
        print(f"缺图清单已写入：{out}（可人工补图）")


if __name__ == "__main__":
    sys.exit(main())