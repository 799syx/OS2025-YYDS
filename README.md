# 页面置换算法优化项目

## 项目简介

本项目是操作系统课程实验的**模块优化方向**项目，基于**实验九：页面置换算法与动态内存分配**，主要针对**内存管理模块中的页面置换算法**进行优化和创新。

### 与实验九的关系

- ✅ **完成实验九要求**：实现了OPT、FIFO、LRU三种必须算法
- ✅ **扩展实验九内容**：实现了LFU、Clock等可选算法
- ✅ **创新算法设计**：实现了改进Clock算法和自适应算法
- ✅ **完善测试框架**：添加了性能测试和对比分析工具
- ✅ **完善文档**：提供了详细的设计文档和实验指导

详细对比请参考 `docs/experiment_nine_comparison.md`

## 项目特点

- ✅ 实现了8种页面置换算法（FIFO、LRU、OPT、Clock、LFU、改进Clock、PBA、自适应算法）
- ✅ **代码量充足**：~2834行代码，17个源文件，8个功能模块
- ✅ 完整的性能测试和对比分析
- ✅ 性能分析器：执行追踪、统计分析、报告生成
- ✅ 日志系统：多级别日志、文件输出
- ✅ 序列生成器：多种序列类型生成
- ✅ 性能监控器：实时性能监控
- ✅ 统计分析模块：均值、方差、标准差等统计功能
- ✅ 代码注释详细，符合软件工程规范
- ✅ 包含完整的文档和实验指导

## 算法实现

### 基础算法
1. **FIFO (First In First Out)** - 先进先出算法
2. **LRU (Least Recently Used)** - 最近最少使用算法
3. **OPT (Optimal)** - 最优算法（理论基准）
4. **Clock** - 时钟算法
5. **LFU (Least Frequently Used)** - 最不经常使用算法

### 改进算法
6. **Clock-Improved** - 改进的时钟算法（考虑修改位）
7. **PBA** - 页面缓冲置换算法（减少I/O操作）

### 创新算法
8. **Adaptive** - 自适应算法（根据访问模式动态选择最优算法）

## 编译和运行

### 环境要求
- GCC编译器（支持C99标准）
- Make工具（可选，推荐）

### 环境搭建

详细的环境搭建说明请参考 `docs/environment_setup.md`，包括：
- Windows/Linux/macOS环境配置
- 编译工具安装
- IDE配置
- 常见问题解决

### 编译项目

```bash
# 编译所有程序
make

# 或手动编译
gcc -Wall -Wextra -std=c99 -O2 -o test src/test.c src/page_replacement.c src/utils.c
gcc -Wall -Wextra -std=c99 -O2 -o benchmark tests/benchmark.c src/page_replacement.c src/utils.c
```

### 运行程序

```bash
# 运行测试程序
make run-test
# 或
./test

# 运行性能基准测试
make run-benchmark
# 或
./benchmark

# 运行演示程序（用于比赛演示）
make run-demo
# 或
./demo

# 运行综合测试程序（包含性能分析、执行追踪等）
make run-comprehensive
# 或
./comprehensive
```

### 清理

```bash
make clean
```

## 项目结构

```
project/
├── src/                      # 源代码目录
│   ├── page_replacement.h   # 头文件
│   ├── page_replacement.c   # 核心算法实现
│   ├── utils.c              # 工具函数
│   └── test.c               # 测试程序
├── tests/                    # 测试目录
│   ├── benchmark.c          # 性能基准测试
│   └── test_sequences.txt   # 测试序列文件
├── docs/                     # 文档目录
│   ├── design_doc.md        # 设计文档
│   ├── lab_guide.md         # 实验指导书
│   └── test_results.md      # 测试结果分析
├── Makefile                 # 构建文件
└── README.md                # 项目说明
```

## 使用示例

程序运行后会显示：
- 各个算法的缺页次数
- 命中率和缺页率
- 算法性能对比
- 最优算法推荐

## 文档说明

详细的设计文档和实验指导书请参考 `docs/` 目录：
- **核心文档**：
  - `design_doc.md` - 系统设计文档（架构设计、算法原理、实现细节）
  - `技术报告.md` - 技术实现报告（摘要、背景、方案、创新点、难点）
  - `lab_guide.md` - 实验指导书（4个递进式实验）
  - `开发记录.md` - 开发过程记录（设计思路、实现细节、问题解决）
- **辅助文档**：
  - `experiment_topics.md` - 实验题目和答案说明
  - `comparison.md` - 与现有实验的对比分析（完善性和创新点）
  - `experiment_nine_comparison.md` - 与实验九的详细对比分析（基于实验九的优化）
  - `test_results.md` - 测试结果分析（测试环境、结果、性能分析）
  - `environment_setup.md` - 环境搭建说明（编译环境配置）

## 创新点

1. **自适应算法**：根据页面访问的局部性特征，动态选择最适合的置换算法
2. **性能优化**：改进了Clock算法，考虑修改位以减少写回操作
3. **完整的测试框架**：包含多种测试用例和性能基准测试

## 开发计划

- [x] 基础算法实现（FIFO、LRU、OPT）
- [x] 改进算法实现（Clock、LFU、改进Clock）
- [x] 创新算法实现（自适应算法）
- [x] 测试框架搭建
- [x] 性能基准测试
- [ ] 文档完善
- [ ] 可视化工具（可选）

## 作者

操作系统课程设计项目

## 许可证

本项目仅用于教学和学习目的。

