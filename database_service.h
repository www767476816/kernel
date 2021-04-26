#pragma once
#include "kernel_module.h"
#include "/usr/include/mysql/mysql.h"

using namespace std;

class ResultSet;

class DataBase : public DataBaseService
{
public:
	//初始化
	bool Init() override;
	//开始
	bool Start(const string &ip, const int &port, const string &user, const string &password, const string &database) override;
	//停止
	bool Stop() override;
	//执行语句
	int ExecSQL(string sql) override;
	//获取结果集
	ResultSet *GetResultSet()const override;
	//ping
	bool Ping() override;
	//重新连接
	bool Reconnect() override;
	//关闭连接
	bool CloseConnect() override;
	//获取错误码
	unsigned int GetErrorCode() override;
	//获取错误信息
	string GetErrorDescribe() override;

private:
	MYSQL *_sql_handle;				   //sql句柄
	unique_ptr<ResultSet> _result_set; //结果集

	string _ip;		  //地址
	int _port;		  //端口
	string _user;	  //用户
	string _password; //密码
	string _database; //数据库
};
class ResultSet
{
public:
	void SetResultSet(MYSQL_RES *result_set);

public:
	//初始化
	bool Init();
	//停止
	bool Stop();
	//提取数据
	bool ExtractData(const int &field_index, int &data, int defaults = 0);
	bool ExtractData(const int &field_index, double &data, double defaults = 0);
	bool ExtractData(const int &field_index, long long &data, long long defaults = 0);
	bool ExtractData(const int &field_index, string &data, string defaults = "");
	//获取列数
	int GetFieldCount();
	//下一行
	bool MoveToNext();
	//结束
	bool IsEnd();

private:
	MYSQL_RES *_mysql_res; //结果集

	MYSQL_FIELD *_field_pos; //列数据
	MYSQL_ROW _row_pos;		 //行数据

	int _field_num; //总列数
	int _row_num;	//总行数
};
