#ifndef TRADE_GETWAY_TRADE_RES_H_
#define TRADE_GETWAY_TRADE_RES_H_
#include <map>
#include "xtp_trader_api.h"
#include "macro.h"

class CXtpTraderRes;

class CXtpTradeReq
{
public:
	CXtpTradeReq();
	~CXtpTradeReq();

private:
	bool setLastError(int nPos);

	bool createCatalog(const char *szCatalog);

public:

	/*
	 * @name init
	 *
	 * @brief 初始化xtp交易接口
	 *
	 * @param [in] szKey		  软件开发Key（由中泰给）
	 * @param [in] szFilePath     存贮订阅信息文件的目录（一个真实存在的有可写权限的路径）
	 * @param [in] nClientID      客户id（用户自定义的）
	 *
	 * @return true-初始化成功  false-初始化失败，调用getLastErrorMsg获取错误信息
	 */
	bool init(const char *szKey, const char *szFilePath, int nClientID, CXtpTraderRes *pTradeSpi);

	/*
	* @name doLogin
	*
	* @brief 登录xtp交易服务器
	*
	* @param [in] szUserName	登录名
	* @param [in] szPwd			登录密码
	* @param [in] szIP			xtp交易服务器的IP
	* @param [in] nPort			xtp交易服务器的端口
	*
	* @return 非0-登录成功(该返回值是会话ID)  0-登录失败，调用getLastErrorMsg获取错误信息
	*/
	bool doLogin(const char *szUserName, const char *szPwd, const char *szIP, int nPort);

	// 登出
	bool loginout();

	// 委托下单
	bool doEntrustOrder(XTPOrderInsertInfo *order, uint64_t &orderID);

	// 委托撤单
	uint64_t doCancelEntrust(const uint64_t entrustID);

	// 委托查询(根据多条件查询)
	bool doQueryEntrust(const XTPQueryOrderReq *queryParam, int nRequestID);

	// 委托查询(根据委托ID查询)
	bool doQueryEntrustForID(const uint64_t entrustID, int nRequestID);

	// 成交查询（根据多条件查询)
	bool doQueryTrade(XTPQueryTraderReq *queryParam, int nRequestID);

	// 成交查询(根据成交ID查询)
	bool doQueryTradeForID(const uint64_t tradeID, int nRequestID);

	// 持仓查询
	bool doQueryHold(const char *ticker, int nRequestID);

	// 资产查询
	bool doQueryAssert(int nRequestID);

private:
	
	XTP::API::TraderApi*  m_pXtpTradeReq;				// xtp请求对象
	
	uint64_t              m_uSessionID;					// 和xtp交易服务器通信的会话ID

};


#endif
