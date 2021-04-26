#include "timer.h"

//初始化
bool Timer::Init()
{
    _run = false;
    _thread = nullptr;
    _progress = 0;

    _far_timer.clear();
    _near_timer.clear();

    _call_back = nullptr;

    return true;
}
//启动
bool Timer::Start()
{
    _run = true;
    _thread = make_unique<thread>(thread(&Timer::ThreadFunction, this));
    return true;
}
//停止
void Timer::Stop()
{
    //清空定时器
    ClearTimer();

    _run = false;
    _thread->join();
    _thread = nullptr;

    _call_back = nullptr;

    return;
}
//注册回调函数
void Timer::RegisterCallBackFun(bool (*call_back)(int, int,unsigned int, void *))
{
    _call_back = call_back;
}
#include <iostream>
//设置定时器
unsigned int Timer::SetTimer(const int &owner, const int &timer_type, const int &repeat_count, const int &interval, void *bind_parameter)
{
    if (repeat_count == 0 || repeat_count < -1)
        return 0;
    //生成ID
    unsigned int timer_id = 1;

    lock_guard<mutex> lock(_mutex);
    if (!_near_timer.empty())
    {
        timer_id = _near_timer.rbegin()->second->_timer_id + 1;
    }
    if (!_far_timer.empty() && (_far_timer.rbegin()->second->_timer_id > (timer_id - 1)))
    {
        timer_id = _far_timer.rbegin()->second->_timer_id + 1;
    }
    if (interval >= TIMER_FAR_GRAIN)
    {
        _far_timer.insert(pair<int, shared_ptr<TimerBlock>>(timer_id, make_shared<TimerBlock>(owner,
                                                                                              timer_type,
                                                                                              repeat_count,
                                                                                              interval,
                                                                                              bind_parameter,
                                                                                              chrono::duration_cast<std::chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count(),
                                                                                              timer_id, interval)));
    }
    else
    {
        _near_timer.insert(pair<int, shared_ptr<TimerBlock>>(timer_id, make_shared<TimerBlock>(owner,
                                                                                               timer_type,
                                                                                               repeat_count,
                                                                                               interval,
                                                                                               bind_parameter,
                                                                                               chrono::duration_cast<std::chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count(),
                                                                                               timer_id, interval)));
    }

    return timer_id;
}
//取消定时器
bool Timer::CancelTimer(const unsigned int &timer_id)
{
    if (timer_id <= 0)
        return false;

    lock_guard<mutex> lock(_mutex);

    auto far_result = _far_timer.find(timer_id);
    if (far_result != _far_timer.end())
    {
        _far_timer.erase(far_result);
        return true;
    }

    auto near_result = _near_timer.find(timer_id);
    if (near_result != _near_timer.end())
    {
        _near_timer.erase(near_result);
    }

    return true;
}
bool Timer::CancelTimerByOwner(const int &owner)
{
    vector<unsigned int> timer_id_result;
    {
        lock_guard<mutex> lock(_mutex);
        for (auto &item : _far_timer)
        {
            if (item.second->_owner == owner)
                timer_id_result.push_back(item.second->_timer_id);
        }
        for (auto &item : _near_timer)
        {
            if (item.second->_owner == owner)
                timer_id_result.push_back(item.second->_timer_id);
        }
    }
    for (auto id : timer_id_result)
        CancelTimer(id);

    return true;
}
//获取剩余时间
int Timer::RequestLeaveTime(const unsigned int &timer_id)
{
    if (timer_id <= 0)
        return -1;
    lock_guard<mutex> lock(_mutex);

    auto far_result = _far_timer.find(timer_id);
    if (far_result != _far_timer.end())
        return far_result->second->_surplus_interval;

    auto near_result = _near_timer.find(timer_id);
    if (near_result != _near_timer.end())
        return near_result->second->_surplus_interval;

    return -1;
}
//获取定时器信息
TimerBlock *Timer::RequestTimerItem(const unsigned int &timer_id)
{
    return nullptr;
}
//清空定时器
bool Timer::ClearTimer()
{
    lock_guard<mutex> lock(_mutex);

    _far_timer.clear();
    _near_timer.clear();
    _progress = 0;

    return true;
}
//线程函数
void Timer::ThreadFunction()
{
    while (_run)
    {
        int64_t now_time = chrono::duration_cast<std::chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        vector<shared_ptr<TimerBlock>> end_list;
        {
            lock_guard<mutex> lock(_mutex);

            set<int> move_list;
            if (_progress >= TIMER_FAR_GRAIN)
            {
                _progress = 0;
                //检查是否有近期要发生的定时器
                if (!_far_timer.empty())
                {
                    //更新时间
                    for (auto &item : _far_timer)
                    {
                        item.second->_surplus_interval -= (now_time - item.second->_post_moment);
                        item.second->_post_moment = now_time;
                        if (item.second->_surplus_interval < TIMER_FAR_GRAIN)
                            move_list.insert(item.second->_timer_id);
                    }

                    if (move_list.size() > 0)
                    {
                        for (auto id : move_list)
                        {
                            _near_timer[_far_timer.find(id)->first]=_far_timer.find(id)->second;
                            _far_timer.erase(_far_timer.find(id));
                        }
                    }
                }
            }
            //以下是近期要发生的定时器的逻辑
            for (auto index = _near_timer.begin(); index != _near_timer.end();)
            {
                if (!move_list.empty() && move_list.find(index->second->_timer_id) != move_list.end())
                {
                    ++index;
                    continue;
                }

                if (index->second->_surplus_interval <= TIMER_GRAIN)
                {
                    //结束一次
                    end_list.push_back(index->second);
                    index->second->_surplus_interval = index->second->_interval;
                    index->second->_post_moment = now_time;
                    index->second->_repeat_count -= (index->second->_repeat_count == -1 ? 0 : 1);
                    if (index->second->_surplus_interval > TIMER_FAR_GRAIN)
                    {
                         _far_timer[index->first]=index->second;
                        index = _near_timer.erase(index);
                    }
                    else
                    {
                        if (index->second->_repeat_count == 0)
                            index = _near_timer.erase(index);
                        else
                            ++index;
                    }
                }
                else
                {
                    //未结束
                    index->second->_surplus_interval -= (now_time - index->second->_post_moment);
                    index->second->_post_moment = now_time;
                    ++index;
                }
            }
        }
        _progress += TIMER_GRAIN;

        //定时器一次结束，执行回调
        for (auto &item : end_list)
        {
            _call_back(item->_owner, item->_timer_type, item->_timer_id, item->_bind_parameter);
        }
        end_list.clear();
        this_thread::sleep_for(chrono::milliseconds(TIMER_GRAIN));
    }
    return;
}