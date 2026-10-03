#include <IUtilThreadTask.h>
#include <algorithm>
#include <cfloat>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template<class T>
void Expect(const T& expected, const T& actual, const char* message)
{
    Check(expected == actual, message);
}

class LoadedModule
{
public:
    explicit LoadedModule(const char* path) : m_module(Sys_LoadModule(path))
    {
        Check(m_module != nullptr, "DLL load failed");
    }

    ~LoadedModule() { Sys_FreeModule(m_module); }
    LoadedModule(const LoadedModule&) = delete;
    LoadedModule& operator=(const LoadedModule&) = delete;

    CreateInterfaceFn Factory() const
    {
        auto factory = Sys_GetFactory(m_module);
        Check(factory != nullptr, "CreateInterface export missing");
        return factory;
    }

private:
    HINTERFACEMODULE m_module;
};

IUtilThreadTaskFactory* GetTaskFactory(CreateInterfaceFn factory)
{
    int result = IFACE_FAILED;
    auto taskFactory = static_cast<IUtilThreadTaskFactory*>(
        factory(UTIL_THREAD_TASK_FACTORY_INTERFACE_VERSION, &result));
    Expect(int(IFACE_OK), result, "Factory lookup status");
    Check(taskFactory != nullptr, "Task factory missing");
    return taskFactory;
}

struct SchedulerDeleter
{
    void operator()(IThreadedTaskScheduler* scheduler) const { scheduler->Destroy(); }
};
using Scheduler = std::unique_ptr<IThreadedTaskScheduler, SchedulerDeleter>;

Scheduler CreateScheduler(CreateInterfaceFn factory)
{
    Scheduler scheduler(GetTaskFactory(factory)->CreateThreadedTaskScheduler());
    Check(scheduler != nullptr, "Scheduler creation failed");
    return scheduler;
}

struct TaskRecord
{
    int runs = 0;
    int destroys = 0;
    std::vector<float> shouldRunTimes;
    std::vector<float> runTimes;
    std::vector<std::thread::id> runThreads;
};

class RecordingTask : public IThreadedTask
{
public:
    RecordingTask(TaskRecord& record, int id = 0, std::vector<int>* order = nullptr)
        : m_record(record), m_id(id), m_order(order) {}

    void Destroy() override
    {
        ++m_record.destroys;
        delete this;
    }

    bool ShouldRun(float time) override
    {
        m_record.shouldRunTimes.push_back(time);
        return ready && time >= readyAt;
    }

    void Run(float time) override
    {
        ++m_record.runs;
        m_record.runTimes.push_back(time);
        m_record.runThreads.push_back(std::this_thread::get_id());
        if (m_order)
            m_order->push_back(m_id);
        if (onRun)
            onRun();
    }

    bool ready = true;
    float readyAt = -FLT_MAX;
    std::function<void()> onRun;

private:
    TaskRecord& m_record;
    int m_id;
    std::vector<int>* m_order;
};

void ExpectCompleted(const TaskRecord& record)
{
    Expect(1, record.runs, "Task must run exactly once");
    Expect(1, record.destroys, "Task must be destroyed exactly once");
}

void TestFactory(CreateInterfaceFn factory)
{
    auto first = GetTaskFactory(factory);
    Expect(first, GetTaskFactory(factory), "Factory must be a singleton");
    Expect(static_cast<IBaseInterface*>(first),
        factory(UTIL_THREAD_TASK_FACTORY_INTERFACE_VERSION, nullptr), "Null status pointer lookup");
    int result = IFACE_OK;
    Check(factory("UtilThreadTaskFactory_unknown", &result) == nullptr, "Unknown version must fail");
    Expect(int(IFACE_FAILED), result, "Unknown version lookup status");
    Check(factory("UtilThreadTaskFactory_unknown", nullptr) == nullptr, "Unknown version without status");

    TaskRecord record;
    Scheduler a(first->CreateThreadedTaskScheduler());
    Scheduler b(first->CreateThreadedTaskScheduler());
    Check(a && b && a.get() != b.get(), "Schedulers must be independent instances");
    a->QueueTask(new RecordingTask(record));
    Check(!b->RunTask(0.0f), "Second scheduler must have its own queue");
    Check(a->RunTask(0.0f), "First scheduler must retain its task");
    ExpectCompleted(record);
}

void TestEmpty(CreateInterfaceFn factory)
{
    auto scheduler = CreateScheduler(factory);
    Check(!scheduler->RunTask(0.0f), "Empty RunTask must return false");
    scheduler->RunTasks(0.0f, 1);
    scheduler->RunTasks(0.0f, 0);
    scheduler->RunTasks(0.0f, -1);
    scheduler->WaitForAllTasksToComplete();
    scheduler->WaitForAllTasksToComplete();
    Check(!scheduler->RunTask(FLT_MAX), "Empty drain must remain empty");
}

void TestOrdering(CreateInterfaceFn factory)
{
    std::vector<TaskRecord> records(4);
    std::vector<int> order;
    auto scheduler = CreateScheduler(factory);
    scheduler->QueueTask(new RecordingTask(records[0], 0, &order));
    scheduler->QueueTask(new RecordingTask(records[1], 1, &order));
    scheduler->QueueTask(new RecordingTask(records[2], 2, &order), true);
    scheduler->QueueTask(new RecordingTask(records[3], 3, &order), true);
    scheduler->RunTasks(7.5f, 0);
    Expect(std::vector<int>{3, 2, 0, 1}, order, "Front insertion and FIFO order");
    for (const auto& record : records)
    {
        ExpectCompleted(record);
        Expect(std::vector<float>{7.5f}, record.shouldRunTimes, "ShouldRun time forwarding");
        Expect(std::vector<float>{7.5f}, record.runTimes, "Run time forwarding");
    }
    Check(!scheduler->RunTask(7.5f), "Queue must be empty after execution");
}

void TestReadiness(CreateInterfaceFn factory)
{
    TaskRecord delayed, immediate;
    std::vector<int> order;
    auto scheduler = CreateScheduler(factory);
    auto task = new RecordingTask(delayed, 1, &order);
    task->readyAt = 10.0f;
    scheduler->QueueTask(task);
    scheduler->QueueTask(new RecordingTask(immediate, 2, &order));
    Check(scheduler->RunTask(2.5f), "Runnable task behind delayed task must execute");
    Expect(std::vector<int>{2}, order, "Delayed task must be skipped");
    Expect(0, delayed.runs, "Delayed task must stay queued");
    Expect(0, delayed.destroys, "Delayed task must stay alive");
    Check(!scheduler->RunTask(9.0f), "Queue with only unready tasks must return false");
    Check(scheduler->RunTask(10.0f), "Delayed task must become runnable at its threshold");
    Expect(std::vector<int>{2, 1}, order, "Delayed task execution order");
    Expect(std::vector<float>{2.5f, 9.0f, 10.0f}, delayed.shouldRunTimes, "Readiness time forwarding");
    Expect(std::vector<float>{10.0f}, delayed.runTimes, "Delayed Run time forwarding");
    ExpectCompleted(delayed);
    ExpectCompleted(immediate);
}

void TestLimits(CreateInterfaceFn factory)
{
    // Both a one-task limit and a larger limit catch the historical off-by-one bug.
    for (int limit : {1, 3})
    {
        std::vector<TaskRecord> records(5);
        std::vector<int> order;
        auto scheduler = CreateScheduler(factory);
        for (int id = 0; id < int(records.size()); ++id)
            scheduler->QueueTask(new RecordingTask(records[id], id, &order));
        scheduler->RunTasks(0.0f, limit);
        Expect(limit, int(order.size()), "Positive limit must be exact");
        for (int id = 0; id < int(records.size()); ++id)
        {
            Expect(id < limit ? 1 : 0, records[id].runs, "Limited task run count");
            Expect(id < limit ? 1 : 0, records[id].destroys, "Limited task destruction count");
        }
        scheduler->RunTasks(0.0f, 100);
        Expect(std::vector<int>{0, 1, 2, 3, 4}, order, "Remainder must stay queued in order");
        for (const auto& record : records)
            ExpectCompleted(record);
    }
    for (int limit : {0, -1, -7})
    {
        std::vector<TaskRecord> records(5);
        auto scheduler = CreateScheduler(factory);
        for (auto& record : records)
            scheduler->QueueTask(new RecordingTask(record));
        scheduler->RunTasks(0.0f, limit);
        for (const auto& record : records)
            ExpectCompleted(record);
        Check(!scheduler->RunTask(0.0f), "Nonpositive limit must drain runnable tasks");
    }
}

void TestDrain(CreateInterfaceFn factory)
{
    std::vector<TaskRecord> records(3);
    std::vector<int> order;
    auto scheduler = CreateScheduler(factory);
    for (int id = 0; id < int(records.size()); ++id)
    {
        auto task = new RecordingTask(records[id], id, &order);
        task->ready = false;
        scheduler->QueueTask(task, id == 2);
    }
    scheduler->WaitForAllTasksToComplete();
    Expect(std::vector<int>{2, 0, 1}, order, "Shutdown must execute queued tasks in order");
    for (const auto& record : records)
    {
        ExpectCompleted(record);
        Check(record.shouldRunTimes.empty(), "Shutdown must bypass ShouldRun");
        Expect(std::vector<float>{FLT_MAX}, record.runTimes, "Shutdown must pass FLT_MAX");
    }
    Check(!scheduler->RunTask(0.0f), "Shutdown must clear its queue");
    scheduler->WaitForAllTasksToComplete();
    scheduler.reset();
    for (const auto& record : records)
        ExpectCompleted(record);
}

void TestDestroy(CreateInterfaceFn factory)
{
    std::vector<TaskRecord> records(3);
    auto scheduler = CreateScheduler(factory);
    for (auto& record : records)
        scheduler->QueueTask(new RecordingTask(record));
    Check(scheduler->RunTask(0.0f), "First task must execute");
    scheduler.reset();
    ExpectCompleted(records[0]);
    for (std::size_t i = 1; i < records.size(); ++i)
    {
        Expect(0, records[i].runs, "Destroy must not execute pending tasks");
        Expect(1, records[i].destroys, "Destroy must release pending tasks exactly once");
        Check(records[i].shouldRunTimes.empty(), "Destroy must not query readiness");
    }
}

void TestCreatorThread(CreateInterfaceFn factory)
{
    TaskRecord record;
    auto scheduler = CreateScheduler(factory);
    Check(scheduler->IsCurrentThreadCreatorThread(), "Creating thread must be recognized");
    scheduler->QueueTask(new RecordingTask(record));
    bool workerIsCreator = true;
    bool workerRanTask = false;
    std::thread worker([&] {
        workerIsCreator = scheduler->IsCurrentThreadCreatorThread();
        workerRanTask = scheduler->RunTask(3.0f);
    });
    const auto workerId = worker.get_id();
    worker.join();
    Check(!workerIsCreator, "Other thread must not be recognized as creator");
    Check(workerRanTask, "RunTask must work on the calling thread");
    Expect(std::vector<std::thread::id>{workerId}, record.runThreads, "Task must execute on the calling thread");
    Check(scheduler->IsCurrentThreadCreatorThread(), "Creator identity must remain unchanged");
    ExpectCompleted(record);
}

void TestConcurrentQueue(CreateInterfaceFn factory)
{
    constexpr int producerCount = 4;
    constexpr int tasksPerProducer = 128;
    constexpr int taskCount = producerCount * tasksPerProducer;
    std::vector<TaskRecord> records(taskCount);
    std::vector<int> order;
    auto scheduler = CreateScheduler(factory);
    std::promise<void> start;
    auto signal = start.get_future().share();
    std::vector<std::thread> producers;
    for (int producer = 0; producer < producerCount; ++producer)
    {
        producers.emplace_back([&, producer] {
            signal.wait();
            for (int i = 0; i < tasksPerProducer; ++i)
            {
                const int id = producer * tasksPerProducer + i;
                scheduler->QueueTask(new RecordingTask(records[id], id, &order));
            }
        });
    }
    start.set_value();
    for (auto& producer : producers)
        producer.join();
    scheduler->RunTasks(11.0f, 0);
    Expect(taskCount, int(order.size()), "Concurrent queue must retain every task");
    // Each producer's FIFO order must survive interleaving with the other producers.
    std::vector<int> lastId(producerCount, -1);
    for (int id : order)
    {
        const int producer = id / tasksPerProducer;
        Check(id > lastId[producer], "Per-producer FIFO order");
        lastId[producer] = id;
    }
    std::sort(order.begin(), order.end());
    for (int id = 0; id < taskCount; ++id)
    {
        Expect(id, order[id], "Concurrent queue must have no missing or duplicate IDs");
        ExpectCompleted(records[id]);
        Expect(std::vector<std::thread::id>{std::this_thread::get_id()}, records[id].runThreads,
            "Queued task must execute on the consumer thread");
    }
    Check(!scheduler->RunTask(11.0f), "Concurrent queue must be empty after execution");
}

void TestReentrantQueue(CreateInterfaceFn factory)
{
    TaskRecord first, second, recursive, queued;
    std::vector<int> order;
    auto scheduler = CreateScheduler(factory);
    auto task = new RecordingTask(first, 1, &order);
    // A producer must be able to acquire the queue lock while Run is active.
    // If Run held that lock, the join would hang and CTest's timeout would fail.
    task->onRun = [&] {
        std::thread producer([&] {
            scheduler->QueueTask(new RecordingTask(second, 2, &order));
        });
        producer.join();
    };
    scheduler->QueueTask(task);
    scheduler->RunTasks(4.0f, 0);
    Expect(std::vector<int>{1, 2}, order, "Task queued during Run must be executable in the same batch");
    ExpectCompleted(first);
    ExpectCompleted(second);

    auto recursiveTask = new RecordingTask(recursive);
    recursiveTask->onRun = [&] { scheduler->QueueTask(new RecordingTask(queued)); };
    scheduler->QueueTask(recursiveTask);
    scheduler->RunTasks(4.0f, 0);
    ExpectCompleted(recursive);
    ExpectCompleted(queued);
}
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: UtilThreadTaskTests <DLL path> <scenario>\n";
        return 2;
    }
    try
    {
        const std::pair<const char*, void (*)(CreateInterfaceFn)> tests[] = {
            {"Factory", TestFactory}, {"Empty", TestEmpty}, {"Ordering", TestOrdering},
            {"Readiness", TestReadiness}, {"Limits", TestLimits}, {"Drain", TestDrain},
            {"Destroy", TestDestroy}, {"CreatorThread", TestCreatorThread},
            {"ConcurrentQueue", TestConcurrentQueue}, {"ReentrantQueue", TestReentrantQueue}};
        LoadedModule module(argv[1]);
        for (const auto& [name, test] : tests)
        {
            if (std::string(argv[2]) == name)
            {
                test(module.Factory());
                std::cout << name << " passed\n";
                return 0;
            }
        }
        throw std::runtime_error("Unknown test scenario");
    }
    catch (const std::exception& error)
    {
        std::cerr << argv[2] << ": " << error.what() << '\n';
        return 1;
    }
}
