# AndroidApp 构建说明

当前仓库已移除二进制文件（`gradle-wrapper.jar` 与默认 `webp` 启动图标），用于兼容不支持二进制差异的代码托管/PR环境。

## 本地构建
1. 使用本机已安装的 Gradle 直接构建：
   ```bash
   gradle :app:assembleDebug
   ```
2. 或在 Android Studio 打开项目后点击 Sync（IDE 会自动处理缺失的 wrapper 二进制）。

## 启动图标
当前采用 `mipmap-anydpi-v26` 下的 XML 自适应图标。
如需恢复多分辨率位图图标，可自行放回：
- `app/src/main/res/mipmap-*/ic_launcher.webp`
- `app/src/main/res/mipmap-*/ic_launcher_round.webp`
