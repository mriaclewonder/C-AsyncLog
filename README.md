# AsyncLog

本项目学习B站up主恋恋风尘zack，链接

[异步日志系统](https://www.bilibili.com/video/BV14HR8YkEkw/?spm_id_from=333.1387.collection.video_card.click&vd_source=a71cbeb980bf33cb4481cf8fca37f2eb)

一个轻量级的 C++ **异步日志库**。调用方将日志消息提交到线程安全队列后立即返回，由后台工作线程负责将消息顺序写入日志文件，从而避免同步磁盘 I/O 阻塞业务线程。

## 特性

- **异步写入**：`Logger::log()` 只负责入队，日志落盘由独立工作线程完成。
- **线程安全**：基于 `std::mutex` + `std::condition_variable` 的生产者-消费者队列，多线程可同时调用 `log()`。
- **格式化输出**：支持 `{}` 占位符，按参数顺序替换，风格类似 `fmt` / `std::format`。
- **类型友好**：任意可通过 `operator<<` 写入 `std::ostream` 的类型均可作为参数。
- **优雅退出**：析构时关闭队列并 `join` 工作线程，保证队列中剩余的日志全部写完。
- **追加写入**：日志文件以追加模式打开，多次运行不会覆盖历史内容。

## 目录结构

```
AsyncLog.sln            # Visual Studio 解决方案
AsyncLog.vcxproj        # 项目文件（v143 工具集，Win32/x64，Debug/Release）
AsyncLog.vcxproj.filters
AsyncLog.h              # 日志库实现（LogQueue + Logger，头文件形式）
AsyncLog.cpp            # 示例程序
README.md               # 本文档
```

## 环境要求

- Windows
- Visual Studio 2022（MSVC v143 工具集，工程默认使用 Windows 10 SDK）
- 支持 C++11 及以上的编译器（若脱离 VS 工程单独使用，仅需 `AsyncLog.h`）

## 构建与运行

1. 使用 Visual Studio 2022 打开 `AsyncLog.sln`。
2. 选择 `Debug` / `Release` 与 `x64` / `Win32` 配置。
3. 按 `F5` 或 `Ctrl+F5` 生成并运行。

也可以在开发者命令行中使用 MSBuild：

```bat
msbuild AsyncLog.sln /p:Configuration=Release /p:Platform=x64
```

运行后会在程序当前工作目录下生成（或追加）`log.txt`。

## 快速开始

```cpp
#include "AsyncLog.h"

int main() {
    Logger logger("log.txt");   // 打开/创建日志文件并启动后台线程

    logger.log("Starting application.");
    logger.log("Hello {}", "World");
    logger.log("User {} performed {} in {} seconds.", 42, "login", 3.5);

    // logger 析构时会等待队列中剩余的日志全部写入文件
    return 0;
}
```

对应的 `log.txt` 内容：

```text
Starting application.
Hello World
User 42 performed login in 3.5 seconds.
```

## 工作原理

```text
业务线程                      LogQueue                    后台线程
   |                             |                            |
   |-- log("Hello {}", "World") -> push(msg)                  |
   |                             |-- 通知 -------------------> pop()
   |                             |                            |-- 写入 log.txt
   |<-- 立即返回 -----------------|                            |
```

1. `log()` 将格式化后的字符串放入 `LogQueue`。
2. 工作线程阻塞在条件变量上，队列有数据时被唤醒并取出消息。
3. 工作线程将消息逐行写入 `std::ofstream`（追加模式）。
4. 析构时调用 `shutdown()`，工作线程消费完剩余消息后退出，随后关闭文件。
