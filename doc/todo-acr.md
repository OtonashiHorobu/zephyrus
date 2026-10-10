# zephyrus TODO — acr（现有游戏，保持不回归）

> 维护规则见 [todo-shared.md](todo-shared.md)。ACR 代码不删除；
> 本轮只做“分块搬迁 + 构建拆分”，行为必须与现状一致。

## A1. 基线

- [x] `cmake --build --preset conan-debug` 绿色（2026-10-09 重构前）。
- [x] `ctest --preset conan-debug` 绿色（重构后 3/3）。
- [ ] `scripts/acr-start.sh` + `client-zephyrus.sh` 实测不回归（菜单/ESP/rage）。
- [ ] 保留 [TODO.md](TODO.md) 中 A.2 待验证项（飞刀/手雷、fake-bit、文档、提交）。

## A2. 分块搬迁

- [x] `include/zephyrus/features/{esp,exploits,movement,rage,skeleton,thirdperson}.hpp`
      → `include/zephyrus/features/acr/`（exploits/movement 为未入库文件，用 mv）。
- [x] `src/zephyrus/features/*.cpp` → `src/zephyrus/features/acr/`。
- [x] `include/zephyrus/overlay/{overlay,menu}.hpp` → `include/zephyrus/overlay/acr/`。
- [x] `src/zephyrus/overlay/{overlay,menu}.cpp` → `src/zephyrus/overlay/acr/`。
- [x] `include/zephyrus/acr/`、`src/zephyrus/acr/game.cpp` 保持不动（已是独立模块）。
- [x] 更新所有 `#include` 与 CMake 源列表（26 处 include 已替换）。
- [x] 命名空间暂不改（`zephyrus::features` / `zephyrus::overlay` 仍为 ACR 所用），
      命名空间整理列入 A5。

## A3. 构建拆分

- [x] `zephyrus-core` 只留 shared 源（config/hook/log/rmlui/version/zephyrus）。
- [x] 新增 `zephyrus-acr-core`（STATIC）：acr 模块 + features/acr + overlay/acr + sdl(SDL1.2)
      + backend_sdl12_gl2 + system_interface_sdl12。
- [x] `zephyrus-acr`（SHARED）= `src/zephyrus_acr.cpp` + `zephyrus-acr-core`。
- [x] `zephyrus-standalone` 链接 `zephyrus-acr-core`（不触发入口 constructor）。
- [x] 保留 Linux 链接参数：static-libstdc++（standalone）、
      `-fvisibility=hidden` / `--exclude-libs,ALL` / version-script（shared）。

## A4. 验证

- [ ] `scripts/acr-start.sh` 注入实测一次：菜单 + ESP + rage + movement + exploits 回归。
- [ ] standalone 打开 ACR 菜单（拖动/滚动/配置 tab 正常）。
- [ ] `doc/TODO.md` A.2 的端到端验证项补做（飞刀/手雷）。

## A5. 后续（非本轮）

- [ ] 命名空间整理：`zephyrus::acr::features` / `zephyrus::acr::overlay`（机械 sed）。
- [ ] ACR 代码继续保留在仓库，但与 CS:GO 构建完全隔离（各自 target 互不链接）。
