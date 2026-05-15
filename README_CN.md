[English](README.md) | **简体中文**

# Seelie 桌面宠物

一款基于 Qt6/C++ 的原生桌面宠物，能够对 AI 编码工具的事件做出反应。轻量级（空闲时 < 10MB 内存）原生应用，搭载可扩展的 sprite pack 引擎，支持自定义角色。

## 特性

- **透明无边框、始终置顶**的窗口与角色动画
- **Sprite pack 引擎** —— 通过 `.opk` 包加载自定义角色
- **Lottie 动画**（基于三星 rlottie 库）—— 流畅的 60fps 播放
- **Win98 风格气泡提示** —— 自动消失的提示文案
- **多供应商语音合成** —— 阶跃星辰、MiniMax、Azure Speech、OpenAI；切换供应商无需重启
- **框架无关** —— 支持 OpenCode、Claude Code、Codex 或任何能发送 IPC 消息的工具
- **主动提示引擎** —— 检测编码模式并展示上下文相关建议
- **跨平台** —— macOS、Windows、Linux
- 空闲时**< 10MB 内存占用**

## Sprite Packs（角色包）

Seelie 通过 sprite pack（`.opk` 文件）和 Codex 宠物包（`.codex-pet` 文件）支持自定义角色。每个包包含：
- Sprite sheet 或 Lottie 动画
- 动画定义
- 事件到动画的映射
- 预览图

### 安装包

1. **拖拽**：将 `.opk` 或 `.codex-pet` 文件拖到宠物窗口上
2. **手动**：将文件复制到 `~/.config/Seelie/packs/`
3. **内置**：构建过程中会生成官方包

### Codex 宠物

<p align="center">
  <img src="assets/screenshots/codex-pet.png" alt="Codex 宠物示例" width="170">
</p>

Seelie 原生读取由上游 [openai/skills `hatch-pet`](https://github.com/openai/skills/blob/main/skills/.curated/hatch-pet/SKILL.md) skill 生成的 `.codex-pet` 归档。格式由固定 8×9 atlas（`spritesheet.webp`，1536×1872 像素，192×208 单元格）加 `pet.json` manifest 组成。将归档拖到宠物窗口上 —— Seelie 会自动识别并直接渲染，无需转换。9 个动画行（`idle`、`running-right`、`running-left`、`waving`、`jumping`、`failed`、`waiting`、`running`、`review`）直接映射到宠物状态机。

### 从上游档案拉取社区 Live2D 包

`assets/packs/` 中的 21 个一方包（Live2D Free Material 样例 + Furina + UnityChan）是仓库内唯一被 git 跟踪的角色包。其他碧蓝航线、少女前线、偶像次元、为美好的世界献上祝福等角色保存在上游社区档案 [Eikanya/Live2d-model](https://github.com/Eikanya/Live2d-model)（约 16 GB）。导入方式：

```bash
# 一次性：将上游档案 shallow clone 到项目中（节省时间和空间）
git clone https://github.com/Eikanya/Live2d-model thirdparty/upstream-live2d --depth=1

# 运行导入（精选 PICKS + 各分类批量导入，每类约 50 个）
cmake --build build --target import_packs

# 从导入的源目录生成 .opk 归档
cmake --build build --target generate_packs
```

导入的角色包会落到 `assets/packs/<id>/`（已 gitignore）—— 仅本地存在，可从上游 clone 重新生成。资产版权归原始游戏厂商所有；请仅作个人使用。

### 创建包

包格式规范见 `schemas/character-pack-v1.schema.json`。

## 构建

### 前置依赖

- **Qt 6.5+**（Widgets、Gui、Network、LinguistTools）
- **CMake 3.19+**
- **C++17 编译器**（GCC 10+、Clang 12+、MSVC 2019+）

### macOS

```bash
# 通过 Homebrew 安装 Qt
brew install qt@6

# 构建
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build .

# 创建 .app 包
macdeployqt Seelie.app

# 运行
open Seelie.app
```

### Windows（MSVC）

```powershell
# 通过 Qt Installer 或 vcpkg 安装 Qt
# vcpkg：vcpkg install qt6-base qt6-tools

# 构建
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"
cmake --build . --config Release

# 打包 Qt DLL
windeployqt Release\Seelie.exe

# 运行
Release\Seelie.exe
```

### Windows（MinGW）

```powershell
# 通过 Qt Installer 安装 Qt（选择 MinGW 套件）

# 构建
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\mingw_64"
cmake --build .

# 打包 Qt DLL
windeployqt Seelie.exe

# 运行
Seelie.exe
```

### Linux

```bash
# 安装 Qt 开发包
# Ubuntu/Debian：
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential

# Fedora：
sudo dnf install qt6-qtbase-devel qt6-qttools-devel cmake gcc-c++

# 构建
mkdir build && cd build
cmake ..
cmake --build .

# 运行
./Seelie
```

### 自定义更新服务器（可选）

默认更新服务器地址在构建时通过 CMake 缓存变量配置。Fork 项目时可在 configure 时覆盖，无需修改源码：

```bash
cmake -B build -DSEELIE_DEFAULT_UPDATE_ENDPOINT=updates.example.com:9340
```

运行时也可通过 QSettings 中的 `updateServerEndpoint` 键覆盖。

## IPC 协议

Seelie 通过本地 UDP 接收 IPC 消息：

| 传输 | 默认端点 |
|---|---|
| **UDP** | `127.0.0.1:52847` |

所有 Node.js gateway 默认发送至此端点。可通过任意 gateway CLI 命令的 `--endpoint <host:port>` 覆盖。

### 消息格式

按行分隔的 JSON。每条消息是一个以 `\n` 结尾的 JSON 对象。

### 消息类型

#### Event（事件）

```json
{
  "type": "event",
  "source": "opencode|claude-code|codex",
  "event": "session.start",
  "toolName": "write",
  "filePath": "src/main.cpp"
}
```

#### Tip（提示）

```json
{
  "type": "tip",
  "title": "Having trouble?",
  "body": "It looks like you're running into repeated errors.",
  "animation": "alert"
}
```

#### Ping/Pong

```json
// 请求
{ "type": "ping" }

// 响应
{ "type": "pong" }
```

### 统一事件名称（17 个事件）

| 事件 | 描述 |
|---|---|
| `session.start` | 会话/轮次开始 |
| `session.end` | 会话/轮次结束 |
| `session.idle` | 会话空闲（无活动） |
| `session.error` | 会话发生错误 |
| `prompt.submitted` | 用户提交了 prompt |
| `tool.before` | 即将执行工具 |
| `tool.after` | 工具执行完成 |
| `tool.failed` | 工具执行失败 |
| `permission.requested` | 向用户请求权限 |
| `permission.denied` | 权限被拒绝 |
| `permission.response` | 收到权限响应 |
| `subagent.started` | 子代理启动 |
| `subagent.stopped` | 子代理停止 |
| `notification.sent` | 通知已展示 |
| `file.edited` | 文件被编辑 |
| `file.watched` | 监视的文件发生变更 |
| `todo.updated` | 待办列表更新 |

## Gateway

从 npm 全局安装 CLI gateway：

```bash
npm install -g @eastlake/seelie-gateway
```

验证它能与运行中的 Seelie 通信：

```bash
seelie-gateway --ping
```

### 使用 Node 版本管理器（fnm / nvm / asdf）

如果你通过 fnm、nvm 或 asdf 安装 Node，`seelie-gateway` 命令会被放在每个 shell 各自的 shim 目录中，**不在 AI 工具派生 hook 子进程时的 PATH 上**。在终端里直接运行 `seelie-gateway` 没问题，但从 hook 调用会静默失败。

将其包装成一个 shell 脚本，用绝对路径定位 Node 与 gateway CLI。保存为 `~/.local/bin/seelie-gateway-hook` 并 `chmod +x`：

```sh
#!/bin/sh
# fnm 版本 —— 替换定位块即可适配 nvm / asdf
set -eu
fnm_root="$HOME/Library/Application Support/fnm/node-versions"
selected_base=""
for base in "$fnm_root"/*/installation; do
  [ -d "$base" ] || continue
  selected_base="$base"
done
[ -z "$selected_base" ] && { echo "no fnm Node installation found" >&2; exit 127; }
node_bin="$selected_base/bin/node"
gateway_cli="$selected_base/lib/node_modules/@eastlake/seelie-gateway/cli.mjs"
[ -x "$node_bin" ] || { echo "node not found" >&2; exit 127; }
[ -f "$gateway_cli" ] || { echo "gateway CLI not found" >&2; exit 127; }
exec "$node_bin" "$gateway_cli" "$@"
```

对于 nvm，将 `fnm_root` 块替换为 `node_bin="$HOME/.nvm/versions/node/<version>/bin/node"`。如果使用系统 Node（Homebrew 或发行版包），可以跳过包装脚本，直接在 hook 中调用 `seelie-gateway`。

然后在下面的 hook 配置中以包装脚本替代 `seelie-gateway` —— 例如 `~/.local/bin/seelie-gateway-hook --source claude-code --event session.start`。

### Claude Code

将 hook 添加到 `~/.claude/settings.json`：

```json
{
  "hooks": {
    "SessionStart": [{
      "hooks": [{
        "type": "command",
        "command": "seelie-gateway --source claude-code --event session.start",
        "timeout": 3,
        "async": true
      }]
    }],
    "Stop": [{
      "hooks": [{
        "type": "command",
        "command": "seelie-gateway --source claude-code --event session.idle",
        "timeout": 3,
        "async": true
      }]
    }],
    "PreToolUse": [{
      "hooks": [{
        "type": "command",
        "command": "seelie-gateway --source claude-code --event tool.before",
        "timeout": 5,
        "async": true
      }]
    }],
    "PostToolUse": [{
      "hooks": [{
        "type": "command",
        "command": "seelie-gateway --source claude-code --event tool.after",
        "timeout": 3,
        "async": true
      }]
    }]
  }
}
```

### 健康检查

检查 Seelie 是否在运行：

```bash
seelie-gateway --ping
# 退出码 0 = 存活，1 = 无响应
```

### 手动测试

发送一个测试事件：

```bash
seelie-gateway --source claude-code --event session.start
```

## 项目结构

```
seelie/
├── CMakeLists.txt              # 构建配置
├── CLAUDE.md                   # AI 工具的项目向导
├── CONTRIBUTING.md
├── LICENSE
├── README.md / README_CN.md
├── Seelie_zh_CN.ts                # 简体中文翻译
├── src/
│   ├── main.cpp                # 应用入口
│   ├── mainwindow.h/cpp        # 透明无边框宠物窗口
│   ├── IpcServer.h/cpp         # UDP IPC 服务端
│   ├── UdpWorker.h/cpp         # UDP worker（跑在独立 QThread 上）
│   ├── EventRouter.h/cpp       # 验证 17 个标准事件，转发提示文案
│   ├── PetStateMachine.h/cpp   # 宠物状态机 FSM —— 持有逻辑状态，发射动画链
│   ├── LottieAnimationEngine.h/cpp  # 主动画引擎（rlottie）
│   ├── Live2DAnimationEngine.h/cpp  # Live2D Cubism 引擎
│   ├── SpriteAnimationEngine.h/cpp  # 旧版 sprite-sheet 引擎
│   ├── LottieEffectOverlay.h/cpp    # 视觉特效叠加层
│   ├── CharacterPack.h/cpp          # 角色包数据结构
│   ├── CharacterPackManager.h/cpp   # 角色包发现与切换
│   ├── PackManagerWidget.h/cpp      # 角色包浏览器 UI
│   ├── EcgWidget.h/cpp              # ICU 监护仪显示模式
│   ├── TipWidget.h/cpp              # Win98 风格气泡
│   ├── TipsEngine.h/cpp             # 上下文提示模式匹配引擎
│   ├── TipsCatalog.h/cpp            # 提示目录加载器（i18n JSON）
│   ├── TTSEngine.h/cpp              # 基于 ITtsProvider 的 HTTP 协调器
│   ├── tts/
│   │   ├── ITtsProvider.h           # 供应商契约（synthesize / cancel）
│   │   ├── ProviderConfig.h         # 自由格式的每供应商配置袋
│   │   ├── TtsProviderRegistry.h/cpp     # 全部供应商的描述符表
│   │   ├── StepFunHttpProvider.h/cpp     # 阶跃星辰适配器
│   │   ├── MiniMaxHttpProvider.h/cpp     # MiniMax 适配器（hex 编码 JSON）
│   │   ├── AzureSpeechProvider.h/cpp     # Azure Speech 适配器（SSML 请求体）
│   │   └── OpenAiTtsProvider.h/cpp       # OpenAI 适配器
│   ├── ConfigManager.h/cpp          # 分层配置（默认值/便携/用户）
│   ├── SettingsPanelWidget.h/cpp    # 设置面板 UI（通用 + 语音 选项卡）
│   ├── StyledAlertWidget.h/cpp      # 主题化弹窗
│   ├── GlobalShortcutManager.h/cpp  # 显示/隐藏快捷键
│   ├── FullscreenWatcher.h/cpp      # 游戏模式自动隐藏
│   ├── SystemTray.h/cpp             # 系统托盘集成
│   ├── UpdateChecker.h/cpp          # 版本检查 UDP 客户端
│   └── MacFocusFix.h/.mm            # macOS 焦点修复
├── assets/
│   ├── animations.json         # Sprite 动画定义
│   ├── fonts/                  # 内置 HarmonyOS Sans SC
│   ├── i18n/                   # tips.<locale>.json（事件提示 + 招呼语）
│   ├── icons/                  # 应用图标
│   ├── lottie/
│   │   ├── character/          # 18 个 Lottie 角色动画
│   │   └── effects/            # 6 个 Lottie 特效（alert-pulse、confetti……）
│   └── packs/                  # 一方 Live2D 角色包
├── docs/
│   └── superpowers/
│       ├── specs/              # 设计文档（TTS 抽象等）
│       └── plans/              # 实施方案
├── gateways/
│   └── seelie-gateway/            # @eastlake/seelie-gateway CLI（Node.js，零依赖）
├── installer/
│   ├── config.xml.in           # Qt Installer Framework 根配置
│   ├── seelie.ini.template        # 安装器随 Seelie.exe 一起分发的便携默认值
│   ├── packages/               # IFW 包载荷（脚本、许可证、meta）
│   └── translations/           # 安装器界面翻译
├── schemas/
│   └── character-pack-v1.schema.json   # 角色包格式 schema
├── scripts/                    # 构建与导入辅助脚本（Python）
├── server/                     # Erlang/OTP UDP 更新服务器（rebar3）
├── tests/                      # Qt Test 测试套件（UDP 端口 52848）
│   ├── test_ipc_animations.cpp        # UDP IPC 端到端
│   ├── test_pet_state_machine.cpp
│   ├── test_ecg.cpp / test_gaming_mode.cpp
│   ├── test_tts_providers.cpp         # 各适配器单元测试（QHttpServer）
│   ├── test_tts_engine.cpp            # FakeProvider 契约测试
│   └── manual/test_tts_live.cpp       # 真实 API 烟雾测试（SEELIE_LIVE_TTS=1 启用）
└── thirdparty/
    ├── CubismNativeFramework/  # 子模块 —— Live2D Cubism SDK
    ├── CubismNativeSamples/    # 子模块 —— Cubism 示例（仅构建期使用）
    └── miniz/                  # 内联的 zip 库（.opk 归档支持）
```

## 配置

配置文件：`~/.config/Seelie/config.json`

```json
{
  "windowX": 100,
  "windowY": 500,
  "language": "en",
  "autoStart": false,
  "ipcEndpoint": "127.0.0.1:52847"
}
```

`ipcEndpoint` 字段默认为 `127.0.0.1:52847`，可被覆盖。

## 语音合成（TTS）

Seelie 可以通过四个云端供应商把提示读出来。语音引擎运行在独立线程上，使用一次性 HTTP 合成（不分片流式），在网络抖动时也稳定；切换供应商无需重启。

### 支持的供应商

| 供应商 | 鉴权 | 说明 |
|---|---|---|
| **阶跃星辰（StepFun）** | Bearer token | 默认地址：`https://api.stepfun.com/step_plan/v1/audio/speech`。文档示例提供两个音色（`cixingnansheng`、`linjiajiejie`）。 |
| **MiniMax** | Bearer token | 默认地址：`https://api.minimaxi.com/v1/t2a_v2`。如你的账户要求 `GroupId`，请直接附加到 URL 中。 |
| **Azure Speech** | 订阅密钥 | 端点根据 `region` 推导（例如 `eastus` → `eastus.tts.speech.microsoft.com`）。SSML 请求体，`Ocp-Apim-Subscription-Key` 头。 |
| **OpenAI** | Bearer token | 默认地址：`https://api.openai.com/v1/audio/speech`。兼容自部署的 OpenAI-API 网关。 |

### 配置步骤

1. 打开设置面板（系统托盘的齿轮图标，或右键宠物）
2. **通用** 选项卡 → 勾选 **启用语音**
3. **语音** 选项卡 → 从下拉框选择供应商，填写凭证（token、voice ID、可选的 base URL / 模型）
4. 点击底部的 **测试** 按钮验证

音色字段是自由文本 —— 把你从供应商控制台拿到的任意 voice ID 粘进去（系统/克隆/内测都行）。所有凭证会自动 `trim` 首尾空白，粘贴时不必担心多余空格。

### 新增供应商

抽象层很薄，新增第五个后端（比如 ElevenLabs）大约 150 行纯协议代码：

1. 在 `src/tts/<Name>HttpProvider.h/.cpp` 实现 `seelie::tts::ITtsProvider`（请求构造 + 响应解析；不涉及音频，不涉及线程）
2. 在 `src/tts/TtsProviderRegistry.cpp` 追加一条 `ProviderDescriptor`：稳定 ID、显示名、必填/可选字段、工厂 lambda
3. 在 `tests/test_tts_providers.cpp` 加一组单元测试，对着本地 `QHttpServer` 夹具

设置面板会从注册表自动构建出新供应商的页面，无需改 UI。

### 测试真实 API

`tests/manual/test_tts_live.cpp` 通过环境变量启用，跑真实供应商接口；发布前自检很方便：

```bash
SEELIE_LIVE_TTS=1 \
  SEELIE_STEPFUN_TOKEN=... \
  SEELIE_MINIMAX_TOKEN=... \
  SEELIE_AZURE_KEY=... SEELIE_AZURE_REGION=eastus \
  SEELIE_OPENAI_TOKEN=... \
  ./build/tests/test_tts_live
```

未导出某个供应商的凭证时，对应测试自动 skip。CI 永远不跑这个目标。

## 资产致谢

- 角色 sprite sheet 与动画数据：[clippyjs/clippy.js](https://github.com/clippyjs/clippy.js)（MIT 许可证）—— Clippy、Bonzi、F1、Genie、Genius、Links、Merlin、Peedy、Rocky、Rover
- 视觉特效：自制 Lottie 特效动画（MIT 许可证）
- 动画引擎：[三星 rlottie](https://github.com/Samsung/rlottie)（MIT 许可证）

## 鸣谢

### Live2D 角色包

- [Eikanya/Live2d-model](https://github.com/Eikanya/Live2d-model) —— 来自碧蓝航线及其他作品的 Live2D Cubism 3+ 模型社区档案。按需 clone 到 `thirdparty/upstream-live2d/`（opt-in，约 16 GB）；`scripts/import_live2d.py --local` 据此填充 `assets/packs/`。资产版权归原始游戏厂商所有；导入内容请仅作个人使用。
- [Bilibili: BV1fP411e7fA](https://www.bilibili.com/video/BV1fP411e7fA) —— `assets/packs/` 中 `little_demon`（小恶魔）与 `yumi` 两个 VTube Studio 模型包的来源。版权归原始创作者所有。

### Sprite Packs（遗留）

- [clippyjs/clippy.js](https://github.com/clippyjs/clippy.js) —— 把 Clippy 等角色带回 Web 的原始 JavaScript 库。全部 10 个 Office Assistant 角色的 sprite sheet 与动画定义都来自该项目。
- [pi0/clippyjs](https://github.com/pi0/clippyjs) —— ClippyJS 的现代 TypeScript 重写。
- [thebeebs/OfficeAssistant](https://github.com/thebeebs/OfficeAssistant) —— 微软 Office Assistant 的原始 C++ 源码。

## 许可证

MIT © HUANG Cheng
