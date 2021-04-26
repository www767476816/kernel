#pragma once

#include "kernel_module.h"
#include "epoll_service.h"
#include "timer.h"
#include "database_service.h"

class KernelService
{
public:
    shared_ptr<NetWorkService> GetNetWorkInterface() { return make_shared<EpollService>(); }
    shared_ptr<TimerService> GetTimerInterface() { return make_shared<Timer>(); }
    shared_ptr<DataBaseService> GetDataBaseInterface() { return make_shared<DataBase>(); }
    MemoryCacheService *GetMemoryCacheInterface() { return nullptr; }
};