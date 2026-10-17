# 分支策略

Zephyrus 按游戏拆成两条长期分支：共享同一段历史（到 `fb6b150` 为止），
之后各自独立演进，互不包含对方的游戏代码。

| 分支 | 游戏 | 状态 |
|---|---|---|
| `csgo` | CS:GO（legacy linux64） | **主开发分支**（默认分支） |
| `acr` | ACR | **已冻结**，仅存档，不再开发 |

- 每条分支只包含本游戏的绑定、功能、overlay 与资源；公共层（hook、RmlUi、
  config、logging、测试）在两条分支上分别维护。
- `acr` 的代码基线是最后一个纯 ACR 提交 `fb6b150`（其上只有一个冻结说明提交），
  标签 `acr-final` 指向该状态。它不接收 CS:GO 时期的共享层改动；将来恢复 ACR
  开发时，需要先把 `csgo` 上的公共层改动移植过来。
- 新功能、修复、依赖升级一律进 `csgo`。不要在 `acr` 上开新工作。
- CS:GO 分支的构建与测试命令见 `AGENTS.md`，脚本索引见 `scripts/README.md`。

## 远端

```bash
git push -u origin csgo acr
git push origin acr-final
# GitHub → Settings → Branches → Default branch 改为 csgo，然后再删除远端 main
```
