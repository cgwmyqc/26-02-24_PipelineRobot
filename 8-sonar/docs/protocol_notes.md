# BHJ-3 通信协议说明

## 1. 简述

BHJ-3 管道检测声呐通过百兆以太网与上位机进行通信。协议分为三类：

1. 无连接广播数据包
2. 上位机下发指令包
3. 下位机上传数据包

设备默认网络参数如下：

- 默认设备 IP：`192.168.0.10`
- 默认子网掩码：`255.255.255.0`
- 设备端口：`23`

当前项目联调默认参数如下：

- 声呐 IP：`192.168.1.10`
- 上位机 IP：`192.168.1.132`
- 接收端口：`2002`
- 设备端口：`23`

BHJ-3 支持 DHCP 和静态 IP 两种模式。静态模式下，上位机和声呐 IP 都可由上位机软件修改。

## 2. 无连接广播数据包

声呐上电后，会自动向 UDP 端口 `2002` 广播设备信息描述符，便于上位机搜索连接。

### 报文长度

- 固定 24 字节

### 报文字段

| 字节 | 字段 | 说明 |
| --- | --- | --- |
| 0 | Head | 固定 `0xFE` |
| 1 | Head | 固定 `0x44` |
| 2-3 | Sonar Number | 声呐编号 |
| 4 | Frequency | 工作频率索引，默认 `5`，对应 `2.25MHz` |
| 5 | Angle | 开角，固定 `0x14`，即 1.4 度 |
| 6 | Mini Dist | 最小测量距离，固定 `0x05`，即 5cm |
| 7 | Max Dist | 最大测量距离，固定 `0x60`，即 6m |
| 8 | Version | 版本号，400 点版本为 `001-099`，800 点版本为 `101-199` |
| 9 | DHCP State | `0=关闭`，`1=开启` |
| 10-13 | LocalADDR0-3 | 设备 IP 地址 |
| 14 | IpPoolStart | DHCP 给 PC 分配的地址尾段 |
| 15-22 | Reserved | 保留 |
| 23 | Term | 固定 `0xFD` |

### 频率换算

- `frequency_in_khz = Byte4 * 50 + 2000`
- 默认 `Byte4 = 5`，即 `2250kHz = 2.25MHz`

## 3. 上位机下传指令包（UDP）

上位机通过 35 字节 UDP 指令包切换声呐工作状态并设置参数。

### 报文长度

- 固定 35 字节

### 报文字段

| 字节 | 字段 | 说明 |
| --- | --- | --- |
| 0 | Head | 固定 `0xFE` |
| 1 | Head | 固定 `0x44` |
| 2 | Work Status | `0=开始工作`，`1=停止工作` |
| 3 | Range Index | 量程档位 |
| 4 | Reserved | 固定 `0` |
| 5 | Rev/Hold | Bit6 为电机方向位，其他位为 0 |
| 6 | Reserved | 固定 `0x43` |
| 7 | Reserved | 固定 `0x02` |
| 8 | Start Gain | 强度，`0-20` |
| 9 | LOGF | `0=10dB`，`1=20dB`，`2=30dB`，`3=40dB` |
| 10 | Absorption | 吸收系数，范围 `0-255`，表示 `0.00-2.55 dB/m` |
| 11 | Power | 功率，`0-20` |
| 12 | Reserved | 固定 `0x78` |
| 13 | Reserved | 固定 `0x03` |
| 14 | Pulse Length | 发射脉宽，`1-100` 表示 `10-1000us`，步长 `10us` |
| 15 | Profile MinRange | 轮廓最小范围，单位 `0.01m` |
| 16 | DHCP Type | `0=OFF`，`1=ON`，`0xFF=恢复出厂 IP` |
| 17-20 | LocalADDR0-3 | 设备 IP 地址 |
| 21 | IpPoolStart | PC 地址尾段 |
| 22 | Reserved | 保留 |
| 23 | Calibrate | `0=Normal`，`1=校准到 0 度` |
| 24 | Switch Delay | 默认 `0` |
| 25 | Frequency | 工作频率索引，默认 `5` |
| 26-33 | Time | 64 位毫秒时间戳，Byte26 为高位，Byte33 为低位 |
| 34 | Term | 固定 `0xFD` |

### 量程索引

| Range Index | 量程 |
| --- | --- |
| 4 | 25.0cm |
| 6 | 50.0cm |
| 8 | 75.0cm |
| 10 | 1.0m |
| 20 | 2.0m |
| 30 | 3.0m |
| 40 | 4.0m |
| 50 | 5.0m |
| 60 | 6.0m |

### 方向位

- `Byte5 Bit6 = 0`：正常方向
- `Byte5 Bit6 = 1`：反向

## 4. 下位机上传数据包（UDP）

每次收到转换数据指令后，探头转完一周 `800/400` 步后回传两类数据：

1. 轮廓数据：每完成一个步进回传一包
2. 能量数据：完成一周后回传一包

### 报文结构

| 字节 | 字段 | 说明 |
| --- | --- | --- |
| 0-2 | Head ASCII | 固定 `'ISX'`，即 `0x49 0x53 0x58` |
| 3 | Head ID | `0x10=轮廓数据`，`0x20=能量数据` |
| 4 | Link Status | 高 4 位是总包数，低 4 位是当前包号 |
| 5-6 | Head Pos | 探头位置和方向位 |
| 7 | Range | 量程索引 |
| 8-9 | Prof Rng | 当前包采样点数 |
| 10-11 | Data Bytes | 当前帧总采样点数 |
| 12-19 | Time | 64 位毫秒时间戳 |
| 20-(20+DataBytes-1) | Data | 采样数据，每点 1 字节 |
| 最后 1 字节 | Term | 固定 `0xFC` |

### Link Status

- `Byte4 Bit3-0`：当前包号
- `Byte4 Bit7-4`：总包数

示例：

- `0x21`：总共 2 包，当前第 1 包
- `0x22`：总共 2 包，当前第 2 包

### Head Pos 和方向位

#### 位定义

- `Byte5`：`Head Pos Low`
- `Byte6 Bit0`：`Head Pos Low` 的最高位
- `Byte6 Bit1-5`：`Head Pos High`
- `Byte6 Bit6`：方向位

#### 计算公式

- `Head Pos High Byte = (Byte6 & 0x3E) >> 1`
- `Head Pos Low Byte = ((Byte6 & 0x01) << 7) | (Byte5 & 0x7F)`
- `Head Position = (Head Pos High Byte << 8) | Head Pos Low Byte`
- `Angle = 0.45 * Head Position`

#### 方向定义

- `Step Direction = (Byte6 & 0x40) >> 6`
- `0 = 顺时针`
- `1 = 逆时针`

### Range

`Byte7` 的含义与下发控制包中的 `Range Index` 一致。

### Prof Rng

表示当前包采样点数，解码公式如下：

- `Data Length High Byte = (Byte9 & 0x3E) >> 1`
- `Data Length Low Byte = ((Byte9 & 0x01) << 7) | (Byte8 & 0x7F)`
- `Data Length = (Data Length High Byte << 8) | Data Length Low Byte`

### Data Bytes

表示当前帧总采样点数，解码公式如下：

- `Data Bytes Length High Byte = (Byte11 & 0x7E) >> 1`
- `Data Bytes Length Low Byte = ((Byte11 & 0x01) << 7) | (Byte10 & 0x7F)`
- `Data Bytes = (Data Bytes Length High Byte << 8) | Data Bytes Length Low Byte`

### 数据区说明

- 从 `Byte20` 开始是采样数据
- 每个采样点占 1 字节
- 每个采样单位对应 `2mm`
- 声速基准为 `1500m/s`

## 5. 当前项目实现约定

当前 `8-sonar` 工程采用以下约定：

- 绑定本机 `192.168.1.132:2002`
- 向设备 `192.168.1.10:23` 发送 35 字节控制包
- 默认启动时发送 `Work Status=0`
- 默认退出时发送 `Work Status=1`
- 按 `Byte4` 的高低 4 位执行分包重组
- 解析后的角度统一输出为 `angle_deg`
- 采样基础距离轴统一按 `sample_distance_m = index * 0.002`

### 四方向特征距离输出

项目当前会额外输出 `0°`、`90°`、`180°`、`270°` 四个方向的特征距离。

- 该输出只针对 `Head ID = 0x10` 的轮廓数据
- 该输出不是协议原生字段，而是应用层派生结果
- 角度匹配采用 `±0.45°` 容差
- 特征距离定义为：该方向轮廓帧中“最强回波样本”对应的距离
- 当多个样本幅值相同，优先取距离最近的那个
