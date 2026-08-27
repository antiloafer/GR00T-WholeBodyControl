# 刷卡开门 VLA 任务 — 进度同步与分工

> 本文档用于任务成员之间同步进度、对齐分工，随进展持续更新。

## 1. 项目目标

在 Unitree G1 + SONIC 全身控制框架上，微调 VLA 模型（Isaac-GR00T N1.7），使其自主完成「走向门 → 刷卡 → 开门 → 通过」任务。

## 2. 当前进度（截至 2026-08-26）

| 事项 | 状态 | 说明 |
|------|------|------|
| VR 全身遥操作流程 | ✅ 已跑通 | 真实机器人可被 PICO 遥操 |
| 相机 server 端 | ✅ 已修改并部署 | 替换为 Intel RealSense D455f，固定于机器人本体 |
| 手部动作控制链 | ⬜ 未开始 | BrainCo 二代五指灵巧手 |
| 数据采集 | ⬜ 未开始 | 阶段一待启动 |
| 模型微调 | ⬜ 未开始 | 阶段二待启动 |

## 3. 与官方配置的硬件差异

本项目与官方（GR00T-WholeBodyControl / GEAR-SONIC）默认配置存在两处关键差异，后续所有改动都围绕它们展开：

| 组件 | 官方默认 | 本项目 | 影响 |
|------|----------|--------|------|
| 手部 | Unitree Dex3 三指手（7 DOF/手） | BrainCo 二代五指灵巧手 | 动作空间维度、手部控制协议 |
| 相机 | Luxonis OAK 相机 | Intel RealSense D455f | 相机驱动、安装位置 |

## 4. 遥操作启动流程（真实机器人）

### 4.1 机器人端（已部署，开机自动启动）

相机 server 以 systemd 服务形式运行，无需手动启动：

```bash
# 查看状态
sudo systemctl status composed_camera_server.service

# 查看日志
journalctl -u composed_camera_server.service -f
```

### 4.2 工作站端（笔记本，共 2 个终端）

**终端 1 — C++ 部署（SONIC 全身控制，Docker 方式）**

> 前置条件：已安装 Docker 且当前用户在 docker 组（`sudo usermod -aG docker $USER`）；主机已设置 TensorRT 路径（`export TensorRT_ROOT=/path/to/TensorRT`）。

```bash
cd gear_sonic_deploy
./docker/run-ros2-dev.sh          # 启动容器并进入交互式 shell

# 进入容器后（首次运行需先编译，之后可跳过）：
just build

./deploy.sh --input-type zmq_manager real
# 等待出现 "Init done"
```

> 说明：`run-ros2-dev.sh` 进入容器后已自动 `source scripts/setup_env.sh`，无需手动执行。

**终端 2 — PICO 遥操 Streamer**

```bash
source .venv_teleop/bin/activate
python gear_sonic/scripts/pico_manager_thread_server.py --manager
# 首次调试可加可视化：--vis_vr3pt --vis_smpl
```

> 若 C++ 部署与 PICO streamer 不在同一台机器，需给 deploy 加 `--zmq-host <PICO机器IP>`。

### 4.3 PICO 操作序列

1. 穿戴 PICO 头显 + 手柄 + 脚部 tracker，**穿紧身裤**（宽松衣物会遮挡 tracker）
2. 站立在**校准姿势**：直立、双脚并拢、手臂自然下垂
3. 启动pico端程序
4. 按 **A+B+X+Y** → 启动策略 + 首次全量校准（CALIB_FULL）
5. 按 **A+X** → 进入 POSE 模式（全身遥操激活，机器人跟随你的动作）
6. 再按 **A+X** → 切回 PLANNER（待机）
7. 按 **A+B+X+Y** → 停止策略（急停）

### 4.4 常用控制键速查

| 动作 | 按键 |
|------|------|
| 启动 / 停止策略 | A+B+X+Y |
| 切换 POSE ↔ PLANNER | A+X |
| 切换 POSE ↔ PLANNER_FROZEN_UPPER | B+Y |
| 进入/退出 VR_3PT | 左摇杆按下 |
| 手部抓握 | 对应手的 Trigger |
| 切换步态模式（前进） | A+B |
| 切换步态模式（后退） | X+Y |
| 移动方向 | 左摇杆 |
| 转向（yaw） | 右摇杆 |
| 暂停 pose 推流 | 按住 Menu |
| **键盘急停** | **O**（在 C++ 终端） |

> ⚠️ 切模式前务必先让身体姿态与机器人当前姿态对齐，否则机器人会剧烈动作。

## 5. 任务与分工

### 阶段一：少量采集，可行性验证

**目标**：用最小成本验证「数据有效性 + 手部动作可行」，不追求数量。

| 负责人 | 任务 | 产出 / 验收标准 |
|--------|------|-----------------|
| 耿 | 采集 10 条开门数据（**不带手部动作控制**） | 数据能正常回放，视频清晰拍到门与刷卡器，关节数据无异常，episode 完整 |
| 樊 | 设计手部动作，打通手部动作控制链 | 明确 BrainCo 手自由度 N；定义 3~4 个固定手势；手柄按键可切换手势 |

**阶段一完成标准**：
- 10 条数据通过数据有效性检查（见第 6 节）
- 手部能通过手柄按键在固定手势间切换（哪怕尚未接入采集）

### 阶段二：批量采集 + 首次微调

**目标**：凑够数据量，跑通一次完整的「采集 → 微调 → 部署」闭环。

| 负责人 | 任务 | 产出 |
|--------|------|------|
| 耿、樊 | 轮流采集，累计 **50 条**数据 | LeRobot v2.1 数据集（含手部动作） |
| 樊（或指定） | 用 Isaac-GR00T 微调模型 | 首个微调 checkpoint + 训练曲线 |

**阶段二完成标准**：微调命令成功跑完、产出 checkpoint，且模型在真机/回放上能输出合理动作（不要求高成功率）。

### 阶段三：扩量到 100 条

**目标**：在阶段二验证微调链路可行后，继续补数据提升成功率。

| 负责人 | 任务 |
|--------|------|
| 耿、樊 | 轮流采集，累计至 **100 条** |

**阶段三完成标准**：100 条数据入库，重复微调迭代，成功率达到可接受水平。

## 6. 数据采集操作要点

### 6.1 启动数据采集

阶段一推荐使用 all-in-one 启动器（自动拉起 C++ 部署 + PICO 遥操 + 数据采集 + 相机查看四个面板）：

```bash
python gear_sonic/scripts/launch_data_collection.py \
    --camera-host <机器人IP> \
    --task-prompt "walk to the door, tap the access card on the card reader, then open the door"
```

> 机器人默认 IP 为 `192.168.123.164`。请统一 task-prompt，保证所有数据标注一致。

### 6.2 录制控制

| 输入 | 动作 |
|------|------|
| 左手 Grip + A | 开始 / 停止录制（保存当前 episode） |
| 左手 Grip + B | 丢弃当前 episode（标记为废片） |
| 键盘 `c` | 同「开始/停止」 |
| 键盘 `x` | 同「丢弃」 |

### 6.3 采集后清洗

```bash
source .venv_data_collection/bin/activate
python gear_sonic/scripts/process_dataset.py \
    --dataset-path outputs/<数据集名> \
    --output-path outputs/<数据集名>_cleaned
```

### 6.4 数据有效性检查清单（阶段一验收用）

- [ ] ego_view 视频能清楚看到门把手和刷卡器
- [ ] 关节角度无 NaN、无跳变、无大面积零值
- [ ] episode 完整（从起点到通过门，无中途断录）
- [ ] 视频帧与动作数据时间对齐
- [ ] task_description 标注正确

## 7. 潜在问题与风险

### 7.1 门类型待确认（自动 vs 手动）

- 若为**自动玻璃门**：任务简化为「走向门 → 刷卡 → 等待门开 → 通过」，**省去抓把手拉门的精细动作**。
- 需新增关注点：门开合的感应范围与延迟、机器人站位、等待时长。
- 若为**手动门**：需保留「抓把手拉门」动作，对手部动作要求更高。

> **待确认**：实际场地的门是自动还是手动？这会直接影响 prompt 与动作设计，建议阶段一启动前先定。

### 7.2 笔记本部署

数据采集程序部署在笔记本上，笔记本 + 机器人组成采集系统，需满足：

- 笔记本需 NVIDIA GPU（C++ 部署依赖 TensorRT）
- 笔记本、机器人、PICO 需在同一局域网，**网络延迟尽量 < 30ms**（建议有线或 5GHz）
- 笔记本磁盘空间充足（视频 + parquet 占用较大）

### 7.3 手部动作的兼容性

阶段一采集的数据**不含手部动作**（手部字段为零值）。后续接入 BrainCo 手后，动作空间维度会从「每手 7 维」变为「每手 N 维」，阶段一数据在维度上可能与阶段二不兼容，需提前规划：

- 阶段一数据主要用于验证**全身轨迹 + 视觉**的有效性
- 阶段二正式采集时再统一手部维度，避免新旧数据混用

## 8. 补充建议

1. **手部动作设计建议（樊）**：先确认 BrainCo 手自由度 N 与通信协议（CAN/串口/SDK），定义 4 个固定手势即可覆盖开门任务——`开手`、`捏卡`、`握把手`、`握拳`，通过手柄 Trigger/Grip 或数字键切换，无需连续主从映射。

2. **数据多样性**：采集时变化机器人起始站位、朝向、光照，避免模型死记固定位置（详见「开门任务」对空间定位的要求）。

3. **微调基线**：`launch_finetune.py` 使用 base 模型 `nvidia/GR00T-N1.7-3B`，建议保留 `--color-jitter-params`（光照增强），20k 步起步。

4. **安全**：遥操有危险性，采集时务必一人操作、一人在键盘旁随时准备按 `O` 急停，保持 3 米安全区。

---

*本文档随进展持续更新。*
