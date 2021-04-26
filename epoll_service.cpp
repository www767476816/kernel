#include "epoll_service.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>

bool EpollService::Init(const string &ip, const int &port, const int &max_connection)
{
    _epoll_handle = -1;
    _socket_listen_handle = -1;
    _run = false;
    _thread_array.clear();
    _port = port;
    _call_back = nullptr;

    _active_connection.clear();
    _connection_pool.clear();
    for (int i = 0; i < max_connection; ++i)
    {
        _connection_pool.push_back(make_shared<ConnectionItem>(i));
        _connection_pool.back().get()->Init();
    }
    _data_queue.clear();
    return true;
}

bool EpollService::Start()
{
    _epoll_handle = epoll_create(_connection_pool.size() + 1);
    if (_epoll_handle == -1)
    {
        ErrorTrace("epoll_create");
        return false;
    }
    _socket_listen_handle = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket_listen_handle == -1)
    {
        ErrorTrace("socket create");
        return false;
    }
    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = INADDR_ANY;
    addr_in.sin_port = htons(_port);

    if (bind(_socket_listen_handle, (struct sockaddr *)&addr_in, sizeof(addr_in)) == -1)
    {
        ErrorTrace("bind");
        return false;
    }
    if (listen(_socket_listen_handle, 20) == -1)
    {
        ErrorTrace("listen");
        return false;
    }

    //由于监听socket采用水平触发模式，所以这里单独处理一下
    struct epoll_event ev;
    ev.data.fd = _socket_listen_handle;
    ev.events = EPOLLIN;
    if (epoll_ctl(_epoll_handle, EPOLL_CTL_ADD, _socket_listen_handle, &ev) == -1) // 添加一个节点
    {
        ErrorTrace("epoll_ctl ADD");
        ShutDownSocket(_socket_listen_handle, -1);
        return false;
    }

    _run = true;
    for (int i = 0; i < sysconf(_SC_NPROCESSORS_CONF); ++i)
        _thread_array.push_back(make_unique<thread>(thread(&EpollService::ThreadFunction, this)));

    return true;
}
void EpollService::Stop()
{
    {
        lock_guard<mutex> lock(_mutex);
        vector<int> active_connection_id(_active_connection.size());
        for (auto item : active_connection_id)
            ShutDownSocket(item, 0);
        _active_connection.clear();
        _connection_pool.clear();
        _data_queue.clear();
    }
    _run = false;
    for (auto &i : _thread_array)
        i->join();
    _thread_array.clear();

    close(_socket_listen_handle);
    close(_epoll_handle);
    _port = 0;
    _call_back = nullptr;
}
void EpollService::RegisterCallBack(void (*call_back)(int, int))
{
    _call_back = call_back;
}
bool EpollService::SendData(const int &handle, const string &data)
{
    {
        lock_guard<mutex> lock(_mutex);

        auto connection = _active_connection.find(handle);
        if (connection == _active_connection.end() || connection->second.get()->IsConnect() == false)
        {
            ErrorTrace("SendData");
            ShutDownSocket(handle, -1);
            return false;
        }
        connection->second.get()->PushData(data);
    }
    return ModifyEpollNode(handle, SEND);
}
bool EpollService::Connect(const string &ip, const int &port)
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        ErrorTrace("socket create");
        return false;
    }
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    if (inet_aton(ip.c_str(), &serv_addr.sin_addr) == 0)
        return false;
    serv_addr.sin_port = htons(port);

    int connect_result = connect(server_socket, (sockaddr *)&serv_addr, sizeof(serv_addr));
    if (connect_result == -1)
    {
        ErrorTrace("Connect");
        ShutDownSocket(server_socket, -1);
        return false;
    }
    SetNonblocking(server_socket);
    //add epoll node and set epoll state to EPOLLIN
    if (!AddEpollNode(server_socket))
        return false;

    lock_guard<mutex> lock(_mutex);

    if (_connection_pool.empty())
    {
        ErrorTrace("connection pool empty!!!");
        ShutDownSocket(server_socket, 0);
        return false;
    }
    int _connection_id = _connection_pool.front().get()->GetConnectionID();
    _active_connection[_connection_id] = _connection_pool.front();
    _connection_pool.pop_front();

    _active_connection[_connection_id].get()->SetSocketHandle(server_socket);
    _active_connection[_connection_id].get()->SetAddrInfo(ip, port);
    _active_connection[_connection_id].get()->SetConnect(true);

    return true;
}
int EpollService::ExtractData(list<string> &data_container, int count)
{
    if (count == 0)
    {
        //全部取出
        lock_guard<mutex> lock(_mutex);

        count = _data_queue.size();
        for (auto item : _data_queue)
            data_container.emplace_back(item->_data, item->_size);
        _data_queue.clear();
    }
    else if (count > 0)
    {
        lock_guard<mutex> lock(_mutex);

        count = max(count, (int)_data_queue.size());
        auto iterator = _data_queue.begin();
        for (int i = 0; i < count; ++i)
            data_container.emplace_back(iterator->get()->_data, iterator->get()->_data + iterator->get()->_size);
        for (int i = 0; i < count; ++i)
            _data_queue.pop_front();
    }
    else
    {
        count = -1;
    }
    return count;
}
bool EpollService::CloseSocket(const int &handle)
{
    ShutDownSocket(handle, 0);
    return true;
}

void EpollService::ThreadFunction()
{
    while (_run)
    {
        struct epoll_event event_result[_connection_pool.size() + 1];

        int event_count = epoll_wait(_epoll_handle, (epoll_event *)&event_result, _connection_pool.size() + 1, 200);

        if (event_count == -1)
        {
            //error
            ErrorTrace("epoll_wait");
            Stop();
            exit(-1);
        }
        for (int i = 0; i < event_count; ++i)
        {
            if (event_result[i].events & EPOLLIN)
            {
                if (event_result[i].data.fd == _socket_listen_handle)
                {
                    //accept
                    AcceptEvent();
                }
                else
                {
                    //recv
                    RecvEvent(event_result[i].data.fd);
                }
            }
            else if (event_result[i].events & EPOLLOUT)
            {
                //send
                SendEvent(event_result[i].data.fd);
            }
            else
            {
                //errro
                ShutDownSocket(event_result[i].data.fd, -1);
                ErrorTrace("active fd");
            }
        }
        this_thread::sleep_for(chrono::milliseconds(1));
    }
}
bool EpollService::AddEpollNode(const int &handle)
{
    struct epoll_event ev;
    ev.data.fd = handle;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(_epoll_handle, EPOLL_CTL_ADD, handle, &ev) < 0) // 添加一个节点
    {
        ErrorTrace("epoll_ctl ADD");
        ShutDownSocket(handle, -1);
        return false;
    }
    return true;
}
bool EpollService::ModifyEpollNode(const int &handle, const int &flag)
{
    struct epoll_event ev;
    ev.data.fd = handle;
    ev.events = flag | EPOLLET;
    if (epoll_ctl(_epoll_handle, EPOLL_CTL_MOD, handle, &ev) < 0) // 修改一个节点
    {
        ErrorTrace("epoll_ctl MOD");
        ShutDownSocket(handle, -1);
        return false;
    }
    return true;
}

bool EpollService::DeleteEpollNode(const int &handle)
{
    if (epoll_ctl(_epoll_handle, EPOLL_CTL_DEL, handle, nullptr) < 0) // 删除一个节点
    {
        ErrorTrace("epoll_ctl DEL");
        ShutDownSocket(handle, -1);
        return false;
    }
    return true;
}

void EpollService::AcceptEvent()
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    int client_handle = accept(_socket_listen_handle, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_handle == -1)
    {
        ErrorTrace("accept");
        return;
    }

    std::cout << "accept a new client: " << ntohl(client_addr.sin_addr.s_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;

    SetNonblocking(client_handle);
    //add epoll node and set epoll state to EPOLLIN
    AddEpollNode(client_handle);

    lock_guard<mutex> lock(_mutex);

    if (_connection_pool.empty())
    {
        ErrorTrace("connection pool empty!!!");
        ShutDownSocket(client_handle, 0);
        return;
    }
    int _connection_id = _connection_pool.front().get()->GetConnectionID();
    _active_connection[_connection_id] = _connection_pool.front();
    _connection_pool.pop_front();

    _active_connection[_connection_id].get()->SetSocketHandle(client_handle);
    _active_connection[_connection_id].get()->SetAddrInfo(inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    _active_connection[_connection_id].get()->SetConnect(true);

    return;
}
void EpollService::RecvEvent(const int &handle)
{

    size_t remain_len = MAX_LENGTH + 8 + 1;
    ssize_t recv_len = 0;
    char data[MAX_LENGTH + 8 + 1];

    while (remain_len > 0)
    {
        recv_len = recv(handle, data + (MAX_LENGTH + 8 + 1 - remain_len), remain_len, MSG_WAITALL);
        if (recv_len == -1)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            else
            {
                ErrorTrace("recv");
                ShutDownSocket(handle, -1);
                return;
            }
        }
        else if (recv_len == 0)
        {
            if (remain_len == (MAX_LENGTH + 8 + 1))
            {
                //socket close
                ShutDownSocket(handle, 0);
                return;
            }
            break;
        }

        remain_len -= recv_len;
    }
    data[MAX_LENGTH] = '\0';
    lock_guard<mutex> lock(_mutex);
    _data_queue.push_back(make_shared<DataPacket>(BytesToInt(data), BytesToInt(data + 4), data + 8));
    return;
}
void EpollService::SendEvent(const int &handle)
{
    //从发送区域取出数据
    shared_ptr<DataPacket> data = nullptr;
    {
        lock_guard<mutex> lock(_mutex);
        auto connection = _active_connection.find(handle);
        if (connection == _active_connection.end() || connection->second.get()->IsConnect() == false)
        {
            ShutDownSocket(handle, -1);
            return;
        }
        data = connection->second.get()->PopData();
    }
    if (data == nullptr)
        return;
    ssize_t remain_len = MAX_LENGTH + 8 + 1;
    ssize_t send_len = 0;
    while (remain_len > 0)
    {
        send_len = send(handle, data.get() + (MAX_LENGTH + 8 + 1 - remain_len), remain_len, MSG_WAITALL);
        if (send_len == -1)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            else
            {
                ErrorTrace("send");
                ShutDownSocket(handle, -1);
                return;
            }
        }
        else if (send_len == 0)
            continue;

        remain_len -= send_len;
    }
    data = nullptr;
    //判断待发送区域是否还有数据，如果有就继续发送，如果没有切换回接收状态
    {
        lock_guard<mutex> lock(_mutex);
        auto connection = _active_connection.find(handle);
        if (connection == _active_connection.end() || connection->second.get()->IsConnect() == false)
            return;
        if (connection->second.get()->IsSendEnd())
            ModifyEpollNode(handle, RECV);
        else
            ModifyEpollNode(handle, SEND);
    }

    return;
}
// void EpollService::CloseEvent(const int &handle){}

inline void EpollService::SetNonblocking(const int &handle)
{
    fcntl(handle, F_SETFL, fcntl(handle, F_GETFD, 0) | O_NONBLOCK);
}
void EpollService::ShutDownSocket(const int &handle, const int &flag)
{
    {
        lock_guard<mutex> lock(_mutex);
        auto iterator = _active_connection.find(handle);
        //如果没找到，则默认连接已经被关闭了
        if (iterator != _active_connection.end())
        {
            iterator->second.get()->Reset();
            _connection_pool.push_back(iterator->second);
            _active_connection.erase(iterator);
        }
    }
    if (_call_back != nullptr)
        _call_back(handle, flag);
    shutdown(handle, SHUT_RDWR);

    while (1)
    {
        int close_flag = close(handle);
        if (close_flag == -1 && errno == EINTR)
            continue;
        break;
    }

    return;
}
int EpollService::BytesToInt(const char *bytes)
{
    int result = bytes[0] & 0xFF;
    result |= ((bytes[1] << 8) & 0xFF00);
    result |= ((bytes[2] << 16) & 0xFF0000);
    result |= ((bytes[3] << 24) & 0xFF000000);
    return result;
}
/////////////////////////////////////////////////////////////////////////////////////
void ConnectionItem::Init()
{
    _scoket_handle = -1;
    _ip.clear();
    _port = 0;
    _connect = false;
    _waiting_area.clear();
}
void ConnectionItem::Reset()
{
    _scoket_handle = -1;
    _ip.clear();
    _port = 0;
    _connect = false;
    _waiting_area.clear();
}

bool ConnectionItem::PushData(const string &data)
{
    _waiting_area.push_back(make_shared<DataPacket>((int)data.size(), 0, data.c_str()));
    return true;
}

shared_ptr<DataPacket> ConnectionItem::PopData()
{
    if (IsSendEnd())
        return nullptr;
    shared_ptr<DataPacket> result = _waiting_area.front();
    _waiting_area.pop_front();
    return result;
}