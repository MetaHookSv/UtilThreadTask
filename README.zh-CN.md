# UtilThreadTask

从 [MetaHookSv](https://github.com/hzqst/MetaHookSv) 拆分的独立任务队列。
多个线程可以提交任务；任务在调用 `RunTask` / `RunTasks` 的线程执行。
调度器自身不创建工作线程。

保留原来的 `UtilThreadTask.dll`、`CreateInterface` 导出和
`UtilThreadTaskFactory_001` ABI。源码基线为
`hzqst/MetaHookSv@fe80b6d60bfb487b52aed7ea7ec0492e7b27a5d2` 中的
`PluginLibs/UtilThreadTask` 和 `include/Interface/IUtilThreadTask.h`。

## 构建与测试

需要 Windows、Visual Studio 2022（C++ x86 工具和 Windows SDK）、CMake 3.21
或更新版本，以及 Git。Debug / Release 使用 C++20、静态 MSVC runtime
和 VC-LTL 5.3.1。

```bat
scripts\build-UtilThreadTask-x86-Release.bat
scripts\build-UtilThreadTask-x86-Debug.bat
```

脚本依次配置、构建、运行 CTest、安装；失败立即退出。
DLL 和 PDB 安装到
`install\x86\<Configuration>\svencoop\metahook\dlls`。
将 DLL 复制到游戏的 `svencoop/metahook/dlls` 目录即可。
此库由使用方加载，无需加入 `plugins.lst`。

直接使用 CMake：

```bat
cmake -S . -B build/x86/Release -G "Visual Studio 17 2022" -A Win32 -DCMAKE_INSTALL_PREFIX=install/x86/Release
cmake --build build/x86/Release --config Release --parallel
ctest --test-dir build/x86/Release -C Release --output-on-failure
cmake --install build/x86/Release --config Release
```

`BUILD_TESTING` 默认开启；配置时传入 `-DBUILD_TESTING=OFF` 可仅构建库。
其他生成器也需要使用 MSVC x86；单配置生成器还须指定
`-DCMAKE_BUILD_TYPE=Debug` 或 `Release`。

## 依赖

首次配置自动获取固定提交
`4d23b6fecd79dc949aabc2e145480cd1328d4a35` 的 MetaHook SDK，
不拉取子模块、不构建 launcher；VC-LTL 5.3.1 下载到 `thirdparty/cache`，
并通过以下 SHA-256 校验：
`7a18799ed3aa84a225610a5447a56bc534c5c98ccb8dec05caba0e3f633431ad`。

本地或离线构建可指定已有依赖：

```bat
scripts\build-UtilThreadTask-x86-Release.bat -DMETAHOOK_SOURCE_PATH=D:\MetaHook -DVC_LTL_Root=D:\VC-LTL-5.3.1
```

初始化 CMake cache 时也接受同名环境变量，显式 CMake 参数优先。
外部依赖目录只读。`METAHOOK_SOURCE_PATH` 需要提供
`include/HLSDK/common/interface.h`、`interface.cpp` 和 `LICENSE`；
`VC_LTL_Root` 需要指向解压后的 VC-LTL 二进制包。
下载缓存位置可通过 `UTILTHREADTASK_DEPENDENCY_CACHE_DIR` 覆盖。

## 接口与生命周期

加载 DLL，将 `CreateInterface` 导出转换为 `CreateInterfaceFn`，
使用 `UTIL_THREAD_TASK_FACTORY_INTERFACE_VERSION` 获取 `IUtilThreadTaskFactory`，
再调用 `CreateThreadedTaskScheduler()`。安装的头文件分别位于
`include/Interface` 和 `include/HLSDK/common`；使用方需要添加这两个 include 路径。

- `QueueTask(task)` 在队尾入队，`QueueTask(task, true)` 在队首入队。
  入队后任务由调度器管理；指针必须有效，`Destroy()` 应在分配对象的模块中释放它。
- `RunTask(time)` 查找第一个 `ShouldRun(time)` 为真的任务，调用
  `Run(time)` 和 `Destroy()`，返回是否执行了任务。
- `RunTasks(time, maxTasks)` 最多执行指定数量；零或负数表示不限数量。
  未就绪任务保留在队列中，不阻挡后面的就绪任务。普通 `Run` 回调可以继续入队。
- `IsCurrentThreadCreatorThread()` 仅检查创建线程，执行方法不强制线程归属。
- `WaitForAllTasksToComplete()` 同步对所有剩余任务调用 `Run(FLT_MAX)`
  和 `Destroy()`，忽略 `ShouldRun`。它是关闭时的排空操作，不等待后台线程。
  单独调用调度器的 `Destroy()` 会释放剩余任务，不执行它们。

关闭或销毁前先停止生产者并结束执行调用。
关闭操作持有队列锁执行回调；关闭和销毁回调不得修改或递归排空调度器。
任务回调不得抛出异常。卸载 DLL 前须销毁所有调度器。

## 回归测试

十个 CTest 场景通过公开接口动态加载实际 DLL，覆盖 factory 获取、队列顺序、
就绪条件、执行上限、关闭排空、任务恰好释放一次、线程识别、多生产者并发入队
和执行中继续入队。Release 下检查仍然生效，每个场景超时为 30 秒。
测试无需安装游戏或额外测试框架。

## 许可证

MIT，详见 [LICENSE](LICENSE) 和 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。
