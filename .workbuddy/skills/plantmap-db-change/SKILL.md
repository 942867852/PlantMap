---
name: plantmap-db-change
description: PlantMap 植物数据存储格式变更流程：数据模型、toJson/fromJson、dbversion 链式迁移、展示/编辑/检索界面必须同步并补自检。适用于修改 SpeciesInfo、plantmap.json 结构、schema_version、别名或照片等数据字段的任务。
agent_created: true
---

# PlantMap 数据库格式变更流程

适用：新增/删除植物属性、改字段名或结构、改别名存储方式、改照片保存方式。先读仓库 `docs/ARCHITECTURE.md`（“数据格式变更流程”一节）再动手；若文档与代码不一致，以代码为准并把差异回报给用户。

## 硬性不变式

- 迁移函数一旦发布，**只增不改**：新格式变更只能新增 `migrateV{N}ToV{N+1}`，在 `migrateToLatest()` 中接入，并把 `DbVersion::kCurrentSchemaVersion` 提高到 N+1。
- `loadFromFile` 保持“先迁移、再解析”（当前在 taxondocument.cpp），不能绕过迁移直接 fromJson。
- JSON 往返必须无损：`toJson → fromJson → toJson` 结果一致（允许已声明的规范化，如数值舍入）。
- 导入数据库版本高于当前软件版本时拒绝；低于或等于当前版本时先迁移再使用。

## 必须同步的 4 处

1. **数据模型**：`src/core/speciesinfo.h/.cpp`（或 `TaxonNode`）增删字段。
2. **序列化**：同步 `toJson` 与 `fromJson`；旧文件缺新字段时给安全默认值（“空/未填写”，不要伪造有效数值）。
3. **迁移**：在 `src/core/dbversion.cpp` 新增迁移函数并提高 `dbversion.h` 中 `kCurrentSchemaVersion`。
4. **界面**：
   - `speciesview`：只读展示新属性；
   - `speciesform`：编辑页新增/删除对应控件；新植物页面不得预填有效默认值；
   - 若属性参与检索，同步高级检索条件（保持 AND 精确匹配）。

## 本项目既定格式规则

- 别名是数组里的独立条目（例如 `["玫瑰花","徘徊花"]`），不是用逗号拼接的单个字符串。
- 照片 JSON 只保存相对文件名，实体文件在数据目录 `photos/` 下。
- 月份字段显示规则：连续段区间（`2月-4月`）、跨年（`12月-次年2月`）、找不到连续段起点（典型为全选）时输出 `全年` 或 `1月-12月`。改存储结构不得破坏这些约定。
- pH 等数值字段：落库与展示统一保留 1 位小数；未保存检测按 0.1 精度比较，避免“没改却提示未保存”。
- 只有单端有值的范围显示 `最低 xx` / `最高 xx`；两端都没有则不显示。
- 凡是会写入 JSON 的字段改动，检查展示、编辑、自检三处是否都有对应行为。

## 自检要求

- 在 `tests/selftest.cpp` 增加迁移自检：构造旧版本样例 → 迁移到最新 → 与“直接按新格式保存”的结果一致；旧样例不应随新格式更新。
- 构建并运行自检：

```powershell
$env:Path='D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\CMake_64\bin;'+$env:Path
cmake --build build\Desktop_Qt_6_10_1_MinGW_64_bit-Debug --target PlantMap plantmap_selftest
bin\plantmap_selftest.exe
```

## 完成后

向用户报告：schema 版本从几升到几、涉及哪些文件、迁移兼容性结论，以及是否影响旧数据库。
