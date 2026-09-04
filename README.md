# PlantMap 植物图谱

基于 Qt 6 Widgets 的桌面植物图谱软件。数据保存在 JSON 文件中，采用
`界 → 门 → 纲 → 目 → 科 → 属 → 种 → 亚种` 的完整层级分类。

## 构建

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=D:/Qt/6.10.1/mingw_64 \
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build
```

## 运行

```bash
./build/PlantMap.exe --data-dir <数据目录>
```

不传 `--data-dir` 时默认写入源码目录下的 `data/`（开发期方便查看）。

## 自检

```bash
./build/plantmap_selftest.exe
```

自检覆盖：分类链逐级插入、拉丁学名全局查重、节点删除、JSON 往返一致性。

## 目录结构

```
src/core/        数据模型（纯 QtCore，无界面依赖）
src/ui/          Qt Widgets 界面
tests/           命令行自检
data/            运行时数据（JSON + 照片），可随时清空重建
```

详细设计见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。
