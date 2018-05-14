#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/ini_parser.hpp"
#include "boost/filesystem.hpp"
#include "MyApp.h"

CMyApp theApp;

const char *g_error_msg[] = {
	"Successful operation",
	"The log directory of XTP is invalid",
	"The key of XTP is invalid",
	"The address of XTP is invalid",
	"The login account of XTP is invalid",
	"The request object of XTP is invalid"
};

CMyApp::CMyApp()
	:m_bInitCfg(false),
	m_xtpRes(NULL),
	m_nReconnectCount(0),
	m_bLogined(false),
	m_timerReconnect(m_io_service, boost::posix_time::seconds(RECONNEC_INTERVAL_TIME))
{
	m_xtpRes = new CXtpTraderRes;

	m_log.init("TradeRunner", true);
}

CMyApp::~CMyApp()
{
	delete m_xtpRes;
}

void CMyApp::set_last_error(const char * szError)
{
	if (!szError || strlen(szError) == 0)
	{
		m_strLastError = "Invalid error information";
	}
	else
	{
		m_strLastError = szError;
	}
}

std::string CMyApp::get_last_error()
{
	return m_strLastError;
}

bool CMyApp::connect_xtp()
{
	T_XtpTradeInfo & xtpTradeInfo = theApp.m_config.getXtpTradeInfo(0);
	m_xtpReq.init(xtpTradeInfo.szKey, "../log/xtpLib/", xtpTradeInfo.clientID, m_xtpRes);
	if (!m_xtpReq.doLogin(xtpTradeInfo.szUserName, xtpTradeInfo.szPwd,
		                  xtpTradeInfo.tAddr.szIP, xtpTradeInfo.tAddr.nPort))
	{
		m_bLogined = false;
		printf("login xtp server failure, cause: %s\n", theApp.get_last_error().c_str());
	}
	else
	{
		m_bLogined = true;
		printf("Login xtp server successfully!\n");
	}

	return true;
}

int CMyApp::generate_request_id()
{
	static int nRequestID = 0;
	int nTempRequestID=0;
	boost::mutex::scoped_lock lock(m_mutexRequestID);
	{
		nTempRequestID = ++nRequestID;
	}
	return nTempRequestID;
}

void CMyApp::deadline_handler(const boost::system::error_code & e, CMyApp * pThis)
{
	T_XtpTradeInfo &tXtpTrade = pThis->m_config.getXtpTradeInfo(0);

	printf("Reconnect %d times!\n", pThis->m_nReconnectCount);

	if (!pThis->m_xtpReq.doLogin(tXtpTrade.szUserName, tXtpTrade.szPwd,
		                         tXtpTrade.tAddr.szIP, tXtpTrade.tAddr.nPort))
	{
		pThis->m_bLogined = false;

		pThis->m_timerReconnect.expires_at(pThis->m_timerReconnect.expires_at() + boost::posix_time::seconds(RECONNEC_INTERVAL_TIME));
		pThis->m_timerReconnect.async_wait(boost::bind(&CMyApp::deadline_handler, boost::asio::placeholders::error, pThis));
	}
	else
	{
		pThis->m_bLogined = true;
		printf("Login xtp server successfully!\n");
	}
}

void CMyApp::set_login_status(bool bLogin)
{
	m_bLogined = bLogin;
}

bool CMyApp::is_login()
{
	return m_bLogined;
}

void CMyApp::relogin()
{
	++m_nReconnectCount;
	m_timerReconnect.async_wait(boost::bind(&CMyApp::deadline_handler, boost::asio::placeholders::error, this));
}

boost::asio::io_service & CMyApp::get_io_service()
{
	return m_io_service;
}

size_t CMyApp::generate_msg_id()
{
	boost::mutex::scoped_lock lock(m_mutex_msgid);
	static size_t msgID = 0;
	++msgID;

	return msgID;
}