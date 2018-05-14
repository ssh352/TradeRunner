#include <string>
#include "xtp_trader_api.h"
#include "XtpTradeReq.h"
#include "XtpTradeRes.h"
#include "DataType.h"
#include "MyApp.h"

extern const char *g_error_msg[];

CXtpTradeReq::CXtpTradeReq()
{
	m_pXtpTradeReq = NULL;
	m_uSessionID = 0;
}

CXtpTradeReq::~CXtpTradeReq()
{
}

bool CXtpTradeReq::setLastError(int nPos)
{
	// 检查下标是否合法
	if (nPos < 0 || nPos > TG_LAST_END - 1)
		return false;

	theApp.set_last_error(g_error_msg[nPos]);
	
	return true;
}

bool CXtpTradeReq::createCatalog(const char * szCatalog)
{
	if (!szCatalog)
		return false;

	if (strlen(szCatalog) == 0)
		return 0;

	return false;
}

bool CXtpTradeReq::init(const char *szKey, const char *szFilePath, int nClientID, CXtpTraderRes *pTradeSpi)
{
	if (!szFilePath || strlen(szFilePath) == 0)
	{
		setLastError(TG_XTP_LOG_CATALOG_INVALID);
		return false;
	}

	if (!szKey || strlen(szKey) == 0)
	{
		setLastError(TG_XTP_KEY_INVALID);
		return false;
	}

	m_pXtpTradeReq = XTP::API::TraderApi::CreateTraderApi(nClientID, szFilePath);

	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_ADDR_INVALID);
		return false;
	}

	m_pXtpTradeReq->SetSoftwareVersion("1.1.0");
	m_pXtpTradeReq->SetSoftwareKey(szKey);

	m_pXtpTradeReq->RegisterSpi(pTradeSpi);// 注册回调对象

	setLastError(TG_SUCCESS);

	return true;
}

bool CXtpTradeReq::doLogin(const char *szUserName, const char *szPwd, const char *szIP, int nPort)
{
	if (nPort <= 0 || !szIP || strlen(szIP) == 0)
	{
		setLastError(TG_XTP_ADDR_INVALID);
		return false;
	}

	if (!szUserName || strlen(szUserName) == 0 || 
		!szPwd || strlen(szPwd) == 0)
	{
		setLastError(TG_XTP_ACCOUNT_INVALID);
		return false;
	}

	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return false;
	}

	m_uSessionID = m_pXtpTradeReq->Login(szIP, nPort, szUserName, szPwd, XTP_PROTOCOL_TCP);
	if (0 == m_uSessionID)
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
		return false;
	}

	return true;
}

bool CXtpTradeReq::loginout()
{
	if (!m_pXtpTradeReq->Logout(m_uSessionID))
		return false;
	
	return true;
}

bool CXtpTradeReq::doEntrustOrder(XTPOrderInsertInfo *order, uint64_t &orderID)
{
	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return false;
	}
	
	orderID = m_pXtpTradeReq->InsertOrder(order, m_uSessionID);
	if (!orderID)
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
		return false;
	}

	return true;
}

uint64_t CXtpTradeReq::doCancelEntrust(const uint64_t entrustID)
{
	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return 0;
	}

	uint64_t cancelOrderID = m_pXtpTradeReq->CancelOrder(entrustID, m_uSessionID);
	if (!cancelOrderID)
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
	}

	return cancelOrderID;
}

bool CXtpTradeReq::doQueryEntrust(const XTPQueryOrderReq *queryParam, int nRequestID)
{
	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return false;
	}

	XTPQueryOrderReq qp;
	memset(&qp, 0, sizeof(XTPQueryOrderReq));

	int nRet = m_pXtpTradeReq->QueryOrders(&qp/*queryParam*/, m_uSessionID, nRequestID);
	if (0 != nRet)
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
		return false;
	}

	return true;
}

bool CXtpTradeReq::doQueryEntrustForID(const uint64_t entrustID, int nRequestID)
{
	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return false;
	}

	int nRet = m_pXtpTradeReq->QueryOrderByXTPID(entrustID, m_uSessionID, nRequestID);
	if (0 != nRet)
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
		return false;
	}

	return true;
}

bool CXtpTradeReq::doQueryTrade(XTPQueryTraderReq *queryParam, int nRequestID)
{
	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return false;
	}

	XTPQueryTraderReq qt;
	memset(&qt, 0, sizeof(XTPQueryTraderReq));

	int nRet = m_pXtpTradeReq->QueryTrades(&qt/*queryParam*/, m_uSessionID, nRequestID);
	if (0 != nRet)
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
		return false;
	}

	return true;
}

bool CXtpTradeReq::doQueryTradeForID(const uint64_t tradeID, int nRequestID)
{
	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return false;
	}

	int nRet = m_pXtpTradeReq->QueryTradesByXTPID(tradeID, m_uSessionID, nRequestID);
	if (0 != nRet)
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
		return false;
	}

	return true;
}

bool CXtpTradeReq::doQueryHold(const char *ticker, int nRequestID)
{
	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return false;
	}

	int nRet = m_pXtpTradeReq->QueryPosition(""/*ticker*/, m_uSessionID, nRequestID);
	if (0 != nRet)
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
		return false;
	}

	return true;
}

bool CXtpTradeReq::doQueryAssert(int nRequestID)
{
	if (!m_pXtpTradeReq)
	{
		setLastError(TG_XTP_REQ_OBJ_INVALID);
		return false;
	}

	if (0 != m_pXtpTradeReq->QueryAsset(m_uSessionID, nRequestID))
	{
		XTPRI *p = m_pXtpTradeReq->GetApiLastError();
		theApp.set_last_error(p->error_msg);
		return false;
	}

	return true;
}
