#ifndef MY_APP_H_
#define MY_APP_H_

#include <set>
#include <string>
#include <sstream>
#include "DataType.h"
#include "OrderData.h"
#include "XtpTradeReq.h"
#include "XtpTradeRes.h"
#include "Config.h"
#include "Log.h"



class CMyApp
{
public:
	CMyApp();
	~CMyApp();

	void set_last_error(const char *szError);
	std::string get_last_error();

	// 连接xtp服务器
	bool connect_xtp();

	int generate_request_id();

	static void deadline_handler(const boost::system::error_code &e, CMyApp *pThis);

	void set_login_status(bool bLogin);
	bool is_login();

	// 重新登录
	void relogin();

	boost::asio::io_service & get_io_service();

	template <class Type>
	static Type stringToNum(const std::string & str)
	{
		std::istringstream iss(str);
		Type num;
		iss >> num;
		return num;
	}

	size_t generate_msg_id();

private:
	boost::asio::io_service m_io_service;
	std::string		m_strLastError;			// 交易前置最后一次的出错信息
	int				m_nReconnectCount;      // 累计每一次重连成功时的重连次数
	bool            m_bLogined;			    // 是否已登录xtp交易服务器
	bool			m_bInitCfg;				// 是否初始化了交易前置配置
	boost::mutex	m_mutexRequestID;
	boost::asio::deadline_timer m_timerReconnect;	// 重连定时器
	boost::mutex    m_mutex_msgid;

public:
	server_ptr      m_server;
	COrderData		m_orderDta;				// 请求包和应答包的处理对象
	CXtpTradeReq	m_xtpReq;				// xtp请求对象
	CXtpTraderRes	*m_xtpRes;				// xtp应答对象
	CConfig			m_config;				// 用于配置操作
	CLog			m_log;
	

};

extern CMyApp theApp;

#endif

