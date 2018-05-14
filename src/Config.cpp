#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/ini_parser.hpp"
#include "Config.h"



CConfig::CConfig()
	:m_nThreadCount(1)
{
}

CConfig::~CConfig()
{
}

bool CConfig::read_config()
{
	bool bRet;
	try
	{
		boost::property_tree::ptree pt;
		boost::property_tree::ini_parser::read_ini("../etc/TradeRunner.ini", pt);

		std::string ipServer = pt.get<std::string>("server.servIP");
		int         portServer = pt.get<int>("server.servPort", 8182);
		strncpy(m_tTradeGetwayAddr.szIP, ipServer.c_str(), MAX_IP);
		m_tTradeGetwayAddr.nPort = portServer;

		//m_nThreadCount = pt.get<int>("server.threadCount", 1);

		std::string ipTradeSys = pt.get<std::string>("tradeSys.sysIP", "");
		int         portTradeSys = pt.get<int>("tradeSys.sysPort", 8186);
		strncpy(m_tTradeSysAddr.szIP, ipTradeSys.c_str(), MAX_IP);
		m_tTradeSysAddr.nPort = portTradeSys;

		m_nClientID = pt.get<int>("xtpTrade.clientID", 1);
		std::string xtpIP1 = pt.get<std::string>("xtpTrade.ip1", "");
		int nPort1 = pt.get<int>("xtpTrade.port1", 6001);
		std::string username1 = pt.get<std::string>("xtpTrade.username1", "");
		std::string pwd1 = pt.get<std::string>("xtpTrade.pwd1", "");
		std::string key1 = pt.get<std::string>("xtpTrade.key1", "");
		if (!xtpIP1.empty() && !username1.empty() && !pwd1.empty() && !key1.empty())
		{
			m_xtpTradeInfo[0].clientID = m_nClientID;
			strncpy(m_xtpTradeInfo[0].tAddr.szIP, xtpIP1.c_str(), MAX_IP);
			m_xtpTradeInfo[0].tAddr.nPort = nPort1;
			strncpy(m_xtpTradeInfo[0].szUserName, username1.c_str(), MAX_USERNAME);
			strncpy(m_xtpTradeInfo[0].szPwd, pwd1.c_str(), MAX_PWD);
			strncpy(m_xtpTradeInfo[0].szKey, key1.c_str(), MAX_KEY);
		}

		std::string xtpIP2 = pt.get<std::string>("xtpTrade.ip2", "");
		int nPort2 = pt.get<int>("xtpTrade.port2", 6001);
		std::string username2 = pt.get<std::string>("xtpTrade.username2", "");
		std::string pwd2 = pt.get<std::string>("xtpTrade.pwd2", "");
		std::string key2 = pt.get<std::string>("xtpTrade.key2", "");
		if (!xtpIP2.empty() && !username2.empty() && !pwd2.empty() && !key2.empty())
		{
			m_xtpTradeInfo[1].clientID = m_nClientID;
			strncpy(m_xtpTradeInfo[1].tAddr.szIP, xtpIP2.c_str(), MAX_IP);
			m_xtpTradeInfo[1].tAddr.nPort = nPort2;
			strncpy(m_xtpTradeInfo[1].szUserName, username2.c_str(), MAX_USERNAME);
			strncpy(m_xtpTradeInfo[1].szPwd, pwd2.c_str(), MAX_PWD);
			strncpy(m_xtpTradeInfo[1].szKey, key2.c_str(), MAX_KEY);
		}

		bRet = true;
	}
	catch (std::exception &e)
	{
		bRet = false;
		printf("%s\n", e.what());
	}

	return bRet;
}

T_Addr & CConfig::getTradeGetwayAddr()
{
	return m_tTradeGetwayAddr;
}

T_Addr & CConfig::getTradeSysAddr()
{
	return m_tTradeSysAddr;
}

T_XtpTradeInfo & CConfig::getXtpTradeInfo(int pos)
{
	return m_xtpTradeInfo[pos];
}

int CConfig::get_thread_count()
{
	return m_nThreadCount;
}

int CConfig::get_client_id()
{
	return m_nClientID;
}
