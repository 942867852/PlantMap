# PlantMap 架构说明

## 设计目标

1. 完整、可校验的植物分类层级：界 → 门 → 纲 → 目 → 科 → 属 → 种，
   种之下并列挂“亚种 / 变种 / 变型 / 品种”四个末级。
2. 每个种及种下等级拥有独立的资料页（习性、环境需求、生长形态、照片、地理分布、物候等）。
3. 拉丁学名全库唯一，用哈希表索引，新增/修改时立即查重。
4. 数据与界面分离：核心层只用 QtCore，便于测试和未来扩展搜索、导出等。
5. 预留扩展空间：等级枚举、属性结构、自定义属性表。

## 层级与数据挂载规则

```
界 Kingdom → 门 Phylum → 纲 Class → 目 Order → 科 Family
  → 属 Genus → 种 Species → { 亚种 Subspecies / 变种 Variety /
                               变型 Form / 品种 Cultivar }  （并列末级）
```

- 新建“下级分类”时程序自动推导下一级，禁止跳级、倒挂。
- “种”的默认下级为“亚种”；新增“变种 / 变型 / 品种”需走
  `addNode(parentId, name, rank)` 显式等级重载。
- 亚种/变种/变型/品种都是末级，不能再分。
- 每个节点的名称在**同一父节点下唯一**。
- 只有“种”及种下的四个末级可以承载资料（描述、照片、环境需求等）。
- 拉丁学名规范化后（小写、去空白和标点）在全库唯一，由
  `QHash<QString,int>` 维护 `规范名 → 节点 id`。

## 核心类

| 文件 | 职责 |
| --- | --- |
| `dbversion.h/.cpp` | 数据库 schema 版本识别与链式迁移（1→2→3→4） |
| `taxonrank.h/.cpp` | 等级枚举、下一级推导、中文名、末级判定 |
| `speciesinfo.h/.cpp` | 物种资料：环境需求、生长形态、物候、地理分布、照片、扩展属性 |
| `taxondocument.h/.cpp` | 整棵分类树 + 资料 + 拉丁名索引 + JSON 持久化 + 子树克隆/恢复 |
| `displayformat.h/.cpp` | 月份区间、数值范围、一位小数等纯格式化 |
| `csvexport.h/.cpp` | CSV 导出（固定列 + 属性列自动推导） |
| `pinyin.h` / `pinyin_first.h` | 中文名拼音首字母检索（输入 `yx` 匹配“银杏”） |
| `mainwindow.h/.cpp` | 主窗口、分类树、搜索、增删改、撤销重做、收藏夹、批量操作、导入导出入口 |
| `speciesview.h/.cpp` | 物种资料只读详情页（图片 + 全部属性 + 地图） |
| `speciesform.h/.cpp` | 物种资料编辑表单（独立编辑窗口 + 草稿自动保存） |
| `specieseditdialog.h/.cpp` | 资料编辑对话框（保存/取消 + 回滚 + 孤儿照片清理） |
| `undomanager.h/.cpp` | 撤销/重做命令栈（AddNode/RemoveNode/RenameNode/SetInfo） |
| `favoritesstore.h/.cpp` | 收藏夹存储（favorites.json 持久化） |
| `statsdialog.h/.cpp` | 统计面板（总览/等级分布/习性分布/省区分布） |
| `comparedialog.h/.cpp` | 物种属性对比表格 |
| `province_map_widget.h/.cpp` | 中国省级行政区示意图控件（点击多选 / 只读展示） |
| `china_map_data.h` | 省界坐标数据（含港澳台、南海诸岛、十段线） |
| `photolightbox.h/.cpp` | 全屏照片灯箱（前后翻页 / Esc 关闭） |
| `photoimageutils.h/.cpp` | 照片按需缩放加载 + 缩略图缓存 |
| `wheelignorefilter.h` | 拦截下拉/数值控件的滚轮误改 |

## 数据文件

`plantmap.json` 结构：

```json
{
  "app": "PlantMap",
  "schema_version": 4,
  "taxonomy": {
    "next_id": 12,
    "roots": [ { "id": 1, "rank": "kingdom", "name": "植物界", "children": [...] } ]
  }
}
```

节点 JSON：

```json
{
  "id": 7,
  "rank": "species",
  "name": "玫瑰",
  "info": {
    "scientific_name": "Rosa rugosa",
    "native_regions": ["410000"],
    "habitat": "山地灌丛",
    "...": "..."
  },
  "children": [ ]
}
```

照片文件复制到 `<数据目录>/photos/`，JSON 只保存相对文件名。
默认数据目录是 `bin/data/`（可执行文件同级目录），把整个 `bin/` 文件夹复制到
另一台电脑即可连同数据库一起运行；也可用 `--data-dir` 显式指定。

## 数据格式变更流程（必须同步清单）

每次修改植物数据存储格式（新增属性、删除属性、调整字段名或结构）时，
必须同步完成以下 4 步，任何一步都不能遗漏：

1. **修改数据模型**：在 `SpeciesInfo`（或 `TaxonNode`）中增删字段；
2. **修改序列化**：同步更新 `speciesinfo.cpp` / `taxondocument.cpp` 中的
   `toJson` 与 `fromJson`，旧字段缺失时给出安全的默认值；
3. **增加迁移规则**：在 `dbversion.cpp` 中新增一个版本迁移函数（例如
   `migrateV4ToV5`），并让 `migrateToLatest()` 支持该版本，同时提高
   `DbVersion::kCurrentSchemaVersion`；
4. **同步界面**：根据新字段更新只读详情页（`speciesview`）和编辑页
   （`speciesform`）；如果该属性会影响检索，还要同步高级检索条件。

每完成一次格式变更，应在 `tests/selftest.cpp` 中补充对应的迁移自检：
旧版本样例可以迁移到新版本，且迁移后数据与直接保存新版本一致。

## 撤销 / 重做

分类树的新增、删除、重命名与资料编辑均可撤销重做（Ctrl+Z / Ctrl+Y）。
命令类型定义在 `mainwindow.h` 的 `UndoCommand`（AddNode / RemoveNode / RenameNode /
SetInfo），由 `MainWindow` 的双栈（`m_undoStack` / `m_redoStack`）管理，栈深上限 100。
删除节点的撤销需要恢复子树 JSON 与被删照片文件字节，故一并保存在命令里。

## 未来扩展点

- 等级：如需要更细等级，扩展 `TaxonRank` 枚举及映射表、`subRanks()` 即可。
- 属性：按上方“数据格式变更流程”增加字段；通用键值放 `custom` 表。
- 检索：`TaxonomyDocument` 已可遍历全部节点，可加花期/光照筛选。
- 导入导出：CSV/Excel 可在 core 层加工具，与 UI 解耦。
