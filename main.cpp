#include "kernel_engine.h"

#include <string>
using namespace std;

void TestDataBase()
{
    KernelService kernel;
    auto database = kernel.GetDataBaseInterface();

    database->Init();
    database->Start("127.0.0.1", 3306, "root", "123456789", "db_test");
    int result = database->ExecSQL("select * from tb_test");
    cout << "result:" << result << endl;

    auto result_set = database->GetResultSet();
    while (!result_set->IsEnd())
    {
        int id, year;
        string name;
        double money;
        result_set->ExtractData(0, id);
        result_set->ExtractData(1, name);
        result_set->ExtractData(2, year);
        result_set->ExtractData(3, money);

        cout << "id:" << id << ",name:" << name << ",year:" << year << ",money:" << money << endl;

        result_set->MoveToNext();
    }

    database->Stop();
}
///////////////////////////////////////////////////////////////////////////////////////
KernelService kernel;
auto timer = kernel.GetTimerInterface();
bool TimerCallback(int, int, unsigned int, void *)
{
    cout << "定时器结束" << endl;
    timer->Stop();
    return true;
}
void TestTimer()
{
    timer->Init();
    timer->RegisterCallBackFun(TimerCallback);
    timer->Start();
    timer->SetTimer(1, 1, 1, 3000);
}
///////////////////////////////////////////////////////////////////////////////////////
void SocketCallback(int handle,int flag)
{
    //socket 关闭，flag 0表示正常关闭，-1表示异常关闭
}
void TestSocket()
{
    KernelService kernel_service;
    auto net_work = kernel_service.GetNetWorkInterface();
    net_work->Init("127.0.0.1", 4396, 1000);
    net_work->RegisterCallBack(SocketCallback);
    net_work->Start();

    list<string> data_list;
    while (1)
    {
        int result = net_work->ExtractData(data_list);
        if (result > 0)
            cout << "接收到" << result << "条消息！" << endl;
        for (auto item : data_list)
        {
            cout << "size:" << item.size() << ",data:" << item << endl;
        }
        data_list.clear();
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    net_work->Stop();
}
int main(int, char **)
{
    return 0;
}
