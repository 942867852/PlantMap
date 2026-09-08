---
name: plantmap-release
description: PlantMap（Qt 6 / Windows，D:\QtProject\PlantMap）的版本发布流程：版本号对齐、README 与更新说明、自检构建、打包交付（bin 打包 zip 并上传 GitHub Release，含创建 Release/上传资产），以及用户授权后的 git 提交/tag/推送。适用于“发布新版 / 升级到 vX.Y / 写更新说明 / 提交并打 tag / 打包上传 release”；未收到明确指示不做 git 或发布操作。
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

## 打包与上传 GitHub Release（bin 文件夹 → 该次 release）

适用：用户要求"把 bin 打包上传 / 发到 Release"。tag 已推送后执行。bin 的 exe 需为
本次构建产物；`plantmap_selftest.exe` 是开发测试程序，打包时排除。

### 1. 打包 bin 为 zip（用 Python，排除 selftest）

```python
# 在仓库根目录运行
python -c "
import zipfile, os
src='bin'; out='PlantMap_v0.3.2_win64.zip'; exclude={'plantmap_selftest.exe'}
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    for root,dirs,files in os.walk(src):
        for f in files:
            if f in exclude: continue
            full=os.path.join(root,f); z.write(full, os.path.relpath(full,src))
print(out, round(os.path.getsize(out)/1048576,2),'MB')
"
```

### 2. 获取 GitHub 凭据（无 gh CLI 时）

用户机器的 GitHub token 缓存在 Git Credential Manager：

```bash
printf "protocol=https\nhost=github.com\n\n" | "/c/Program Files/Git/bin/git.exe" credential fill
# 输出 password=gho_... 即为 OAuth token（gho_ 开头）
```

注意：token 属敏感信息，用完不要写入任何文件、不输出到日志。

### 3. 创建 Release 并上传（GitHub API）

```bash
TOKEN="<上一步的 gho_ token>"
# 3a. 创建 Release（关联已推送的 tag vX.Y；幂等——已存在会报错，可忽略或先删旧 release）
curl -s -X POST "https://api.github.com/repos/942867852/PlantMap/releases" \
  -H "Authorization: token $TOKEN" -H "Accept: application/vnd.github+json" \
  -H "Content-Type: application/json" \
  -d '{"tag_name":"vX.Y","name":"PlantMap vX.Y","body":"<release notes 摘要>","draft":false,"prerelease":false}'
# 返回里记下 "id"（release id）与 "html_url"

# 3b. 上传 zip（upload_url 模板替换 {?name,label}）
curl -s -X POST "https://uploads.github.com/repos/942867852/PlantMap/releases/<release_id>/assets?name=PlantMap_vX.Y_win64.zip" \
  -H "Authorization: token $TOKEN" -H "Accept: application/vnd.github+json" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@PlantMap_vX.Y_win64.zip"
# 返回 "state":"uploaded" 即成功，browser_download_url 为下载地址
```

### 4. 收尾

- 删除本地临时 zip（不进 git）。
- 若旧 release 已存在需覆盖：用 `GET /releases/tags/vX.Y` 拿旧 release id 和
  assets id，`DELETE /releases/assets/<id>` 删旧资产、`DELETE /releases/<id>` 删旧
  release 后重建，或 `PATCH /releases/<id>` 更新正文。
- 向用户报告：release 页面地址 + 下载链接 + zip 大小。

### 版本号小版本约定

- `project(PlantMap VERSION x.y.z)` 支持三位；纯文本（README/RELEASE_NOTES）同步 `vX.Y.Z`。
- GitHub 打包文件名用 `PlantMap_vX.Y.Z_win64.zip` 三位完整版本，避免与旧版混淆。

## 交付与手动兜底

- exe 输出在 `bin\`，Qt 运行库已由 windeployqt 部署；携带数据库需复制整个 `bin` 文件夹。
- 压缩发布包（如 PlantMap.rar）由用户本机完成，或仅在用户明确授权时代为压缩。
- 若用户不愿走 API/没有可用 token，就把上述可执行步骤和文案交给用户，或引导用户在
  浏览器 Release 页面手动上传 zip。
