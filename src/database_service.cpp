#include "database_service.h"

bool DataBase::Init()
{
	_sql_handle = nullptr;
	_result_set=make_unique<ResultSet>(ResultSet());
	_result_set->Init();

	_ip.clear();
	_port = 0;
	_user.clear();
	_password.clear();
	_database.clear();
	return true;
}

bool DataBase::Start(const string &ip, const int &port, const string &user, const string &password, const string &database)
{
	_sql_handle = mysql_init(NULL);
	if (_sql_handle == nullptr)
		return false;

	auto lambda_func = [&] {
		mysql_close(_sql_handle);
		_sql_handle = nullptr;
	};
	//设置参数
	if (mysql_options(_sql_handle, MYSQL_SET_CHARSET_NAME, "utf8"))
	{
		lambda_func();
		return false;
	}
	//连接
	if (!mysql_real_connect(_sql_handle, ip.c_str(), user.c_str(), password.c_str(), database.c_str(), port, 0, 0))
	{
		lambda_func();
		return false;
	}
	_ip = ip;
	_port = port;
	_user = user;
	_password = password;
	_database = database;
	return true;
}

bool DataBase::Stop()
{
	_result_set->Stop();

	_ip.clear();
	_port = 0;
	_user.clear();
	_password.clear();
	_database.clear();

	if (_sql_handle)
	{
		mysql_close(_sql_handle);
		_sql_handle = nullptr;
	}
	mysql_library_end();
	return true;
}

int DataBase::ExecSQL(string sql)
{
	int query_result = mysql_real_query(_sql_handle, sql.c_str(), sql.size());

	if (query_result != 0)
	{
		//执行错误
		if (GetErrorCode() != CR_SERVER_GONE_ERROR && GetErrorCode() != CR_SERVER_LOST)
			return -1;

		//重新连接
		Reconnect();
		if (mysql_real_query(_sql_handle, sql.c_str(), sql.size()) != 0)
			return -1;
	}
	//执行成功，获取结果集
	MYSQL_RES *store_result = mysql_store_result(_sql_handle);
	if (store_result == 0 && mysql_field_count(_sql_handle) != 0)
		return -1;

	_result_set->SetResultSet(store_result);

	return (int)mysql_affected_rows(_sql_handle);
}

ResultSet *DataBase::GetResultSet()const
{
	return _result_set.get();
	
}
bool DataBase::Ping()
{
	if (mysql_ping(_sql_handle) == 0)
		return true;
	return false;
}

bool DataBase::Reconnect()
{
	if (_sql_handle != nullptr && Ping())
		return true;

	CloseConnect();

	return Start(_ip, _port, _user, _password, _database);
}

bool DataBase::CloseConnect()
{
	if (_sql_handle)
	{
		_result_set->Stop();
		mysql_close(_sql_handle);
		_sql_handle = nullptr;

		_ip.clear();
		_port = 0;
		_user.clear();
		_password.clear();
		_database.clear();
	}
	return true;
}

unsigned int DataBase::GetErrorCode()
{
	return mysql_errno(_sql_handle);
}

string DataBase::GetErrorDescribe()
{
	return mysql_error(_sql_handle);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ResultSet::SetResultSet(MYSQL_RES *result_set)
{
	if (_mysql_res != NULL)
	{
		mysql_free_result(_mysql_res);
		_mysql_res = nullptr;
	}

	_mysql_res = result_set;
	_field_num = mysql_num_fields(_mysql_res);
	_row_num = mysql_num_rows(_mysql_res);
	_row_pos = mysql_fetch_row(_mysql_res);
	_field_pos = mysql_fetch_fields(_mysql_res);
}

bool ResultSet::Init()
{
	_mysql_res = nullptr;
	_field_pos = nullptr;
	_row_pos = nullptr;
	_field_num = 0;
	_row_num = 0;
	return true;
}

bool ResultSet::Stop()
{
	_mysql_res = nullptr;
	_field_pos = nullptr;
	_row_pos = nullptr;
	_field_num = 0;
	_row_num = 0;
	return true;
}

bool ResultSet::ExtractData(const int &field_index, int &data, int defaults)
{
	if (field_index < 0 || field_index > _field_num)
		return false;

	if (_row_pos[field_index] == NULL)
		data = defaults;
	else
		data = atoi(_row_pos[field_index]);
	return true;
}

bool ResultSet::ExtractData(const int &field_index, double &data, double defaults)
{
	if (field_index < 0 || field_index > _field_num)
		return false;

	if (_row_pos[field_index] == NULL)
		data = defaults;
	else
		data = atof(_row_pos[field_index]);
	return true;
}

bool ResultSet::ExtractData(const int &field_index, long long &data, long long defaults)
{
	if (field_index < 0 || field_index > _field_num)
		return false;

	if (_row_pos[field_index] == NULL)
		data = defaults;
	else
		data = atoll(_row_pos[field_index]);
	return true;
}

bool ResultSet::ExtractData(const int &field_index, string &data, string defaults)
{
	if (field_index < 0 || field_index > _field_num)
		return false;

	if (_row_pos[field_index] == NULL)
		data = defaults;
	else
		data = _row_pos[field_index];

	return true;
}

int ResultSet::GetFieldCount()
{
	return _field_num;
}

bool ResultSet::MoveToNext()
{
	if (_mysql_res == 0)
		return false;

	_row_pos = mysql_fetch_row(_mysql_res);

	if (_row_pos == 0)
		mysql_free_result(_mysql_res);

	return _row_pos != 0;
}

bool ResultSet::IsEnd()
{
	return _row_pos == nullptr;
}
