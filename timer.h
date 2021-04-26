#pragma once
#include "kernel_module.h"

using namespace std;

#define TIMER_GRAIN 100                   //小粒度
#define TIMER_FAR_GRAIN TIMER_GRAIN * 600 //大粒度

struct TimerBlock;
class Timer : public TimerService
{
public:
    //~Timer(){}
public:
    bool Init() override;
    bool Start() override;
    void Stop() override;
    //注册回调函数
    void RegisterCallBackFun(bool (*call_back)(int, int, unsigned int, void *)) override;

    //设置定时器
    unsigned int SetTimer(const int &owner, const int &timer_type, const int &repeat_count, const int &interval, void *bind_parameter = nullptr) override;
    //取消定时器
    bool CancelTimer(const unsigned int &timer_id) override;
    //取消定时器
    bool CancelTimerByOwner(const int &owner) override;
    //获取剩余时间(单位：毫秒)
    int RequestLeaveTime(const unsigned int &timer_id) override;
    //获取定时器信息，不存在返回NULL
    TimerBlock *RequestTimerItem(const unsigned int &timer_id);
    //清空定时器
    bool ClearTimer() override;
    //暂停定时器
    bool PauseTimer(const unsigned int &timer_id);
    //重启定时器
    bool RestartTimer(const unsigned int &timer_id);
    //提取数据
    //bool ExtractTimerData(list<tagTimerResult> &lTimerQueue);

private:
    //线程函数
    void ThreadFunction();

private:
    bool _run;
    unique_ptr<thread> _thread;
    int64_t _progress;

    mutex _mutex;
    map<int, shared_ptr<TimerBlock>> _far_timer;
    map<int, shared_ptr<TimerBlock>> _near_timer;
    
    //参数列表：UserID,定时器类型，定时器ID，用户输入参数
    bool (*_call_back)(int, int, unsigned int, void *);
};

struct TimerBlock
{
    int _owner;
    int _timer_type;
    int _repeat_count;     //重复次数
    int _interval;         //时间间隔
    void *_bind_parameter; //绑定参数

    int64_t _post_moment;  //插入时间
    unsigned int _timer_id;         //定时器编号
    int _surplus_interval; //剩余时间

    TimerBlock() = default;
    TimerBlock(const int &owner, const int &timer_type, const int &repeat_count, const int &interval, void *bind_parameter, const int64_t &post_moment, const int &timer_id, const int &surplus_interval) : _owner(owner),
                                                                                                                                                                                                            _timer_type(timer_type),
                                                                                                                                                                                                            _repeat_count(repeat_count),
                                                                                                                                                                                                            _interval(interval),
                                                                                                                                                                                                            _bind_parameter(bind_parameter),
                                                                                                                                                                                                            _post_moment(post_moment),
                                                                                                                                                                                                            _timer_id(timer_id),
                                                                                                                                                                                                            _surplus_interval(surplus_interval){};
    bool operator<(const TimerBlock &timer_block)
    {
        return timer_block._surplus_interval < _surplus_interval;
    }
};