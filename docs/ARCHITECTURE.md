# PlantMap 架构说明

## 设计目标

1. 完整、可校验的植物分类层级，最低一级为“亚种”。
2. 每个种 / 亚种拥有独立的资料页（习性、环境需求、生长形态、照片、描述）。
3. 拉丁学名全库唯一，用哈希表索引，新增/修改时立即查重。
4. 数据与界面分离：核心层只用 QtCore，便于测试和未来扩展搜索、导出等。
5. 预留扩展空间：等级枚举、属性结构、自定义属性表。

## 层级与数据挂载规则

```
界 Kingdom → 门 Phylum → 纲 Class → 目 Order → 科 Family
  → 属 Genus → 种 Species → 亚种 Subspecies
```

- 新建“下级分类”时程序自动推导下一级，禁止跳级、倒挂。
- 每个节点的名称在**同一父节点下唯一**。
- 只有“种”和“亚种”可以承载资料（描述、照片、环境需求等）。
- 拉丁学名规范化后（小写、去空白和标点）在全库唯一，由
  `QHash<QString,int>` 维护 `规范名 → 节点 id`。

## 核心类

| 文件 | 职责 |
| --- | --- |
| `taxonrank.h/.cpp` | 等级枚举、下一级推导、中文名 |
| `speciesinfo.h/.cpp` | 物种资料：环境需求、生长形态、物候、照片、扩展属性 |
| `taxondocument.h/.cpp` | 整棵分类树 + 资料 + 拉丁名索引 + JSON 持久化 |
| `mainwindow.h/.cpp` | 主窗口、分类树导航、增删改 |
| `speciesform.h/.cpp` | 物种资料编辑表单 |

## 数据文件

`plantmap.json` 结构：

```json
{
  "app": "PlantMap",
  "version": 1,
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
  "info": { "scientific_name": "Rosa rugosa", "...": "..." },
  "children": [ ]
}
```

照片文件复制到 `<数据目录>/photos/`，JSON 只保存相对文件名。

## 未来扩展点

- 等级：如需要“变种/变型/品种”，扩展 `TaxonRank` 枚举及映射表即可。
- 属性：在 `SpeciesInfo` 增加字段并同步 JSON；通用键值放 `custom` 表。
- 检索：`TaxonomyDocument` 已可遍历全部节点，可加花期/光照筛选。
- 导入导出：CSV/Excel 可在 core 层加工具，与 UI 解耦。
