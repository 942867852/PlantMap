---
name: plantmap-release
description: PlantMap（Qt 6 / Windows，D:\QtProject\PlantMap）的版本发布流程：版本号对齐、README 与更新说明、自检构建、打包交付，以及用户授权后的 git 提交/tag/推送。适用于“发布新版 / 升级到 vX.Y / 写更新说明 / 提交并打 tag”；未收到明确指示不做 git 或发布操作。
agent_created: true
---

# PlantMap 版本发布流程

适用于用户要求发布新版本、升级版本号或生成发布说明。

## 项目红线

- 仓库路径：`D:\QtProject\PlantMap`；当前 shell 为 PowerShell。
- 未收到用户明确指示前，**不执行** git 提交/推送/tag、不修改远端、不上传或发布任何文件。
- `bin/`、`build/`、`*.rar` 均不入 git；不要移动、删除或改名用户已打包的发布压缩包。
- 用户要求讲解时，每次 git 操作后简要说明命令的作用与结果。

## 版本号来源

- 唯一源码入口：`CMakeLists.txt` 中 `project(PlantMap VERSION x.y LANGUAGES CXX)`。
- 编译宏 `PLANTMAP_VERSION` 会注入窗口标题与 `QApplication` 版本，无需改动 main.cpp / mainwindow.cpp。
- README、RELEASE_NOTES 里的 `v0.x` 是纯文本，需要同步搜索更新。

## 标准流程

1. 与用户确认目标版本号（例如 v0.3）。
2. 修改 `CMakeLists.txt` 的 `VERSION`。
3. 更新 README.md：
   - 顶部“当前版本”与新版 RELEASE_NOTES 的链接；
   - “更新记录”顶部新增一行；功能列表有变化时同步修改。
4. 新建 `RELEASE_NOTES_vX.Y.md`，沿用 v0.2 的结构：新增功能 / 体验改进 / 问题修复 / 数据说明。
5. 全仓库搜索 `rg -n "v0\.[0-9]|0\.[0-9]"`，找出过期的纯文本版本号并更新（不要误改数据库 `schema_version`，它不是软件版本）。
6. 构建并自检：

```powershell
$env:Path='D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\CMake_64\bin;'+$env:Path
cmake --build build\Desktop_Qt_6_10_1_MinGW_64_bit-Debug --target PlantMap plantmap_selftest
bin\plantmap_selftest.exe
```

若该构建目录不存在，先 `Get-ChildItem build` 找实际目录名。

7. 交付：提示用户打开 `bin\PlantMap.exe` 检查版本号与改动效果。
8. git（仅在用户明确指示后）：
   - 先 `git status` 确认变更范围，并向用户展示计划；
   - 提交并推送到 `main`；
   - 创建注解 tag：`git tag -a vX.Y -m "PlantMap vX.Y"`，再推送 tag；
   - 向用户解释：commit 是快照，tag 给发布版本一个固定名字，推送 tag 后 GitHub Releases 才能引用它。

## 打包与上传

- exe 输出在 `bin\`，Qt 运行库已由 windeployqt 部署；携带数据库需复制整个 `bin` 文件夹。
- 压缩发布包（如 PlantMap.rar）由用户本机完成，或仅在用户明确授权时代为压缩。
- GitHub Releases 的创建、上传、发布是外部操作，必须等用户明确同意；若用户不愿自动操作浏览器，就把可执行的步骤和文案交给用户手动完成。
