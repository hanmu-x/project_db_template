

#include "string/silly_algorithm.h"
#include <filesystem>
#include <regex>
#include "datetime/simple_time.h"

#include <vector>
#include <fstream>

#include "database/otl/otl_tools.h"
#include "json/silly_jsonpp.h"
#include "singleton/silly_singleton.h"

#include "config.h"

struct DataEntry
{
	std::string code; // CODE
	std::string atm; // RuleValidTime
	int intv;		// STDT
	int grade;		// warnGradeID 
	float swc;  // 不录
	float drrp; // 阈值Threshold
	int wtypeId;

};




// 时间格式转换 
std::string convertTimeFormat(const std::string& inputTime)
{
	std::string outputTime;

	// 定义输入和输出的时间格式
	const char* inputFormat = "%Y%m%d%H%M";
	const char* outputFormat = "%Y-%m-%d %H:%M";

	std::tm timeStruct = {};
	std::istringstream ss(inputTime);

	// 将输入时间字符串解析为 tm 结构
	ss >> std::get_time(&timeStruct, inputFormat);

	if (!ss.fail())
	{
		// 将 tm 结构转换为输出时间格式的字符串
		std::ostringstream oss;
		oss << std::put_time(&timeStruct, outputFormat);
		outputTime = oss.str();
	}
	else
	{
		// 解析失败，输出错误信息或处理异常
		std::cout << "Failed to parse input time: " << inputTime << std::endl;
	}

	return outputTime;
}


// 使用提供的字符串分割函数来读取数据
std::vector<DataEntry> readSplitData(const std::string& filename)
{
	std::vector<DataEntry> data;

	// 打开文件
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cout << "Error opening file: " << filename << std::endl;
		return data; // 返回空向量以示错误
	}

	std::string line;
	std::getline(file, line);
	while (std::getline(file, line))
	{

		// 使用第一个分割函数来分割行
		std::vector<std::string> tokens = silly_string_algo::split(line, ',');
		// 如果分割后的 tokens 数量不为 6，跳过此行（假设数据行格式正确）
		if (tokens.size() != 6)
		{
			continue;
		}
		// 创建一个 DataEntry 结构体并填充数据
		DataEntry entry;
		entry.code = tokens[0];
		entry.atm = tokens[1];
		entry.intv = std::stoi(tokens[2]);
		entry.grade = std::stoi(tokens[3]);
		entry.swc = std::stof(tokens[4]);
		entry.drrp = std::stof(tokens[5]);

		// 将填充后的结构体添加到 data 向量中
		data.push_back(entry);
	}
	file.close();
	return data;
}



/// <summary>
/// 获取下载最新数据的发布时间
/// </summary>
/// <param name="directory_path"></param>
/// <returns></returns>
std::string get_latest_data_pub_time(const std::string& directory_path)
{
	std::string latest_tm = "";
	// 定义时间格式的正则表达式
	std::regex time_regex(R"(\d{8})"); // 匹配8位数字

	// 检查目录是否存在
	if (std::filesystem::exists(directory_path) && std::filesystem::is_directory(directory_path))
	{
		// 遍历目录下的所有条目
		for (const auto& entry : std::filesystem::directory_iterator(directory_path))
		{
			// 检查是否为目录
			if (std::filesystem::is_directory(entry.status()))
			{
				if (entry.path().filename().string() > latest_tm && std::regex_match(entry.path().filename().string(), time_regex))
				{
					latest_tm = entry.path().filename().string();
				}
			}
		}
	}
	else
	{
		std::cout << directory_path << ": 目录不存在或不是一个目录" << std::endl;
	}

	return latest_tm;
}

// 查找指定目录下日期最大的文件，并返回其路径
std::string findLatestFile(const std::string& directoryPath)
{
	std::string latestFilePath = "";
	std::string latesttm = "";
	std::regex time_regex(R"(\d{12})"); // 匹配8位数字

	for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
	{
		if (entry.is_regular_file())
		{
			if (entry.path().filename().string() > latesttm && std::regex_match(entry.path().filename().stem().string(), time_regex))
			{
				latesttm = entry.path().filename().string();
				latestFilePath = entry.path().string();
				int b = 0;
			}
		}
	}

	return latestFilePath;
}



/// <summary>
/// 插入数据到数据库
/// </summary>
/// <param name="odbcConnStr"></param>
/// <param name="insertSQL"></param>
/// <param name="data"></param>
/// <returns></returns>
bool insertData(const std::string& odbcConnStr, const std::string& insertSQL, const std::vector<DataEntry>& data, const std::string pubTm, const int warnTypeID)
{
	bool status = false;
	otl_connect db;
	try
	{
		// 连接数据库
		db.rlogon(odbcConnStr.c_str(), false);
		db.auto_commit_off();  // 关闭自动提交，以便手动控制事务
		otl_stream insert_stream;
		insert_stream.open(256, insertSQL.c_str(), db);
		insert_stream.set_commit(false); // 设置流的自动提交为 false，需要手动调用 commit

		otl_value<int> typID(warnTypeID);
		otl_datetime atm = otl_tools::otl_time_from_string(pubTm);

		// 循环插入每条数据
		for (const auto& entry : data)
		{
			otl_value<std::string> code(entry.code.c_str());
			otl_value<int> intv(entry.intv);
			otl_value<int> grade(entry.grade);
			otl_value<float> drrp(entry.drrp);
			otl_write_row(insert_stream, code, atm, intv, grade, drrp, typID);
		}
		insert_stream.flush(); // 刷新插入流确保所有数据已经写入

		db.commit();	// 提交事务
		insert_stream.close();
		// 断开数据库连接
		db.logoff();
		status = true;
		return status;
	}
	catch (otl_exception& ex)
	{
		std::cout << "OTL Exception: " << ex.msg << std::endl;
		std::cout << ex.stm_text << std::endl;
		std::cout << ex.var_info << std::endl;
		db.rollback(); // 出现异常时回滚事务
		// 断开数据库连接
		db.logoff();
		status = false;
		return status;
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		db.rollback(); // 出现异常时回滚事务
		// 断开数据库连接
		db.logoff();
		status = false;
		return status;
	}
}



bool selectData(const std::string& odbcConnStr, const std::string& selectSQL, std::vector<DataEntry>& data, const int warnTypeID)
{
	bool status = false;
	otl_connect db;
	try
	{
		// 连接数据库
		db.rlogon(odbcConnStr.c_str(), false);
		db.auto_commit_off();  // 关闭自动提交，以便手动控制事务
		otl_stream select_stream;
		select_stream.open(256, selectSQL.c_str(), db);
		select_stream.set_commit(false); // 设置流的自动提交为 false，需要手动调用 commit
		//select_stream << warnTypeID;  // 第一种传入参数
		otl_write_row(select_stream, warnTypeID); // 第二种传入参数的方式
		// 循环插入每条数据
		while(!select_stream.eof())
		{
			otl_value<std::string> code;
			otl_datetime atm;
			otl_value<int> intv;
			otl_value<int> grade;
			otl_value<float> drrp;
			otl_value<int> typID;
			otl_read_row(select_stream, code, atm, intv, grade, drrp, typID);
			DataEntry temp;
			temp.code = code.v;
			temp.atm = otl_tools::otl_time_to_string(atm);
			temp.intv = intv.v;
			temp.grade = grade.v;
			temp.drrp = drrp.v;
			temp.wtypeId = typID.v;

			data.push_back(temp);
		}
		select_stream.flush(); // 刷新插入流确保所有数据已经写入
		db.commit();	// 提交事务
		select_stream.close();
		// 断开数据库连接
		db.logoff();
		status = true;
		return status;
	}
	catch (otl_exception& ex)
	{
		std::cout << "OTL Exception: " << ex.msg << std::endl;
		std::cout << ex.stm_text << std::endl;
		std::cout << ex.var_info << std::endl;
		db.rollback(); // 出现异常时回滚事务
		// 断开数据库连接
		db.logoff();
		status = false;
		return status;
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		db.rollback(); // 出现异常时回滚事务
		// 断开数据库连接
		db.logoff();
		status = false;
		return status;
	}
}

bool updateData(const std::string& odbcConnStr, const std::string& updateSQL, const int oldWarnTypeID, const int newWarnTypeID)
{
	bool status = false;
	otl_connect db;
	try
	{
		// 连接数据库
		db.rlogon(odbcConnStr.c_str(), false);
		db.auto_commit_off();  // 关闭自动提交，以便手动控制事务
		otl_stream update_stream;
		update_stream.open(256, updateSQL.c_str(), db);
		update_stream.set_commit(false); // 设置流的自动提交为 false，需要手动调用 commit
		
		// 传入参数
		update_stream << newWarnTypeID << oldWarnTypeID;

		//otl_write_row(update_stream, oldWarnTypeID, newWarnTypeID); // 第二种传入参数的方式

		//  执行更新操作
		update_stream.flush(); // 刷新插入流确保所有数据已经写入
		db.commit();	// 提交事务
		update_stream.close();
		// 断开数据库连接
		db.logoff();
		status = true;
		return status;
	}
	catch (otl_exception& ex)
	{
		std::cout << "OTL Exception: " << ex.msg << std::endl;
		std::cout << ex.stm_text << std::endl;
		std::cout << ex.var_info << std::endl;
		db.rollback(); // 出现异常时回滚事务
		// 断开数据库连接
		db.logoff();
		status = false;
		return status;
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		db.rollback(); // 出现异常时回滚事务
		// 断开数据库连接
		db.logoff();
		status = false;
		return status;
	}
}


bool deleteData(const std::string& odbcConnStr, const std::string& deleteSQL, const int WarnTypeID)
{
	bool status = false;
	otl_connect db;
	try
	{
		// 连接数据库
		db.rlogon(odbcConnStr.c_str(), false);
		db.auto_commit_off();  // 关闭自动提交，以便手动控制事务
		otl_stream delete_stream;
		delete_stream.open(256, deleteSQL.c_str(), db);
		delete_stream.set_commit(false); // 设置流的自动提交为 false，需要手动调用 commit

		// 执行更新操作
		delete_stream << WarnTypeID;

		//  执行更新操作
		delete_stream.flush(); // 刷新插入流确保所有数据已经写入
		db.commit();	// 提交事务
		delete_stream.close();
		// 断开数据库连接
		db.logoff();
		status = true;
		return status;
	}
	catch (otl_exception& ex)
	{
		std::cout << "OTL Exception: " << ex.msg << std::endl;
		std::cout << ex.stm_text << std::endl;
		std::cout << ex.var_info << std::endl;
		db.rollback(); // 出现异常时回滚事务
		// 断开数据库连接
		db.logoff();
		status = false;
		return status;
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		db.rollback(); // 出现异常时回滚事务
		// 断开数据库连接
		db.logoff();
		status = false;
		return status;
	}
}


///// <summary>
///// 从数据库中删除数据
///// </summary>
///// <param name="odbcConnStr"></param>
///// <param name="deleteSql"></param>
///// <param name="tm"></param>
///// <param name="warnTypeID"></param>
///// <returns></returns>
//bool deleteData(const std::string& odbcConnStr, const std::string& deleteSql, const int warnTypeID)
//{
//	otl_connect db;
//	try
//	{
//		db.rlogon(odbcConnStr.c_str());
//		db.direct_exec(deleteSql.c_str());
//		db.commit();
//		db.logoff();
//		return true;
//
//	}
//	catch (otl_exception& ex)
//	{
//		std::cout << "OTL Exception: " << ex.msg << std::endl;
//		std::cout << ex.stm_text << std::endl;
//		std::cout << ex.var_info << std::endl;
//		db.rollback(); // 出现异常时回滚事务
//		// 断开数据库连接
//		db.logoff();
//		return false;
//	}
//	catch (std::exception& e)
//	{
//		std::cout << "Exception: " << e.what() << std::endl;
//		db.rollback(); // 出现异常时回滚事务
//		// 断开数据库连接
//		db.logoff();
//		return false;
//	}
//}










///////////////////////////////////////////////////

/// <summary>
/// 插入数据到数据库
/// </summary>
/// <param name="db">已连接的数据库对象</param>
/// <param name="insertSQL">插入数据的SQL语句</param>
/// <param name="data">要插入的数据</param>
/// <param name="pubTm">发布时间</param>
/// <param name="warnTypeID">警告类型ID</param>
/// <returns>true 如果插入成功，否则 false</returns>
bool insertDataIntoTable(otl_connect& db, const std::string& insertSQL, const std::vector<DataEntry>& data, const std::string& pubTm)
{
	try
	{
		db.auto_commit_off();
		otl_stream insert_stream;
		insert_stream.open(256, insertSQL.c_str(), db);
		insert_stream.set_commit(false);

		//otl_value<int> typID(warnTypeID);
		otl_datetime atm = otl_tools::otl_time_from_string(pubTm);

		// 循环插入每条数据
		for (const auto& entry : data)
		{
			otl_value<std::string> code(entry.code.c_str());
			otl_value<int> intv(entry.intv);
			otl_value<int> grade(entry.grade);
			otl_value<float> drrp(entry.drrp);
			otl_write_row(insert_stream, code, atm, intv, grade, drrp);
		}
		insert_stream.flush();

		return true;
	}
	catch (otl_exception& ex)
	{
		std::cout << "OTL Exception: " << ex.msg << std::endl;
		std::cout << ex.stm_text << std::endl;
		std::cout << ex.var_info << std::endl;
		db.rollback(); // 出现异常时回滚事务
		return false;
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		db.rollback(); // 出现异常时回滚事务
		return false;
	}
}

/// <summary>
/// 从数据库中删除数据
/// </summary>
/// <param name="db">已连接的数据库对象</param>
/// <param name="deleteSql">删除数据的SQL语句</param>
/// <param name="warnTypeID">警告类型ID</param>
/// <returns>true 如果删除成功，否则 false</returns>
bool deleteDataIntoTable(otl_connect& db, const std::string& odbcConnStr, const std::string& deleteSql)
{
	try
	{
		db.direct_exec(deleteSql.c_str());
		return true;
	}
	catch (otl_exception& ex)
	{
		std::cout << "OTL Exception: " << ex.msg << std::endl;
		std::cout << ex.stm_text << std::endl;
		std::cout << ex.var_info << std::endl;
		db.rollback(); // 出现异常时回滚事务
		return false;
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		db.rollback(); // 出现异常时回滚事务
		return false;
	}
}

/// <summary>
/// 删除数据并插入数据到数据库，保证在同一个事务中
/// </summary>
/// <param name="odbcConnStr">ODBC连接字符串</param>
/// <param name="deleteSql">删除数据的SQL语句</param>
/// <param name="insertSQL">插入数据的SQL语句</param>
/// <param name="data">要插入的数据</param>
/// <param name="pubTm">发布时间</param>
/// <param name="warnTypeID">警告类型ID</param>
/// <returns>true 如果操作成功，否则 false</returns>
bool deleteAndInsertData(otl_connect& db, const std::string& odbcConnStr, const std::string& deleteSql, const std::string& insertSQL, const std::vector<DataEntry>& data, const std::string& pubTm)
{
	try
	{
		db.rlogon(odbcConnStr.c_str(), false); // 开始事务
		bool deleteSuccess = deleteDataIntoTable(db, odbcConnStr, deleteSql);
		bool insertSuccess = insertDataIntoTable(db, insertSQL, data, pubTm);

		if (deleteSuccess && insertSuccess)
		{
			db.commit(); // 提交事务
			return true;
		}
		else
		{
			db.rollback(); // 回滚事务
			return false;
		}
	}
	catch (otl_exception& ex)
	{
		std::cout << "OTL Exception: " << ex.msg << std::endl;
		std::cout << ex.stm_text << std::endl;
		std::cout << ex.var_info << std::endl;
		db.rollback(); // 出现异常时回滚事务
		return false;
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		db.rollback(); // 出现异常时回滚事务
		return false;
	}
}


/////////////////////////////////////////////////////





int main()
{

	Config config;
#ifndef NDEBUG
	std::string configPath = "../../../../config/config.json";
#else
	std::string configPath = "./config/config.json";
#endif
	if (Config::instance().read_config(configPath))
	{
		std::cout << "Read config file succession " << std::endl;
	}
	else
	{
		std::cout << "ERROR : Failed to read config file " << std::endl;
		return 1;
	}
	std::string path = Config::instance().fpath;

	std::string current_pro_file = "";

	// 创建一个存储 DataEntry 结构的向量
	std::vector<DataEntry> entries;

	// 添加四条数据，其中 WarnTypeID 都为 1000
	for (int i = 0; i < 4; ++i)
	{
		DataEntry entry;
		entry.code = "example_code" + std::to_string(i + 1);
		entry.atm = "2024-07-09 12:00:00"; 
		entry.intv = 60;                   
		entry.grade = 1;                   
		entry.swc = 0.0f;                  
		entry.drrp = 10.5f;                
		entry.wtypeId = 1000;
		entries.push_back(entry);
	}

	//// 插入
	//if (insertData(Config::instance().odbc, Config::instance().inster_sql, entries, "2024-07-09 12:00:00", 1000))
	//{
	//	std::cout << "insert successed" << std::endl;
	//}
	//// 查找

	//if (updateData(Config::instance().odbc, Config::instance().update_sql, 1000, 2000))
	//{
	//	std::cout << "update successed" << std::endl;
	//}

	std::vector<DataEntry> selectEntry;
	if (selectData(Config::instance().odbc, Config::instance().select_sql, selectEntry, 54))
	{
		std::cout << "select successed" << std::endl;
	}

	//if (deleteData(Config::instance().odbc, Config::instance().delete_sql, 2000))
	//{
	//	std::cout << "delete successed" << std::endl;
	//}


	int b = 0;



	//while (true)
	//{
	//	bool is_pro = false;
	//	std::string latestDay = get_latest_data_pub_time(path);
	//	std::filesystem::path latestDir(path);
	//	latestDir.append(latestDay);
	//	std::string latestFile = findLatestFile(latestDir.string());
	//	std::string latestFileTm = std::filesystem::path(latestFile).filename().stem().string();
	//	if (latestFile > current_pro_file)
	//	{

	//		std::string latestProFileTm = convertTimeFormat(latestFileTm);

	//		//// 读取数据
	//		std::vector<DataEntry> data = readSplitData(latestFile);
	//		if (!data.empty())
	//		{
	//			otl_connect db;
	//			if (deleteAndInsertData(db, Config::instance().odbc, Config::instance().delete_sql, Config::instance().inster_sql, data, latestProFileTm))
	//			{
	//				// 更新时间
	//				current_pro_file = latestFile;
	//			}
	//			// 断开数据库连接
	//			db.logoff();
	//		}

	//	}

	//	std::this_thread::sleep_for(std::chrono::seconds(60));  // 秒
	//}


	return 0;
}