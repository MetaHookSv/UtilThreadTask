#include <IUtilThreadTask.h>
#include "ThreadedTask.h"

class CUtilThreadTaskFactory : public IUtilThreadTaskFactory
{
public:
    IThreadedTaskScheduler* CreateThreadedTaskScheduler() override
    {
        return ThreadedTaskScheduler_CreateInstance();
    }
};

EXPOSE_SINGLE_INTERFACE(CUtilThreadTaskFactory, IUtilThreadTaskFactory, UTIL_THREAD_TASK_FACTORY_INTERFACE_VERSION);
