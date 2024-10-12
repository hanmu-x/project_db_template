#pragma once

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <vector>
#include <fstream>

#include "database/otl/otl_tools.h"
#include "json/silly_jsonpp.h"
#include "singleton/silly_singleton.h"


class Config : public silly_singleton<Config>
{
public:

	std::string odbc;
	std::string select_sql;
	std::string update_sql;
	std::string inster_sql;
	std::string delete_sql;
	std::string fpath; 	// 文件根路径
	//int warnTypeID;

	bool read_config(std::string config_path)
	{
		bool status = false;
		Json::Value jv_root = silly_jsonpp::loadf(config_path);
		if (jv_root.isNull())
		{
			SFP_ERROR("配置文件读取失败: {}", config_path);
			return status;
		}
		otl_conn_opt opt = otl_tools::conn_opt_from_json(jv_root["db"]);
		odbc = opt.dump_odbc();
		fpath = jv_root["path"].asString();
		select_sql = jv_root["sql"]["select_sql"].asString();
		update_sql = jv_root["sql"]["update_sql"].asString();
		inster_sql = jv_root["sql"]["inster_sql"].asString();
		delete_sql = jv_root["sql"]["delete_sql"].asString();

		status = true;
		return status;
	}

};

#endif //CONFIG_HPP
