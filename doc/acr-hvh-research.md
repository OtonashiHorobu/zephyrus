# AssaultCube Reloaded — HvH 可滥用机制研究报告

> 目标：系统梳理 ACR 客户端/服务器信任模型下，HvH（cheat vs cheat）可用的
> trick / quirk / exploit，覆盖时间伪造（tickbase/double tap 对应物）、
> spread 计算、网络 desync / anti-aim、协议滥用，并给出利用方式与验证方法。
>
> 源码基线：`/datassd/Clone/acr-git` @ `1bf1c9dc`（18.3.0，PROTOCOL_VERSION 140）
> 编写日期：2026-10-09 · 状态：静态代码审计完成，`[待验证]` 条目需运行时确认

---

## 0. 摘要：ACR 的信任模型

ACR 是「**客户端权威（client-authoritative）**」引擎，与 CS:GO 的服务器权威模型完全相反：

| 状态 | 权威方 | 服务端校验 |
|---|---|---|
| 位置 / 速度 / 朝向 / 俯仰 | 客户端（`SV_POS`/`SV_POSC`） | 仅 `checkpos()` 地图边界 + 固体格强制死亡 |
| 移动速度 / 冲刺 / 跳跃 / noclip | 客户端物理 | **无**（`serverchecks.h` 明确写着 `// TODO: detect speedhack`） |
| 开火时刻 / 事件顺序 | 客户端（事件 `id` 时间戳） | 无墙钟校验；仅事件时间单调 + 冷却 |
| 射击方向 `to` | 客户端 | 无角度校验；仅射线被墙裁剪 |
| 开镜 / 下蹲 | 客户端 bit | 仅合法性白名单 |
| 目标 hitbox 位置（头部） | **射击者客户端**（`lagtrust>=1`，默认） | 距脖颈 ≤ 2 单位（50cm）夹取 |
| 手雷爆炸点 / 飞刀命中对象 | 客户端（`SV_EXPLODE`） | **几乎无**（手雷需消耗一次投掷条目、刀需刀条目） |
| 复活位置 | 客户端（`SV_SPAWN`） | XY 距服务器出生点 ≤ 6×PLAYERRADIUS（Z **无校验**） |
| 伤害结算 / 死亡 / 装甲 | 服务端 | 唯一真正的服务器权威部分 |

结论：**ACR 没有传统意义上的 anti-aim / desync 对抗面**——因为服务端本来就不模拟你。
HvH 的核心不是「绕过服务器校验」，而是「滥用服务器信任 + 服务端事件系统的逻辑漏洞」。
本报告按类别给出 15+ 个可操作机制，其中 6 个为「服务端认可的有效击杀/伤害」漏洞。

### 默认配置关键参数

| 参数 | 默认值 | 出处 |
|---|---|---|
| `scl.lagtrust` | **1**（信任客户端上报的头部位置） | `protos.h:987`，可用 `-kl0/-kl1/-kl2` 修改 |
| 事件时间映射 | `gameoffset + id`，客户端完全控制 `id` | `server.h:372` |
| `SV_POS` 节流 | 客户端 40ms；服务端无限制 | `client.cpp:570` |
| 速度检测 | 无 | `serverchecks.h:375` |

---

## 1. 时间系统：tickbase / double tap 的 ACR 对应物

### 1.1 机制总览

ACR 没有 tick 模拟（无 tickrate、无 tickbase）。所有游戏事件（开火、换弹、爆炸、切枪）
都通过客户端时间戳 `id` 排队，服务端用 `getmillis()` 把客户端时间映射为服务器时间：

```cpp
// server.h:372
int getmillis(int millis, int id)
{
    if (!timesync || (!events.length() && state.waitexpired(millis)))
    {
        timesync = true;
        gameoffset = millis - id;   // 重新锚定：把本次事件的 id 映射为“现在”
        return millis;              // 本次事件时间 = 现在
    }
    return gameoffset + id;         // 其余事件时间 = 锚点 + (id 差)
}
```

事件在 `processevents()` 里按时间序刷出（`serverevents.h:332`）：

```cpp
bool timedevent::flush(client *ci, int fmillis)
{
    if (!valid) return true;
    else if (millis > fmillis) return false;      // 未来事件排队等待
    else if (millis >= ci->lastevent)             // 过期乱序事件直接丢弃
    {
        ci->lastevent = millis;
        process(ci);
    }
    return true;
}
```

客户端可直接控制每个事件的「服务器时间」，约束只有：
1. 事件时间必须 ≥ `ci->lastevent`（单调不减，否则丢弃）；
2. 事件时间 ≤ 服务器当前时间（否则排队等待）；
3. 冷却判定 `wait = millis - lastshot` 用事件时间而非墙钟。

### 1.2 Burst Capacitor（“充能弹匣”/magdump-in-one-frame）★

**原理**：在事件队列里塞一个远期事件（blocker），让 `getmillis` 一直使用旧锚点
（`events.length() > 0` ⇒ 不重锚定）。此后可以发送**回填时间戳**的射击事件：
只要事件时间递增且 ≤ 当前服务器时间，它们会在**同一个服务器帧内全部结算**。

**利用步骤**：
1. 空闲期（队列空且 `waitexpired`）连发两个事件：
   - A = 无害事件（如 `SV_RELOAD`）：被重锚定，事件时间 = 现在，立即处理；
   - B = 同样无害但 id 远期（id_B = id_A + 10000）：此时队列非空，`getmillis` 返回
     `锚点 + id_B` = 现在 + 10s，于是 **B 被排队 10 秒**。
   注意：空闲后的**第一个**事件永远会被重锚定为“现在”，所以 blocker 至少要
   由“锚定事件 A + 远期事件 B”两个包组成。
2. 等待 T 毫秒（什么都不发即可；`lastevent` 停在 A 的时间，`now − lastevent` 持续增长）。
3. 把 N 个 `SV_SHOOT` 打包（id 间隔 = `attackdelay`，事件时间全部落在
   `[lastevent, now]`）：队列仍被 B 占着，不会重锚定 → 服务端在一帧内连续处理
   N 发子弹——M16 每发 73ms，充能 2.2 秒即可 30 发瞬间倾泻。
4. 每发子弹的 `to` 与 `pos` 独立，可一帧内切多个目标/多个爆头点；B 到期后循环重复。

**限制**：`addevent` 队列上限 256（`server.h:355`）；弹药由 `cs.mag[weap]` 服务端扣减；
`lastevent` 推进后需重新“充能”（等待时间片增长）。

**效果（HvH）**：对手（哪怕是 cheat）没有反应帧；所有子弹共享同一 `cs.o` 快照，
使「同一位置多弹齐射」成为可能。这是 ACR 的 double-tap 等价物，且**不需要换枪**。

**验证**：`[待验证]` 论文级时序需实测：建议 hook `weapon.cpp::sendshoot` 或直接构造
`SV_SHOOT` 包，录像确认服务端 `SV_DAMAGE` 到达时间戳聚簇。

### 1.3 冷却语义与瞬切枪

- `shotevent` 冷却：`wait = millis - cs.lastshot; wait < cs.gunwait[weap] → 拒绝`（`serverevents.h:74-80`）。
- `SV_WEAPCHANGE` 只是给**新武器** `gunwait += SWITCHTIME(400/200)`（`server.cpp:3747`），
  而判定用「距上次射击的时间」。因此：**距上次射击 ≥ SWITCHTIME 时，切枪+开火可同包完成**，
  没有独立于 shot clock 的切枪硬直（正常客户端由本地 `weaponchanging` 拦截，cheat 直接忽略）。
- `SV_QUICKSWITCH`：切回主武器只加 200ms（`server.cpp:3752`）。
- 只要事件时间给足 `attackdelay` 间隔，跨武器冷却按 `updateshot()` 统一衰减
  （`server.h:197-204`），不存在“切枪刷新冷却”以外的捷径；但瞬切 + 充能组合可打出
  “步枪弹幕 → 同帧手雷/刀”的套餐。

### 1.4 未来事件（prefire / dead-man shots）

事件时间可以放在未来（≤ 10s 队列内任意），服务端会准点替玩家“开枪/换弹”。
实际限制：`shotevent::process` 有 `cs.isalive()` 检查，死亡 300ms 后（`isalive` 宽限，
`server.h:185-188`）队列里的未来射击会被丢弃。300ms 内 = **死亡后子弹仍然飞出**。

---

## 2. 位置与移动：完全客户端权威

### 2.1 `SV_POS` / `SV_POSC` 接受逻辑

```cpp
// server.cpp:4102-4136（经典包，摘要）
vec newo; loopi(3) newo[i] = getuint(p)/DMF;
int newy = getuint(p);          // yaw：任意 uint，无范围校验
int newp = getint(p);           // pitch：任意 int，无范围校验
... dvel = getint(p)/DVELF;     // 速度增量
if(!broadcast || (cs.state!=CS_ALIVE && cs.state!=CS_EDITING)) break;
if(m_demo(gamemode) || !movechecks(cp, newo, newf, newg)) break;
cs.o = newo;                    // 直接采信
cs.vel.add(dvel);               // 速度累加（唯一服务端用途 = spread，见 §3）
cp.y = newy; cp.p = newp;       // 朝向直接采信
checkmove(cp);                  // 只更新统计，无检测
```

`movechecks()`（`server.cpp:3265-3360`）是唯一的“反作弊”，它只做四件事：
1. 下蹲/开镜状态转换计时；
2. 摔落伤害（`onfloor` 从 0 变 1 时按 `fallz - z` 结算）；
3. `checkpos()` 地图边界/固体格子（**目标点**在固体柱内或 z 超出该柱 floor..ceil → 强制死亡）；
4. 走到物品/旗帜/掉刀 3~5 单位内 → 拾取。

**没有**：最大速度、距离/时间一致性、路径碰撞（无 swept 检测）、爬墙/飞行限制。

### 2.2 Speedhack / Teleport / Noclip

- 直接以任意频率发送 `SV_POS`，每次位置可以横跨全图——只要落点在合法开放格内。
  **可以横穿墙体**（不检查中途），只是不能停在固体柱内（会 `checkpos`→`forcedeath`）。
- 垂直方向只受目标柱 `floor..ceil` 限制：室外柱上限极高 → **Fly / 空降**。
- 水平被夹到地图 `[2, size-2]`，超出同样 `forcedeath`（自杀）。
- `OBIT_CHEAT` 检测函数 `client::cheat()` 在正常构建里没有任何调用者；`farPickups`
  计数器已死代码（只在 `mapchange` 重置）。**无任何远程检测**。

**HvH 用途**：任意角度进场、瞬移抢旗/拾取、躲弹道（对手客户端弹道插值追不上）。

### 2.3 拾取/抢旗/目标点

拾取全部基于服务端 `cs.o`（= 客户端位置）的邻近判定（`movechecks` 末段）：
- 物品 3 单位、掉刀 5 单位、旗帜 `PLAYERRADIUS` 相关距离；
- CTF 得分距离用 `flagpickupo` 到当前位置计算 → 瞬移抢旗后瞬移回基地得分；
- 血量站/医疗包随便刷（有重生计时，但可用瞬移收割）。

### 2.4 摔落伤害与“泰山压顶”击杀 ★

```cpp
// server.cpp:3290-3327
const float dz = cs.fallz - cs.o.z;
if(newonfloor) { // 从空中变 onfloor
    if (dz > 2.5 * CUBES_PER_METER) {
        loopv(clients) {
            ... 若水平距离 ≤ 2.5*PLAYERRADIUS 且 Z 重叠 ...
            if(!isteam(&t,&cp) && !ts.protect(...))
                serverdied(t, cp, 0, OBIT_FALL, FRAG_NONE, cs.o);   // 砸死对方
        }
        if (!hit) 正常的摔伤（≤95）
    }
}
```

**利用**：同帧发送两个 `SV_POS`：
1. 高 z + `onfloor=0`（把 `fallz` 抬高），
2. 把自己瞬移到敌人头顶（同柱合法 z）+ `onfloor=1`。
服务端判定“从高处落到敌人身上” → 敌人被 `serverdied` **直接击杀**（服务器认可，
记分板计分，OBIT_FALL）。自己无摔伤（有 cushion）。这是 ACR 版 “goomba stomp”。

---

## 3. 射击与命中判定（spread / 静默瞄准 / lagtrust）

### 3.1 完整链路

```
客户端 weapon::sendshoot(to)                         weapon.cpp:511
  → SV_SHOOT { cn, id=lastmillis, weap, to.xyz,
               [每个可见玩家的 cn、o.xyz、head.xyz]…, -1 }
服务端 server.cpp:4023 解析 → shotevent(queued, millis=getmillis(id))
processevents → shotevent::process()                 serverevents.h:70
  ├─ 校验：isalive / weap==gunselect / wait≥gunwait / mag>0
  ├─ 服务端 spread：改写 to
  ├─ straceShot(from→to)：只被墙裁剪（from = cs.o 服务端位置）
  ├─ nearesthit() → parsepos(目标位置) → hitplayer()：球/胶囊判定
  └─ shot()：伤害、穿透、跳弹、爆头 FX、serverdamage()
```

### 3.2 服务端 spread 完整公式与全部可控输入

```cpp
// serverevents.h:94-115
const float spreadf = to.dist(from)/1000,          // to 由客户端给
            crouchfactor = cs.crouchfactor(gamemillis);
from.z += PLAYERHEIGHT * crouchfactor;
const int zoomtime = ADSTIME(perk2==PERK_TIME);    // 300 / 200
float adsfactor = 1 - float(cs.scoping
        ? min(gamemillis-cs.scopemillis, zoomtime)
        : zoomtime - min(gamemillis-cs.scopemillis, zoomtime))
    * guns[weap].spreadrem/100/zoomtime;
// 枪械：spread = guns[weap].spread * (cs.vel.magnitude()/3 + 0.4) * 1.2 * crouchfactor;
// applyspread 给 to 的每轴加 (rnd(spread)-spread/2) * spreadf * ads * perk2因子
```

**客户端可控项（全部无需真实状态）**：

| 因子 | 服务端来源 | 伪造方式 | 效果 |
|---|---|---|---|
| `cs.vel` | `cs.vel.add(dvel)` 客户端增量 | 发送抵消增量把速度归零 | 速度项 0 → 最小 0.4 |
| `cs.crouching` | POS 包 bit7 | 只置 bit，不真实下蹲 | ×0.75 |
| 蹲起过渡 | `crouchfactor()` 淡出 | 蹲满 500ms 后松开，500ms 内开火 | 站立状态仍得蹲姿精度 |
| `cs.scoping` | g bit4 / POSC bit | 随意置位 | 开镜精度 |
| 开镜过渡 | `scopemillis` 淡出 | **松开开镜后 zoomtime(300ms) 内开火仍保持开镜精度（≈1−spreadrem/100）** | 不开镜状态打开镜精度 |
| 重开镜 | 过渡公式可叠加 | 松开↔开镜循环几乎不掉精度 | 反复 peek 无惩罚 |
| `cs.perk2` | `SV_LOADOUT` → 复活生效 | 选 `PERK2_STEADY` | 再 ×0.75/×0.65 |

**说明**：
- 最终每轴角偏差 ≈ `spread * factor / (2·|to-from|)` rad，其中
  `factor = (|to-from|/1000) · ads · perk`，即半角 ≈ `spread·ads·perk/2000` rad。
- 散射是**服务端随机**（`rnd`），客户端无法预知/取消；但把上表全部拉满后，
  M16 蹲+开镜+稳枪 perk 的半角 ≈ 65·0.4·1.2·0.75·0.05·0.75/2000 ≈ 0.022°。
- 霰弹枪完全**不吃移动/下蹲项**（`serverevents.h:105` 只用 `spreadf*adsfactor`），
  故霰弹的散布只与距离和开镜有关。
- `applyspread` 有 `spread <= 1` 直接返回的短路：heal/刀/剑/RPG 绝对精度。

### 3.3 lagtrust：目标 hitbox 由射击者决定 ★★

```cpp
// serverballistics.h:104-124
void parsepos(client &c, const vector<posinfo> &pos, vec &out_o, vec &out_head) {
    const posinfo *info = 找 c.clientnum;
    if (scl.lagtrust >= 2 && info) out_o = info->o;      // 完全信任位置
    else out_o = c.state.o;                              // 默认：服务端位置
    out_o.z += PLAYERHEIGHT * c.state.crouchfactor(gamemillis);
    if (scl.lagtrust >= 1 && info && head 各分量>0) {
        out_head = info->head;                           // 客户端上报的“头”
        out_head.sub(out_o);
        if (out_head.magnitude() > 2) out_head.normalize().mul(2);  // 只夹到 50cm
        out_head.add(out_o);
    } else out_head = vec(0,-.25,.25).rotate_around_z(c.y*RAD).add(out_o);
}
```

默认 `lagtrust=1` ⇒ **每个玩家的头部球心可以在服务端躯体中心 50cm 半径内任意摆放**。

**利用**：
1. **Head magnet**：把头部球往自己方向拉 → 用 50cm 的偏移修正瞄准误差，
   加上 `HEADSIZE=0.4`，爆头判定窗比真实大得多。
2. **隔墙爆头**：射线 `straceShot` 只裁到墙面；把敌人头部拉向自己一侧
   （只要敌人在薄墙后 ≤50cm，且头部球面越过墙面）→ **穿墙爆头**，2 米内的目标甚至能
   在几何上完全不可见时被爆头。
3. `sendshoot` 里 `pos` 列表完全来自本地玩家列表（`weapon.cpp:526-537`），
   cheat 可以在发送前覆盖 `players[i]->head/o`。
4. `-kl2` 服务器（`protos.h:1065`）下 `out_o` 也可伪造 → 全 hitbox 摆在射线上，
   等于「百分百命中 + 穿一切 + 任意角度」。

**注意**：`lagtrust=0` 时服务器完全用自己位置 + 用**目标自己上报的 yaw** 估算头部偏移
`vec(0,-.25,.25).rotate_around_z(c.y*RAD)`——这是唯一一处「你的 yaw 影响自己的受击盒」，
但半径只有 ~0.35，意义有限。

### 3.4 hitplayer：用射击者视角构造受击者胶囊 ★

```cpp
// serverballistics.h:40-69  nearesthit() 里：
hz = hitplayer(from, actor.y, actor.p, to, o, head, &end);
//                    ^^^^^^^ 射击者的 yaw/pitch，不是目标的！
float y = yaw*RAD, p = (pitch/4 + 90)*RAD, c = cosf(p);
vec bottom(target), top(sinf(y)*c, -cosf(y)*c, sinf(p));
bottom.z -= PLAYERHEIGHT;
top.mul(PLAYERHEIGHT).add(bottom);
```

- 受害者的「躯干/腿胶囊」方向由**射手**上报的 `yaw/pitch` 决定。
- `POSC` 下 pitch 被编码为 ±90，但**经典 `SV_POS` 的 `newp` 是原始 int 无裁剪**
  （`server.cpp:4109`），`pitch/4` 是整数除法。
- 把 `pitch` 设成 360/720 等值 → `p = 180°/360°` → **胶囊被摆成水平**，
  在角色脚部沿射手朝向伸出 2m 的“人肉长矛”。
- 意义：射手可以微调 `y/p` 把受害者 hitbox 整形到自己的弹道上，
  对冲 spread、打“本该 miss”的擦边球；同时也是**擦边爆头**（头球 + 倾斜躯干）
  的第二重命中保障。
- 副作用：这些值会被原样广播给其他客户端渲染（诡异视角可见）。
  静默瞄准时保持 0/0 则是正常竖直胶囊。

### 3.5 静默瞄准 / no-scope / 弹道伪装

- `to` 与 `cp.y/cp.p` **完全解耦**：服务端从不校验弹道方向与视角一致，
  也不校验开镜状态，因此「静默瞄准」是引擎原生支持的（`to` 任意）。
- 服务端会向下兼容广播 `SV_SHOOT`（含真实 `from/to`）给所有客户端播放曳光/音效
  （`serverevents.h:262`），所以**无法彻底隐藏射击**；可以做到的是：
  - 用 lagtrust 头部穿墙 → 对手画面里子弹打在墙上（甚至触发跳弹特效），却出现
    爆头血花/掉血 → 观感上像“灵异 ricochet”；
  - 使用 `SV_SHOOTC`（紧凑包）时其他客户端拿到 `from=to=(0,0,0)`（`clients2c.cpp:837`），
    但该包对普通武器没有有效弹道（见 §7.2），不能当“隐藏射击”用。
- **无真正的 hide shots**：射击动画/曳光/音效由服务端广播驱动，客户端做不到单边抑制。

### 3.6 命中后穿透与跳弹速查

- `shot()` 命中后会以 ±5° 随机扰动继续穿透（`FRAG_PENETRATE`，距离惩罚 +10m），
  可以一发穿多人；跳弹只在**首段**（`!dist`）且击中法线夹角 >15° 时发生一次
  （`serverballistics.h:507-535`），跳弹重置 exclude，**同一人可被跳弹+穿透二次命中**。
- ACR 无墙体穿透（除跳弹），但 lagtrust 头部/`-kl2` 可绕过。
- 击杀风格位（`FRAG_GIB/FLAG/CRIT/PENETRATE/RICOCHET`）影响计分与 XP，cheat 无法伪造
  服务端 RNG，但可用命中几何（爆头/穿透）刷高分。

---

## 4. 投掷物 / 爆炸协议漏洞（服务端认可的高危项）

### 4.1 `SV_EXPLODE` 手雷远程瞬爆 ★★★

```cpp
// server.cpp:4059-4068 解析：cn, id, weap, flags, x, y, z —— 全部客户端给定
// serverevents.h:14-19
case GUN_GRENADE:
    if (!cs.grenades.remove(flags)) return;      // 只需匹配自己的一次“拉环”id
    damagedealt += explosion(c, o, weap, ...);   // o = 客户端给的坐标！
```

**利用**：
1. 拉环：发一个 `SV_SHOOTC(cn, id, GUN_GRENADE)`（`shotevent` 会 `--mag` 并
   `cs.grenades.add(id)`，扣一颗雷）。
2. 发 `SV_EXPLODE(cn, id, GUN_GRENADE, flags=同一个 id, x,y,z=敌人脚下)`。
   爆炸立即在任意合法开放坐标发生（`checkpos` 只夹进地图），无抛物线/飞行时间校验。
3. `radialeffect` 只做爆炸点→目标的可见性射线，没有“手雷是否到过这里”的检查。
   选在敌人身后/侧面的开放空间即可绕掩体。
4. 可拉满 mag（初始 1 + 拾取最多 3）连炸，且**不会触发自身 spawn protection 解除**
   （解除只在 `shotevent`，`destroyevent` 里没有）→ **复活保护期内白嫖击杀**。

### 4.2 `SV_EXPLODE` 飞刀命中伪造 ★★★

```cpp
// serverevents.h:21-61
case GUN_KNIFE:
    if (!cs.knives.removeany()) return;          // 只消耗任意一把“在飞”的刀（忽略 id！）
    ushort dmg = effectiveDamage(GUN_KNIFE, 0);  // 距离 0 满伤害 80*HEALTHSCALE
    client *hit = valid_client(flags) && flags != c.clientnum ? clients[flags] : NULL;
    // flags = 客户端指定的受害者！
    if (hit) { ... 无任何距离/位置/可见性检查 ...
        hit->state.addwound(...);  // 附加流血
        serverdamage(*hit, c, dmg, GUN_KNIFE, ...); }
```

**利用**：
1. 发 `SV_SHOOTC(cn, id, GUN_KNIFE)`（刀出手，消耗 1 刀弹药、`cs.knives.add`）。
   —— **不需要真的扔**（`SV_THROWKNIFE` 可以完全跳过）。
2. 发 `SV_EXPLODE(cn, id, GUN_KNIFE, flags=任意敌人 clientnum, x,y,z=任意开放点)`。
   命中**无条件成立**：穿墙、全距离、无弹道、无角度。5% 暴击 128 伤害即秒杀；
   否则 53 伤害 + 流血。
3. **可持续循环**：`destroyevent` 会用客户端给的 `o` 在执行端 `sknives` 里落地一把刀
   （`serverevents.h:56-59`）；把 `o` 设在自己脚下，`movechecks` 下一帧就会回收
   （5 单位内 & `ammo[GUN_KNIFE] < 2`）→ 弹药+1 → 再次循环。
   唯一节奏限制是 `SV_SHOOTC` 的 500ms `attackdelay`（事件时间可控，见 §1）。
4. 击杀为服务端认定（OBIT = GUN_KNIFE），计分、清算 normal；被拍死的人无从辩解。

### 4.3 其它爆炸入口

| 入口 | 可控位置 | 校验 | 备注 |
|---|---|---|---|
| `SV_EXPLODE` 手雷 | 是（任意开放点） | 需一次 `grenades.add` 条目 | 4.1 |
| `SV_EXPLODE` 飞刀 | 是 + 指定受害者 | 需一次 `knives.add` 条目 | 4.2 |
| `SV_STREAKUSE` 空袭 | 是（任意点 ±3 随机散布 | 需 `airstrikes>0` | `usestreak` `server.cpp:478` |
| RPG 直击/爆炸 | 否（`to` 被 `straceShot` 裁墙） | — | 正常弹道 |
| 手雷正常投掷 `SV_THROWNADE` | FX only | `throwable>0` | 不产生伤害 |
| `SV_SHOOTC`+`SV_EXPLODE` 组合 | 是 | 如上 | 即可“隔空手雷” |

---

## 5. 刷新 / 装备 / 生命周期

### 5.1 `SV_SPAWN`：复活位置客户端决定 ★★

```cpp
// server.cpp:3979-4001
int cn = getint(p), ls = getint(p);
vec o; loopi(3) o[i] = getint(p)/DMF;      // ← 客户端给的复活坐标
if ((cs.state!=CS_ALIVE && cs.state!=CS_DEAD) || ls!=cs.lifesequence || cs.lastspawn<0) break;
cs.lastspawn = gamemillis;                  // 刷满复活保护
cs.state = CS_ALIVE;
vec lasto = cs.o; cs.o = cp.spawnp = o;
...
if (!m_edit(gamemode) && (lasto.distxy(cs.o) >= 6 * PLAYERRADIUS))   // 只查水平！
    serverdied(cp, cp, 0, OBIT_SPAWN, ...);   // 超界=自己死
```

- 正常客户端在收到 `SV_SPAWNSTATE` 后本地 `entinmap()` 微调再回发
  （`clients2c.cpp:771`）；cheat 直接改数字。
- **XY 限制 6×1.1=6.6 单位（约 1.65m），Z 完全无校验**（z 检查被注释掉）。
  即：可在服务器出生柱内任意高度**出生**（室外柱 = 高空伞降），
  也可在 ±6.6 范围内贴边出生；落点合法即不会触发后续 `checkpos` 死亡。
- `cs.lastspawn=gamemillis` + 不动枪 = 保持复活保护，见 §4.1 的白嫖打法。

### 5.2 `SV_LOADOUT` / `SV_WEAPCHANGE`

- `SV_LOADOUT` 随时可发（`server.cpp:3811`），修改 `nextprimary/secondary/perk1/perk2`，
  下次**复活**生效；`spawnstate()` 会做区间校验（非法则随机），但对具体武器组合
  没有服务器策略校验（除模式覆盖，例如 insta 强制 bolt）。
- `SV_WEAPCHANGE` 可切到任意 `0..NUMGUNS-1`（`server.cpp:3736` 只查范围，不查拥有），
  但 `respawn()` 清空全部 `mag/ammo/gunwait`（`entity.h:405`），非本命武器 mag=0 →
  `shotevent` 拒绝。因此“双主武器”不成立；但**瞬间切换 + 同帧开火**成立（§1.3）。
- 飞刀/手雷条目与 `mag` 的对应关系见 §4。

### 5.3 刀 / 雷 的“可投掷次数”追踪

- `projectilestate::add` 每次 `+1 throwable`，`SV_THROWNADE/KNIFE` 扣 1，
  只用于 FX 校验；**伤害与命中不看 throwable**（只看 `grenades/knives` 条目）。
- 因此可以“拉环→瞬爆”而跳过任何投掷动画/轨迹包。

---

## 6. 其它协议面

### 6.1 消息洪泛与放大（DoS / 反制对手）

- 溢出豁免表（`server.cpp:3551`）：`SV_POS, SV_POSC, SV_SPAWN, SV_SHOOT, SV_SHOOTC, SV_EXPLODE`
  **不计入 overflow**；其他消息累计 200 条即 `DISC_OVERFLOW`（`server.cpp:3554/4805`）。
- 于是用豁免包洪泛不会被踢：每个 `SV_SHOOT` 会广播给全服（N 倍放大），
  `SV_POS` 每包触发 `movechecks` 的全实体拾取循环（O(clients×sents)）。
  既可 self-DoS（队列 256 上限），也可用来**压榨服务器带宽/CPU 为自己创造 lag 优势**。
- `addevent` 256 上限会静默丢弃超出事件（自身射击可能被吞，需注意）。

### 6.2 伪造类消息

| 消息 | 现象 | 说明 |
|---|---|---|
| `SV_CLIENTPING` | 任意显示 ping | 客户端自报，服务端指数平滑后转发（`server.cpp:4092`） |
| `SV_SOUND` | 假跳跃/落地/无弹药音 | 只放行 4 种音效，`cn` 必须是本人/己方 bot（`server.cpp:4573`） |
| `SV_THIRDPERSON` | 任意第三人称距离广播 | `server.cpp:3942`，用于 scoreboard/观感 |
| `SV_SWITCHNAME` | 1s 内可改名的简单节流 | `server.cpp:3824`，无冷却队列 |
| `SV_TEXT` | 私聊/语音目标可控 | 正常聊天面 |
| `SV_EDITENT` | `while(sents.length() <= id)` 无界分配 | 仅 edit/coop 模式可达（`server.cpp:4767`），`id` 巨大 → OOM/卡服 |
| `SV_NEWMAP` | 地图尺寸任意 | `snewmap` 有 clamp（`server.cpp:4787`） |

### 6.3 `SV_CONNECT` 握手

- `acversion/acbuildtype/acthirdperson/acguid` 全部客户端自报（`server.cpp:3575-3578`），
  其中 `acthirdperson` 被当作「允许的第三人称距离」转发给全服；cheat 可借此
  以奇怪值污染别人对该玩家的渲染（危害有限）。
- 重复连接检测按 ENet IP:port（`server.cpp:3607`，**非 GUID**）。

---

## 7. 细节澄清（避免误判）

### 7.1 紧凑射击包 `SV_SHOOTC` 对普通武器无效

`shotevent` 对紧凑包不读 `to/pos`，`to` 保持 `(0,0,0)`（`server.h:50`），
服务端弹道从 `cs.o` 指向地图原点。对其他客户端 `from=to=(0,0,0)`
（`clients2c.cpp:837`）。除了刀/雷（`compact` 语义），**不能用来隐藏真实射击**。

### 7.2 视角与弹道：服务端不校验一致性

- `cp.y/cp.p` 只影响：他人渲染、`hitplayer` 的胶囊方向（§3.4）、默认头部偏移（§3.3）。
- **弹道只看 `to`**。因此“静默瞄准”是原生行为，不是漏洞；真正可被滥用的是
  用 `y/p` 给对手 hitbox 整形。

### 7.3 `cs.vel` 的服务器用途

唯一读取点是 spread 公式（`serverevents.h:112`，另一处 `pitchvel` 被注释）。
伪造/归零速度**不影响任何移动逻辑**（移动本来就客户端权威）。

### 7.4 进入固体格子 = 自杀

`checkpos` 对 SOLID 柱返回 `ret=true`，`movechecks` 视为“collides with the map”，
包含 `++mapcollisions`、发服务端消息、`forcedeath`，并置 `isonrightmap=false`
直到重发 `SV_MAPIDENT`。瞬移穿墙时务必选开放落点。

### 7.5 死亡宽限 300ms

`clientstate::isalive()` 把死后 300ms 仍视为可射击（`server.h:185`）。
结合未来事件队列，可做出“同归于尽后补刀”的行为。

---

## 8. CS:GO 术语 → ACR 对照

| CS:GO 概念 | ACR 现状 | 等价物 |
|---|---|---|
| Tickbase abuse | 无 tick 模拟，事件时间全客户端控 | §1 事件时间伪造 / burst capacitor |
| Double tap | 无独立开火锁 | §1.2 充能弹匣 + §1.3 瞬切枪 |
| Hide shots | 服务器强制广播射击 | 无直球；只能用 lagtrust 穿墙 + 弹道错觉 |
| Anti-aim | 受击躯干胶囊由**射手**决定 | 不存在传统 AA；替代：crouch/scope 淡出期、头部默认偏移、瞬移规避 |
| Desync / fake lag | 位置每包心跳、远程端物理外推 | 稀疏/乱序 POS + 伪造 dvel（§6/§3.3） |
| Backtrack | 服务端信任客户端上报 hitbox | §3.3 lagtrust head/full |
| Wallbang | 无墙体穿透，只有跳弹 | lagtrust 穿墙头 + 手雷/刀协议漏洞 |
| Bunnyhop/speedhack | 无服务器移动模拟 | §2 原生支持 |
| Spread control | 服务端权威随机 | §3.2 输入项全部可伪造（vel/crouch/scope/perk） |

---

## 9. 优先级矩阵（默认服务器 `lagtrust=1`）

| # | 机制 | 类型 | 服务端认可伤害 | 检测难度 | 影响 |
|---|---|---|---|---|---|
| 1 | `SV_EXPLODE` 飞刀指定受害者 | 协议滥用 | ✅ 瞬时 53~128 + 流血，可循环 | 极难（唯一痕迹是刀落地位置） | 秒杀/穿墙/无弹道 |
| 2 | `SV_EXPLODE` 手雷任意点引爆 | 协议滥用 | ✅ 径向伤害 | 难（需记录投掷轨迹才能判） | 隔空炸/绕掩体/保护期白嫖 |
| 3 | lagtrust 头部伪造（默认） | 信任滥用 | ✅ 爆头 | 极难（客户端本就上报） | 50cm 爆头磁铁 + 穿墙爆头 |
| 4 | §1.2 充能弹匣 | 时间伪造 | ✅ 同帧多发 | 极难（时间戳看似合法） | 无反应帧齐射 |
| 5 | §2.4 泰山压顶 | 逻辑漏洞 | ✅ OBIT_FALL | 中（位移不连续） | 瞬移必杀，需同柱 |
| 6 | §5.1 出生点任意 Z | 协议滥用 | — | 低（俯视视角明显） | 高空复活/贴边复活 |
| 7 | §2.2 瞬移/飞行/穿墙 | 信任滥用 | — | 低（录像明显） | 全图机动，客户端端反制 |
| 8 | §3.2 spread 全伪造 | 信任滥用 | ✅ | 难（与服务端公式结果一致） | 静态精度 ≈ 无散射 |
| 9 | `-kl2` 服务器 | 配置 | ✅ 全命中 | — | 完全 OP（需服务器开启） |
| 10 | 豁免包洪泛 | DoS | — | 易（流量） | 制造自身 lag / 压服 |

---

## 10. 运行时验证计划（黑盒）

环境：`scripts/acr-start.sh`（LD_PRELOAD 注入 zephyrus），本地服务器自己开，
先用 `ZEPHYRUS_ACR_BOTS=N` 空服 + `-kl0/1/2` 三种 lagtrust 对比。

| 实验 | 方法 | 预期观测 |
|---|---|---|
| 瞬移/飞行 | hook `sendposition` 直接改写坐标 | 服务端不踢，物品拾取/旗帜瞬移成功；进固体格自杀 |
| 泰山压顶 | 构造两帧 POS（高 z→敌人头上 onfloor=1） | `SV_DAMAGE`/OBIT_FALL 击杀敌人 |
| 充能弹匣 | 先发锚定事件 + 远期 `SV_RELOAD` blocker（两个包），等待后批量 `SV_SHOOT`（时间戳回填） | 服务端同帧产生 N 条 `SV_DAMAGE`/音效 |
| 远程手雷 | `SV_SHOOTC` + `SV_EXPLODE(flags=id, o=敌人上/后方)` | 敌人受伤，特效出现在指定点 |
| 飞刀伪造 | `SV_SHOOTC(KNIFE)` + `SV_EXPLODE(flags=敌人cn, o=脚下)` | 敌人 53/128 伤害+流血；刀可回收再循环 |
| head magnet | `sendshoot` 前把 `players[i]->head` 拉向自己 50cm | 擦边弹道显示爆头；穿薄墙爆头成功 |
| lagtrust 对比 | 同上在 `-kl0/1/2` 下重复 | `-kl0` 失败 / `-kl1` 50cm / `-kl2` 任意位置 |
| spread 伪造 | 移动中把 `dvel` 归零 + 蹲姿 bit + 开镜 bit + 稳枪 perk | 弹着点集合显著收紧（统计 100 发） |
| 视角整形 | 经典 `SV_POS` 发送 pitch=360 等值 | 对手胶囊/渲染异常，可调命中窗口 |
| 协议洪泛 | 高频 `SV_SHOOT`/`SV_POS` | 服务器带宽/CPU 上升，自身不被 DISC_OVERFLOW |

实现切入点（zephyrus 已有 hook 框架）：
- 出站篡改：hook `sendpackettoserv` / `ENet` 写包，或复用游戏 `addmsg/addmsgraw`
  在 `c2sinfo` 周期内插入伪造消息；
- 状态注入：hook `sendposition` / `sendshoot` 直接改写本地 `playerent`；
- 服务端旁路：单独构建一份打上日志的 `acr_server` 观察事件时序。

---

## 11. 修复建议（如果目标是“能打但不能被滥用”）

1. **`SV_EXPLODE` 白名单化**：服务端自己模拟手雷/飞刀轨迹（现有 `bounceent` 物理可复用），
   校验爆炸点 = 模拟终点、飞刀命中 = 模拟碰撞结果；至少校验到点距离与 TTL。
2. **`lagtrust` 默认改 0**，或仅允许 `head` 与「服务端插值位置 + 速度外推」偏差受限
   （现在是 50cm 自由球）。
3. **事件时间改为服务端单调时钟**：`getmillis` 至少做 `id` 与墙钟偏移的滑动窗口校验，
   阻断回填/未来事件；事件队列为空时不要重置锚点（幂等）。
4. **移动校验**：给 `movechecks` 加速度/距离上限（按 ldt 比例）、路径 raycast
   （可复用 `srayclip`），至少检测连续两包间的直线穿墙。
5. **`SV_SPAWN` 用服务端出生点覆盖 Z**，并校验与 `spawndist` 的完整 3D 距离。
6. **冷却与墙钟绑定**：`lastshot`/`gunwait` 用服务器时间而非事件时间。
7. **豁免表收紧**：给 `SV_POS/SHOOT/EXPLODE` 加频率上限；或按字节数计费。
8. **`hitplayer` 胶囊改为受害者自身姿态**（或服务端模拟姿态），不要用射击者角度。

---

## 附录 A：客户端→服务端消息速查（可直接组包）

| 消息 | 载荷 | 关键校验 |
|---|---|---|
| `SV_POS` | `cn:int, x,y,z:uint(/DMF), yaw:uint, pitch:int, g:uint, [roll:int][dx,dy,dz:int][f:uint]` | `movechecks`（无速度）；落点必须在开放格 |
| `SV_POSC` | 5bit cn + 2bit usefactor + 位打包 x/y/yaw/pitch/roll/dvel/f/z/scope/shoot/sprint | 同上 + usefactor 必须等于地图因子 + lifesequence 位 |
| `SV_SHOOT` | `cn, id, weap, to(3), [cn,o(3),head(3)]*, -1` | weap==gunselect、冷却、mag>0；to 任意 |
| `SV_SHOOTC` | `cn, id, weap` | 同上；to=(0,0,0) |
| `SV_EXPLODE` | `cn, id, weap(GUN_GRENADE/GUN_KNIFE), flags, x,y,z` | 手雷需 `grenades.remove(flags)`；刀需 `knives.removeany()`；无位置校验 |
| `SV_RELOAD` | `cn, id, weap` | ammo≥1 & mag 未满 |
| `SV_WEAPCHANGE` | `cn, weap` | 范围 0..NUMGUNS，无拥有校验 |
| `SV_QUICKSWITCH` | `cn` | 无 |
| `SV_LOADOUT` | `np, ns, perk1, perk2` | 复活时才应用 |
| `SV_THROWNADE` | `cn, from(3), vel(3), cooked` | `throwable>0`，FX only |
| `SV_THROWKNIFE` | `cn, from(3), vel(3)` | `knives.throwable>0`，FX only |
| `SV_SPAWN` | `cn, lifesequence, x,y,z` | XY 距服务器点 ≤6.6；Z 无校验 |
| `SV_SOUND` | `cn, snd` | 仅 4 种音效 |
| `SV_CLIENTPING` | `ping` | 无 |
| `SV_STREAKUSE` | `x,y,z` | 需 airstrikes>0 |
| `SV_PINGPONG` | `millis` | 回显 |

## 附录 B：精确公式与常量

```
DMF = 16.0（位置量化）  DNF = 100.0（速度量化）  DVELF = 4.0（速度增量量化）
CUBES_PER_METER = 4     PLAYERHEIGHT = 4.5     PLAYERRADIUS = 1.1
HEADSIZE = 0.4          TORSOPART = 0.35       LEGPART = 0.65
CROUCHTIME = 500ms      CROUCHHEIGHTMUL = 0.75
SWITCHTIME = 400/200ms（TIME perk）   ADSTIME = 300/200ms（TIME perk）
SPAWNPROTECT = 1000~2000ms（按模式）  DEATHMILLIS = 300ms
HEALTHSCALE = 10        STARTHEALTH = 100*HEALTHSCALE
```

服务端 spread（普通枪）：

```
spread = guns[weap].spread × (|cs.vel|/3 + 0.4) × 1.2 × crouchfactor   (int)
factor = (|to-from|/1000) × adsfactor × (perk2==STEADY ? 0.75 : 1)
to += (rnd(spread) − spread/2) × factor     // 每轴独立，同一 rnd(spread) 三轴？→ 注意实现
半角 ≈ spread × factor / (2 × |to-from|) = spread × ads × perk / 2000
adsfactor：未开镜=1；开镜后随 (now−scopemillis)/zoomtime 线性降到 spreadrem/100
```

> 注：`applyspread` 对 x/y/z 三轴分别调用 `rnd()`（`serverballistics.h:74`）。

爆炸伤害：`effectiveDamage(weap, dist)`，普通 = range 内满伤、endrange 处减 `rangesub`；
手雷/RPG 用倒数或二次衰减（`server.h:682-709`）。

## 附录 C：关键代码索引（acr-git @ 1bf1c9dc）

| 主题 | 位置 |
|---|---|
| 位置包解析 | `server.cpp:4102-4193` |
| movechecks / 摔落击杀 / 拾取 | `server.cpp:3265-3541` |
| checkmove 无速度检测 | `serverchecks.h:361-376` |
| checkpos 地图边界 | `serverworld.h:85-123` |
| 射击事件处理 | `serverevents.h:70-263` |
| spread 公式 | `serverevents.h:94-115` |
| parsepos / lagtrust | `serverballistics.h:104-124` |
| hitplayer（射手视角胶囊） | `serverballistics.h:40-69` |
| shot/穿透/跳弹 | `serverballistics.h:369-538` |
| 事件时间 getmillis | `server.h:372-381`；`serverevents.h:332-342` |
| 冷却 updateshot/waitexpired | `server.h:190-204` |
| crouchfactor | `server.h:245-253` |
| 武器数据表 | `server.h:644-670` |
| `SV_SHOOT`/`SV_EXPLODE` 解析 | `server.cpp:4023-4068` |
| `SV_SPAWN` | `server.cpp:3979-4001`；客户端回发 `clients2c.cpp:703-779` |
| `SV_WEAPCHANGE`/`QUICKSWITCH` | `server.cpp:3731-3757` |
| `SV_THROWNADE/KNIFE`/`STREAKUSE` | `server.cpp:3759-3808` |
| `SV_LOADOUT` | `server.cpp:3811-3822` |
| overflow / 豁免 | `server.cpp:3543-3557, 4800-4807` |
| 客户端 sendshoot | `weapon.cpp:511-543` |
| 客户端位置发送 | `client.cpp:469-565` |
| 客户端远程插值/dvel | `clients2c.cpp:111-239`；`clientgame.cpp:537-583` |
| 客户端射击入口 | `weapon.cpp:1592-1605` |
| scl 默认值（lagtrust=1） | `protos.h:979-987, 1063-1067` |

---

## 结论速记

1. **ACR 没有服务器权威移动**：`// TODO: detect speedhack` 是字面事实，瞬移=合法操作。
2. **默认服务器信任弹道 hitbox**：头部 50cm 自由球 + 射手决定胶囊 → 爆头磁铁/穿墙头。
3. **`SV_EXPLODE` 是最大的协议洞**：手雷任意点引爆、飞刀指定受害者且无几何校验，
   且可自循环 → 服务端认可的秒杀引擎。
4. **事件时间戳可伪造** → 充能弹匣（同帧齐射）、未来预排枪、冷却跨武器操作。
5. **无传统 anti-aim / hide shots**：因为不需要；防御只靠瞬移/稀疏位置让对手客户端
   外推失准，进攻则靠 lagtrust/协议漏洞。
6. 优先实现（对 cheat 收益/成本比）：**飞刀协议击杀 > 远程手雷 > lagtrust 爆头 >
   充能弹匣 > 泰山压顶 > spread 全伪造**。
