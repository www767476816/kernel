#pragma once
#include "kernel_module.h"
#include <errno.h>
#include <string.h>

using namespace std;

#define RECV EPOLLIN
#define SEND EPOLLOUT
class ConnectionItem;
class EpollService : public NetWorkService
{
public:
    bool Init(const string &ip, const int &port, const int &max_connection) override;
    bool Start() override;
    void Stop() override;
    void RegisterCallBack(void (*call_back)(int,int)) override;

public:
    bool SendData(const int &handle, const string &data) override;
    bool Connect(const string &ip, const int &port) override;
    int ExtractData(list<string> &data_container, int count = 0) override;
    bool CloseSocket(const int &handle) override;

private:
    void ThreadFunction();
    bool AddEpollNode(const int &handle);
    bool ModifyEpollNode(const int &handle, const int &flag);
    bool DeleteEpollNode(const int &handle);

private:
    void AcceptEvent();
    void RecvEvent(const int &handle);
    void SendEvent(const int &handle);
    //void CloseEvent(const int &handle);
private:
    void SetNonblocking(const int &handle);
    void ErrorTrace(string _head) { cout << _head << " error,code:" << errno << ",describe:" << strerror(errno) << endl; }
    void ShutDownSocket(const int &handle, const int &flag);
    int BytesToInt(const char *bytes);

private:
    int _epoll_handle;         //epoll句柄
    int _socket_listen_handle; //socket监听句柄

    bool _run;                    //运行标志位
    vector<unique_ptr<thread>> _thread_array; //工作线程数组
    mutex _mutex;                 //锁
    int _port;
    void (*_call_back)(int,int); //socket关闭的回调函数

    map<int, shared_ptr<ConnectionItem>> _active_connection;
    list<shared_ptr<ConnectionItem>> _connection_pool;

    list<shared_ptr<DataPacket>> _data_queue;
};

class ConnectionItem
{
public:
    ConnectionItem() = default;
    ConnectionItem(int connection_id) :_connection_id(connection_id),_scoket_handle(-1), _ip(""), _port(0), _connect(false) {}
    bool operator==(const ConnectionItem &right_connection)
    {
        return right_connection.GetConnectionID() == _connection_id;
    }

public:
    void Init();
    void Reset();

public:
    int GetConnectionID() const { return _connection_id; }
    bool IsConnect() const { return _connect; }

public:
    void SetAddrInfo(const string &ip, const int &port)
    {
        _ip = ip;
        _port = port;
    }
    void SetConnect(const bool &connect_flag) { _connect = connect_flag; }
    void SetSocketHandle(const int &socket_handle) { _scoket_handle = socket_handle; }
    bool PushData(const string &data);
    shared_ptr<DataPacket> PopData();
    bool IsSendEnd() const { return _waiting_area.empty(); }

private:
    int _connection_id;
    int _scoket_handle;
    string _ip;
    int _port;
    bool _connect;
    list<shared_ptr<DataPacket>> _waiting_area;
};
