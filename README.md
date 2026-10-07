# UtilThreadTask

A standalone task queue extracted from [MetaHookSv](https://github.com/MetaHookSv/MetaHookSv).
Multiple threads can submit tasks; tasks execute on the thread calling `RunTask`
or `RunTasks`. The scheduler creates no worker threads.

The port preserves `UtilThreadTask.dll`, the `CreateInterface` export, and the
`UtilThreadTaskFactory_001` ABI. Source baseline:
`MetaHookSv/MetaHookSv@fe80b6d60bfb487b52aed7ea7ec0492e7b27a5d2`,
`PluginLibs/UtilThreadTask` and `include/Interface/IUtilThreadTask.h`.

## Build and test

Requirements: Windows, Visual Studio 2022 with the C++ x86 tools and Windows SDK,
CMake 3.21 or newer, and Git. Debug and Release use C++20, static MSVC runtime,
and VC-LTL 5.3.1.

```bat
scripts\build-UtilThreadTask-x86-Release.bat
scripts\build-UtilThreadTask-x86-Debug.bat
```

Each script configures, builds, runs CTest, and installs, stopping on failure.
The DLL and PDB are installed under
`install\x86\<Configuration>\svencoop\metahook\dlls`.
Copy `UtilThreadTask.dll` to the game's `svencoop/metahook/dlls` directory.
This library is loaded by its consumers and does not need a `plugins.lst` entry.

Equivalent CMake commands:

```bat
cmake -S . -B build/x86/Release -G "Visual Studio 17 2022" -A Win32 -DCMAKE_INSTALL_PREFIX=install/x86/Release
cmake --build build/x86/Release --config Release --parallel
ctest --test-dir build/x86/Release -C Release --output-on-failure
cmake --install build/x86/Release --config Release
```

`BUILD_TESTING` defaults to `ON`. Pass `-DBUILD_TESTING=OFF` to configure a
library-only build. Other generators must also select MSVC x86; single-config
generators require `-DCMAKE_BUILD_TYPE=Debug` or `Release`.

## Dependencies

The first configure fetches the MetaHook SDK at
`4d23b6fecd79dc949aabc2e145480cd1328d4a35`, without submodules or building the
launcher, and downloads VC-LTL 5.3.1 into `thirdparty/cache`. The VC-LTL archive
is verified with SHA-256
`7a18799ed3aa84a225610a5447a56bc534c5c98ccb8dec05caba0e3f633431ad`.

For a local or offline build, provide existing dependencies:

```bat
scripts\build-UtilThreadTask-x86-Release.bat -DMETAHOOK_SOURCE_PATH=D:\MetaHook -DVC_LTL_Root=D:\VC-LTL-5.3.1
```

The same names are accepted as environment variables when initializing the
CMake cache. Explicit CMake cache values take precedence. External directories
are read-only. `METAHOOK_SOURCE_PATH` must contain `include/HLSDK/common/interface.h`,
`interface.cpp`, and `LICENSE`; `VC_LTL_Root` must be an extracted VC-LTL binary
package. `UTILTHREADTASK_DEPENDENCY_CACHE_DIR` can override the download cache.

## Public interface and lifecycle

Load the DLL and resolve its `CreateInterface` export as `CreateInterfaceFn`.
Request `UTIL_THREAD_TASK_FACTORY_INTERFACE_VERSION` to obtain
`IUtilThreadTaskFactory`, then call `CreateThreadedTaskScheduler()`.
The public headers are in the repository's `include/Interface` and the MetaHook SDK's
`include/HLSDK/common` (headers are not copied into the install tree or the release
archive); add both directories to your include paths.

- `QueueTask(task)` appends a task; `QueueTask(task, true)` inserts at the front.
  Ownership transfers to the scheduler. Submit valid tasks whose `Destroy()`
  releases the object in the module that allocated it.
- `RunTask(time)` scans for the first task whose `ShouldRun(time)` is true,
  then calls `Run(time)` and `Destroy()`. It returns whether a task ran.
- `RunTasks(time, maxTasks)` runs at most `maxTasks` runnable tasks. Zero or
  negative values mean unlimited. Unready tasks remain queued; they do not
  block later runnable tasks. `Run` can enqueue more tasks.
- `IsCurrentThreadCreatorThread()` checks the scheduler's creation thread;
  execution methods do not enforce that thread.
- `WaitForAllTasksToComplete()` synchronously calls `Run(FLT_MAX)` and
  `Destroy()` on every queued task, without consulting `ShouldRun`. It is a
  shutdown drain, not a wait for background workers. `Destroy()` alone releases
  pending tasks without running them.

Stop producers and finish execution calls before shutdown or destruction.
Shutdown holds the queue lock while invoking callbacks: shutdown/destruction
callbacks must not modify or recursively drain the scheduler. Task callbacks
must not throw. Destroy all schedulers before unloading the DLL.

## Regression tests

Ten CTest scenarios dynamically load the actual DLL through the public interface.
They cover factory lookup, queue ordering and readiness, execution limits,
shutdown draining, exactly-once task release, thread identity, concurrent
producers, and enqueueing during `Run`. Checks remain active in Release and
each scenario has a 30-second timeout. Tests require no game installation or
additional test framework.

## License

MIT; see [LICENSE](LICENSE) and [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
