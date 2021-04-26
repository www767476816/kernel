#pragma once

#include <iostream>
#include <list>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <mutex>
#include <thread>
#include <algorithm>

using namespace std;

#define MAX_LENGTH 65526 //65535-head-1

struct DataPacket
{
#pragma pack(1)
    DataPacket() = default;
    DataPacket(int size, int version, const char *data) : _size(size), _version(version)
    {
        fill(_data, _data + MAX_LENGTH, 0);
        copy(data, data + size, _data);
        _data[size] = '\0';
    }
    int _size;
    int _version;
    char _data[MAX_LENGTH + 1];
    //string _data;
};
class NetWorkService
{
public:
    virtual ~NetWorkService(){};

public:
    //ip 参数暂时不用，因为必然监听本地ip。
    virtual bool Init(const string &ip, const int &port, const int &max_connection) = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    //参数0表示正常关闭，-1表示异常关闭
    virtual void RegisterCallBack(void (*call_back)(int, int)) = 0;

public:
    virtual bool SendData(const int &handle, const string &data) = 0;
    virtual bool Connect(const string &ip, const int &port) = 0;
    //2021.4.5 xile
    //count参数表示的是一次取出多少条数据，不够指定的数量时全部取出，默认0时全部取出。
    virtual int ExtractData(list<string> &data_container, int count = 0) = 0;
    virtual bool CloseSocket(const int &handle) = 0;
};

class TimerService
{
public:
    virtual ~TimerService(){};

public:
    virtual bool Init() = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual void RegisterCallBackFun(bool (*call_back)(int, int, unsigned int, void *)) = 0;

public:
    virtual unsigned int SetTimer(const int &owner, const int &timer_type, const int &repeat_count, const int &interval, void *bind_parameter = nullptr) = 0;
    virtual bool CancelTimer(const unsigned int &timer_id) = 0;
    virtual bool CancelTimerByOwner(const int &owner) = 0;
    virtual int RequestLeaveTime(const unsigned int &timer_id) = 0;
    virtual bool ClearTimer() = 0;
};

class ResultSet;
class DataBaseService
{
public:
    virtual ~DataBaseService(){};

public:
    virtual bool Init() = 0;
    virtual bool Start(const string &ip, const int &port, const string &user, const string &password, const string &database) = 0;
    virtual bool Stop() = 0;

public:
    virtual int ExecSQL(string sql) = 0;
    virtual ResultSet *GetResultSet() const = 0;
    virtual bool Ping() = 0;
    virtual bool Reconnect() = 0;
    virtual bool CloseConnect() = 0;
    virtual unsigned int GetErrorCode() = 0;
    virtual string GetErrorDescribe() = 0;
};

class MemoryCacheService
{
public:
    virtual ~MemoryCacheService(){};

public:
    virtual bool Init() = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
};
