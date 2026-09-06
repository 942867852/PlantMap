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
./bin/PlantMap.exe --data-dir <数据目录>
```

编译后的可执行文件统一输出到项目根目录的 `bin/`，并已自动带上 Qt 运行库，
可以直接双击 `bin/PlantMap.exe` 查看。不传 `--data-dir` 时，程序在可执行文件
同级目录的 `bin/data/` 下读写数据库（JSON + 照片）。把整个 `bin/` 文件夹复制到
另一台电脑即可连同数据库一起运行。

## 自检

```bash
./bin/plantmap_selftest.exe
```

自检覆盖：分类链逐级插入、拉丁学名全局查重、节点删除、JSON 往返一致性。

## 目录结构

```
src/core/        数据模型（纯 QtCore，无界面依赖）
src/ui/          Qt Widgets 界面（只读详情页 + 编辑窗口）
tests/           命令行自检
bin/data/        运行时数据库（JSON + 照片），与 exe 放在一起
```

详细设计见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。
