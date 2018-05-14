#pragma once
#include "xtp_trader_api.h"

#define _IS_LOG_SHOW_ 0
#define MAX_REQUEST_ID_NUM 10000

using namespace XTP::API;

class CXtpTraderRes : public TraderSpi
{
public:
	CXtpTraderRes();
	~CXtpTraderRes();

	virtual void OnDisconnected(uint64_t session_id, int reason);

	///错误应答
	virtual void OnError(XTPRI *error_info);

	///报单通知
	virtual void OnOrderEvent(XTPOrderInfo *order_info, XTPRI *error_info, uint64_t session_id);

	///成交通知
	virtual void OnTradeEvent(XTPTradeReport *trade_info, uint64_t session_id);

	virtual void OnCancelOrderError(XTPOrderCancelInfo *cancel_info, XTPRI *error_info, uint64_t session_id);

	virtual void OnQueryOrder(XTPQueryOrderRsp *order_info, XTPRI *error_info, int request_id, bool is_last, uint64_t session_id);

	virtual void OnQueryTrade(XTPQueryTradeRsp *trade_info, XTPRI *error_info, int request_id, bool is_last, uint64_t session_idt);

	virtual void OnQueryPosition(XTPQueryStkPositionRsp *investor_position, XTPRI *error_info, int request_id, bool is_last, uint64_t session_id);

	virtual void OnQueryAsset(XTPQueryAssetRsp *trading_account, XTPRI *error_info, int request_id, bool is_last, uint64_t session_id);


};