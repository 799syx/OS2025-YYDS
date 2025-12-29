# 环境搭建说明

## 一、系统要求

### 1.1 操作系统

本项目支持以下操作系统：
- Windows 10/11
- Linux (Ubuntu, CentOS等)
- macOS

### 1.2 硬件要求

- CPU: 任意（无特殊要求）
- 内存: 至少512MB可用内存
- 磁盘: 至少50MB可用空间

## 二、编译环境搭建

### 2.1 Windows环境

#### 方法一：使用MinGW-w64（推荐）

1. **下载MinGW-w64**
   - 访问：https://www.mingw-w64.org/downloads/
   - 或使用MSYS2：https://www.msys2.org/

2. **安装MinGW-w64**
   ```bash
   # 使用MSYS2安装（推荐）
   pacman -S mingw-w64-x86_64-gcc
   pacman -S mingw-w64-x86_64-make
   ```

3. **配置环境变量**
   - 将MinGW的bin目录添加到PATH环境变量
   - 例如：`C:\msys64\mingw64\bin`

4. **验证安装**
   ```bash
   gcc --version
   make --version
   ```

#### 方法二：使用Visual Studio

1. **安装Visual Studio**
   - 下载Visual Studio Community（免费）
   - 安装时选择"C++桌面开发"工作负载

2. **使用Developer Command Prompt**
   - 打开"Developer Command Prompt for VS"
   - 使用cl.exe编译器（需要修改Makefile）

#### 方法三：使用WSL（Windows Subsystem for Linux）

1. **安装WSL**
   ```powershell
   wsl --install
   ```

2. **在WSL中安装GCC**
   ```bash
   sudo apt update
   sudo apt install gcc make
   ```

### 2.2 Linux环境

#### Ubuntu/Debian

```bash
# 更新包管理器
sudo apt update

# 安装GCC和Make
sudo apt install gcc make

# 验证安装
gcc --version
make --version
```

#### CentOS/RHEL

```bash
# 安装GCC和Make
sudo yum install gcc make

# 或使用dnf（较新版本）
sudo dnf install gcc make
```

### 2.3 macOS环境

#### 方法一：使用Xcode Command Line Tools

```bash
# 安装Command Line Tools
xcode-select --install

# 验证安装
gcc --version
make --version
```

#### 方法二：使用Homebrew

```bash
# 安装Homebrew（如果未安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装GCC
brew install gcc

# 验证安装
gcc --version
```

## 三、项目编译

### 3.1 使用Makefile（推荐）

```bash
# 进入项目目录
cd project3035746-352882

# 编译所有程序
make

# 编译特定目标
make test        # 只编译测试程序
make benchmark   # 只编译基准测试程序

# 运行程序
make run-test     # 运行测试程序
make run-benchmark # 运行基准测试

# 清理编译文件
make clean
```

### 3.2 手动编译

#### 编译测试程序

```bash
gcc -Wall -Wextra -std=c99 -O2 -o test \
    src/test.c \
    src/page_replacement.c \
    src/utils.c
```

#### 编译基准测试程序

```bash
gcc -Wall -Wextra -std=c99 -O2 -o benchmark \
    tests/benchmark.c \
    src/page_replacement.c \
    src/utils.c
```

### 3.3 编译选项说明

- `-Wall`: 启用所有警告
- `-Wextra`: 启用额外警告
- `-std=c99`: 使用C99标准
- `-O2`: 优化级别2（性能优化）

## 四、IDE配置（可选）

### 4.1 Visual Studio Code

1. **安装扩展**
   - C/C++ (Microsoft)
   - C/C++ Extension Pack

2. **配置tasks.json**
   ```json
   {
       "version": "2.0.0",
       "tasks": [
           {
               "label": "build",
               "type": "shell",
               "command": "make",
               "group": {
                   "kind": "build",
                   "isDefault": true
               }
           }
       ]
   }
   ```

3. **配置launch.json**（调试配置）
   ```json
   {
       "version": "0.2.0",
       "configurations": [
           {
               "name": "Debug Test",
               "type": "cppdbg",
               "request": "launch",
               "program": "${workspaceFolder}/test",
               "args": [],
               "stopAtEntry": false,
               "cwd": "${workspaceFolder}",
               "environment": [],
               "externalConsole": false,
               "MIMode": "gdb",
               "miDebuggerPath": "gdb",
               "setupCommands": [
                   {
                       "description": "Enable pretty-printing for gdb",
                       "text": "-enable-pretty-printing",
                       "ignoreFailures": true
                   }
               ]
           }
       ]
   }
   ```

### 4.2 CLion

1. **打开项目**
   - File -> Open -> 选择项目目录

2. **配置CMakeLists.txt**（如果需要）
   ```cmake
   cmake_minimum_required(VERSION 3.10)
   project(PageReplacement)
   
   set(CMAKE_C_STANDARD 99)
   
   add_executable(test 
       src/test.c 
       src/page_replacement.c 
       src/utils.c
   )
   
   add_executable(benchmark 
       tests/benchmark.c 
       src/page_replacement.c 
       src/utils.c
   )
   ```

### 4.3 Code::Blocks

1. **创建新项目**
   - File -> New -> Project -> Console Application

2. **添加源文件**
   - 将src目录下的所有.c和.h文件添加到项目

3. **配置编译器**
   - Settings -> Compiler -> 选择GCC编译器
   - 设置C99标准

## 五、常见问题解决

### 5.1 编译错误

#### 错误：找不到头文件
```
fatal error: page_replacement.h: No such file or directory
```
**解决方法**：
- 检查文件路径是否正确
- 确保在项目根目录下编译
- 检查include路径设置

#### 错误：未定义的引用
```
undefined reference to 'fifo_algorithm'
```
**解决方法**：
- 确保所有源文件都参与编译
- 检查函数声明和定义是否匹配

### 5.2 运行时错误

#### 错误：段错误（Segmentation Fault）
**可能原因**：
- 数组越界
- 空指针访问
- 内存未初始化

**解决方法**：
- 使用调试器（gdb）定位问题
- 检查数组边界
- 检查指针是否为空

#### 错误：内存泄漏
**解决方法**：
- 使用valgrind检查内存泄漏
- 确保所有malloc都有对应的free

### 5.3 Windows特定问题

#### 问题：make命令不存在
**解决方法**：
- 安装MinGW-w64或MSYS2
- 或使用nmake（Visual Studio）
- 或手动编译

#### 问题：中文路径问题
**解决方法**：
- 尽量使用英文路径
- 或使用WSL

## 六、验证安装

### 6.1 快速验证

```bash
# 1. 检查编译器
gcc --version

# 2. 检查Make工具
make --version

# 3. 编译项目
make

# 4. 运行测试
./test

# 5. 运行基准测试
./benchmark
```

### 6.2 预期输出

如果环境配置正确，应该能够：
- ✅ 成功编译项目（无错误和警告）
- ✅ 成功运行测试程序
- ✅ 看到算法性能对比结果

## 七、开发工具推荐

### 7.1 代码编辑器
- Visual Studio Code（跨平台，免费）
- CLion（JetBrains，付费）
- Code::Blocks（跨平台，免费）

### 7.2 调试工具
- GDB（命令行调试器）
- Visual Studio Debugger（Windows）
- LLDB（macOS/Linux）

### 7.3 代码分析工具
- Valgrind（内存检查）
- Cppcheck（静态分析）
- Clang Static Analyzer

## 八、获取帮助

如果遇到问题，可以：
1. 查看项目README.md
2. 查看设计文档和实验指导书
3. 检查代码注释
4. 使用搜索引擎查找错误信息

---

**注意**：本项目使用标准C语言（C99），不依赖任何第三方库，可以在任何支持C99的编译环境中编译运行。

