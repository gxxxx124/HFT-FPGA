# Nasdaq TotalView-ITCH 5.0 协议总结（重点：订单操作）

## 一、协议概述
- **适用范围**：纳斯达克股票市场 LLC 提供的直接数据馈送协议，覆盖软件和硬件（FPGA）版本。
- **核心功能**：
  - 订单级数据追踪（新增、修改、执行、删除）。
  - 交易消息（普通交易、交叉交易、错误交易）。
  - 市场事件通知（开盘、收盘、熔断、停牌等）。

---

## 二、添加订单消息（Add Order Message）
### 1.3.1 Add Order – No MPID Attribution
- **消息类型**：`A`
- **用途**：未归属的订单（默认 MPID 为 "NSDQ"）。
- **字段说明**：

| 字段名               | 偏移量 | 长度  | 类型    | 描述                                                                 |
|----------------------|--------|-------|---------|----------------------------------------------------------------------|
| Message Type         | 0      | 1     | Alpha   | 'A'（无 MPID 归因）                                                  |
| Stock Locate         | 1      | 2     | Integer | 股票定位码（当日唯一）                                                |
| Tracking Number      | 3      | 2     | Integer | 纳斯达克内部追踪号                                                    |
| Timestamp            | 5      | 6     | Integer | 纳秒级时间戳（自午夜起）                                              |
| Order Reference Number | 11     | 8     | Integer | 订单唯一参考号（当日唯一）                                              |
| Buy/Sell Indicator   | 19     | 1     | Alpha   | 'B'（买）/'S'（卖）                                                   |
| Shares               | 20     | 4     | Integer | 订单股份数                                                             |
| Stock                | 24     | 8     | Alpha   | 股票代码（右对齐空格填充）                                              |
| Price                | 32     | 4     | Price(4)| 显示价格（4 位小数）                                                    |

---

### 1.3.2 Add Order with MPID Attribution
- **消息类型**：`F`
- **用途**：归属特定市场参与者的订单。
- **字段说明**：

| 字段名               | 偏移量 | 长度  | 类型    | 描述                                                                 |
|----------------------|--------|-------|---------|----------------------------------------------------------------------|
| Message Type         | 0      | 1     | Alpha   | 'F'（带 MPID 归因）                                                   |
| Stock Locate         | 1      | 2     | Integer | 股票定位码                                                            |
| Tracking Number      | 3      | 2     | Integer | 内部追踪号                                                            |
| Timestamp            | 5      | 6     | Integer | 纳秒级时间戳                                                          |
| Order Reference Number | 11     | 8     | Integer | 订单参考号                                                            |
| Buy/Sell Indicator   | 19     | 1     | Alpha   | 'B'/'S'                                                               |
| Shares               | 20     | 4     | Integer | 股份数                                                                |
| Stock                | 24     | 8     | Alpha   | 股票代码                                                              |
| Price                | 32     | 4     | Price(4)| 显示价格                                                              |
| Attribution          | 36     | 4     | Alpha   | 市场参与者 ID（MPID）                                                  |

---

## 三、修改订单消息（Modify Order Messages）
### 1.4.1 Order Executed Message
- **消息类型**：`E`
- **用途**：订单部分或全部成交。
- **字段说明**：

| 字段名               | 偏移量 | 长度  | 类型    | 描述                                                                 |
|----------------------|--------|-------|---------|----------------------------------------------------------------------|
| Message Type         | 0      | 1     | Alpha   | 'E'（订单执行）                                                      |
| Stock Locate         | 1      | 2     | Integer | 股票定位码                                                            |
| Tracking Number      | 3      | 2     | Integer | 内部追踪号                                                            |
| Timestamp            | 5      | 6     | Integer | 时间戳                                                              |
| Order Reference Number | 11     | 8     | Integer | 原始订单参考号                                                        |
| Executed Shares      | 19     | 4     | Integer | 成交股份数                                                            |
| Match Number         | 23     | 8     | Integer | 成交唯一标识（用于错误交易回溯）                                        |

---

### 1.4.2 Order Executed With Price Message
- **消息类型**：`C`
- **用途**：订单执行价格与原显示价格不同。
- **字段说明**：

| 字段名               | 偏移量 | 长度  | 类型    | 描述                                                                 |
|----------------------|--------|-------|---------|----------------------------------------------------------------------|
| Message Type         | 0      | 1     | Alpha   | 'C'（带价格的订单执行）                                               |
| Stock Locate         | 1      | 2     | Integer | 股票定位码                                                            |
| Tracking Number      | 3      | 2     | Integer | 内部追踪号                                                            |
| Timestamp            | 5      | 6     | Integer | 时间戳                                                              |
| Order Reference Number | 11     | 8     | Integer | 原始订单参考号                                                        |
| Executed Shares      | 19     | 4     | Integer | 成交股份数                                                            |
| Match Number         | 23     | 8     | Integer | 成交标识                                                              |
| Printable            | 31     | 1     | Alpha   | 'Y'（可打印）/'N'（不可打印，用于批量交易）                            |
| Execution Price      | 32     | 4     | Price(4)| 实际成交价格                                                          |

---

### 1.4.3 Order Cancel Message
- **消息类型**：`X`
- **用途**：部分取消订单。
- **字段说明**：

| 字段名               | 偏移量 | 长度  | 类型    | 描述                                                                 |
|----------------------|--------|-------|---------|----------------------------------------------------------------------|
| Message Type         | 0      | 1     | Alpha   | 'X'（订单取消）                                                      |
| Stock Locate         | 1      | 2     | Integer | 股票定位码                                                            |
| Tracking Number      | 3      | 2     | Integer | 内部追踪号                                                            |
| Timestamp            | 5      | 6     | Integer | 时间戳                                                              |
| Order Reference Number | 11     | 8     | Integer | 原始订单参考号                                                        |
| Cancelled Shares     | 19     | 4     | Integer | 取消的股份数                                                          |

---

### 1.4.4 Order Delete Message
- **消息类型**：`D`
- **用途**：完全删除订单。
- **字段说明**：

| 字段名               | 偏移量 | 长度  | 类型    | 描述                                                                 |
|----------------------|--------|-------|---------|----------------------------------------------------------------------|
| Message Type         | 0      | 1     | Alpha   | 'D'（订单删除）                                                      |
| Stock Locate         | 1      | 2     | Integer | 股票定位码                                                            |
| Tracking Number      | 3      | 2     | Integer | 内部追踪号                                                            |
| Timestamp            | 5      | 6     | Integer | 时间戳                                                              |
| Order Reference Number | 11     | 8     | Integer | 原始订单参考号                                                        |

---

### 1.4.5 Order Replace Message
- **消息类型**：`U`
- **用途**：取消并替换订单。
- **字段说明**：

| 字段名               | 偏移量 | 长度  | 类型    | 描述                                                                 |
|----------------------|--------|-------|---------|----------------------------------------------------------------------|
| Message Type         | 0      | 1     | Alpha   | 'U'（订单替换）                                                      |
| Stock Locate         | 1      | 2     | Integer | 股票定位码                                                            |
| Tracking Number      | 3      | 2     | Integer | 内部追踪号                                                            |
| Timestamp            | 5      | 6     | Integer | 时间戳                                                              |
| Original Order Reference | 11     | 8     | Integer | 原订单参考号                                                          |
| New Order Reference Number | 19     | 8     | Integer | 新订单参考号                                                          |
| Shares               | 27     | 4     | Integer | 新股份数                                                              |
| Price                | 31     | 4     | Price(4)| 新显示价格                                                            |

---

## 四、关键规则
1. **订单状态追踪**：
   - 通过 `Order Reference Number` 关联原始订单与修改消息。
   - 可通过多次修改消息累计更新订单状态（如多次部分成交）。
2. **价格表示**：
   - 整数存储，隐含小数位数（如 `Price(4)` 表示 4 位小数）。
3. **时间戳**：
   - 纳秒级精度，自午夜起计算。
4. **MPID 归因**：
   - 带 MPID 的订单需明确市场参与者身份。

---

## 五、参考文档
- [NASDAQ TotalView-ITCH 5.0 规范](https://example.com/NQTVITCHspecification.pdf)

