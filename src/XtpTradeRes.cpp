#include <iostream>
#include <cstring>
#include <iomanip>
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/format.hpp"
#include "XtpTradeRes.h"
#include "macro.h"
#include "DataType.h"
#include "MySession.h"
#include "MyApp.h"




CXtpTraderRes::CXtpTraderRes()
{

}

CXtpTraderRes::~CXtpTraderRes()
{
}

void CXtpTraderRes::OnDisconnected(uint64_t session_id, int reason)
{
	printf("Disconnect the XTP trade server!!!\n");

	theApp.set_login_status(false);

	theApp.relogin();// 开启重连
}

void CXtpTraderRes::OnError(XTPRI *error_info)
{
	printf("OnError, %s\n", error_info->error_msg);
}

void CXtpTraderRes::OnOrderEvent(XTPOrderInfo *order_info, XTPRI *error_info, uint64_t session_id)
{
	if (0 != error_info->error_id)
	{
		theApp.set_last_error(error_info->error_msg);
	}

	if (order_info->order_status != XTP_ORDER_STATUS_NOTRADEQUEUEING)// 只要不是未成交，那么就向交易系统发送
	{
		T_Packet tPacket;
		tPacket.enPacketType = PT_ORDER;
		tPacket.nCount = 1;
		tPacket.nStatus = 1;
		tPacket.data = new char[sizeof(XTPOrderInfo)];
		memcpy(tPacket.data, (char*)order_info, sizeof(XTPOrderInfo));
		theApp.m_orderDta.push_packet_res(tPacket);
	}
	else
	{
	}
}

void CXtpTraderRes::OnTradeEvent(XTPTradeReport *trade_info, uint64_t session_id)
{
	T_Packet tPacket;
	tPacket.enPacketType = PT_TRADE;
	tPacket.nCount = 1;
	tPacket.nStatus = 1;
	tPacket.data = new char[sizeof(XTPTradeReport)];
	memcpy(tPacket.data, (char*)trade_info, sizeof(XTPTradeReport));
	theApp.m_orderDta.push_packet_res(tPacket);
}

void CXtpTraderRes::OnCancelOrderError(XTPOrderCancelInfo * cancel_info, XTPRI * error_info, uint64_t session_id)
{
	T_Packet tPacket;
	tPacket.enPacketType = PT_CANCEL_ORDER;
	tPacket.nCount = 1;
	tPacket.nStatus = 0;
	tPacket.data = new char[sizeof(XTPOrderCancelInfo)];
	memcpy(tPacket.data, (char*)cancel_info, sizeof(XTPOrderCancelInfo));
	theApp.m_orderDta.push_packet_res(tPacket);

}

void CXtpTraderRes::OnQueryOrder(XTPQueryOrderRsp * order_info, XTPRI * error_info, int request_id, bool is_last, uint64_t session_id)
{
	T_Packet tPacket;

	if (error_info->error_id != 0)
	{
		printf("xtp order query failed,cause:%s\n", error_info->error_msg);

		theApp.set_last_error(error_info->error_msg);

		tPacket.enPacketType = PT_ORDER_QUERY;
		tPacket.packetID = request_id;
		tPacket.nCount = 1;
		tPacket.nStatus = 0;
		tPacket.data = new char[1 * sizeof(XTPRI)];
		memcpy(tPacket.data, (char*)error_info, sizeof(XTPRI));
		theApp.m_orderDta.push_packet_res(tPacket);
	}
	else
	{
		static std::deque<XTPQueryOrderRsp> dequeQueryOrder;
		static int nCount = 0;
		++nCount;
		dequeQueryOrder.push_back(*order_info);

		if (is_last)
		{
			tPacket.enPacketType = PT_ORDER_QUERY;
			tPacket.packetID = request_id;
			tPacket.nCount = nCount;
			tPacket.nStatus = 1;

			int nSize = nCount * sizeof(XTPQueryOrderRsp);
			tPacket.data = new char[nSize];
			memset(tPacket.data, 0, nSize);

			for (int i = 0; i < nCount; ++i)
			{
				memcpy(tPacket.data + i * sizeof(XTPQueryOrderRsp), (char*)&dequeQueryOrder.front(), sizeof(XTPQueryOrderRsp));
				dequeQueryOrder.pop_front();
			}

			theApp.m_orderDta.push_packet_res(tPacket);
			nCount = 0;
		}
	}
}

void CXtpTraderRes::OnQueryTrade(XTPQueryTradeRsp * trade_info, XTPRI * error_info, int request_id, bool is_last, uint64_t session_id)
{
	T_Packet tPacket;

	if (error_info->error_id != 0)
	{
		printf("xtp trade query failed,cause:%s\n", error_info->error_msg);

		theApp.set_last_error(error_info->error_msg);

		tPacket.enPacketType = PT_TRADE_QUERY;
		tPacket.packetID = request_id;
		tPacket.nCount = 1;
		tPacket.nStatus = 0;
		tPacket.data = new char[1 * sizeof(XTPRI)];
		memcpy(tPacket.data, (char*)error_info, sizeof(XTPRI));
		theApp.m_orderDta.push_packet_res(tPacket);
	}
	else
	{
		static std::deque<XTPQueryTradeRsp> dequeQueryTrade;
		static int nCount = 0;
		++nCount;
		dequeQueryTrade.push_back(*trade_info);

		if (is_last)
		{
			tPacket.enPacketType = PT_TRADE_QUERY;
			tPacket.packetID = request_id;
			tPacket.nCount = nCount;
			tPacket.nStatus = 1;
			tPacket.data = new char[nCount * sizeof(XTPQueryTradeRsp)];

			for (int i = 0; i < nCount; ++i)
			{
				memcpy(tPacket.data + i * sizeof(XTPQueryTradeRsp), (char*)&dequeQueryTrade.front(), sizeof(XTPQueryTradeRsp));
				dequeQueryTrade.pop_front();
			}

			theApp.m_orderDta.push_packet_res(tPacket);
			nCount = 0;
		}
	}
}

void CXtpTraderRes::OnQueryPosition(XTPQueryStkPositionRsp * investor_position, XTPRI * error_info, int request_id, bool is_last, uint64_t session_id)
{
	T_Packet tPacket;

	if (error_info->error_id != 0)
	{
		printf("xtp position query failed,cause:%s\n", error_info->error_msg);

		theApp.set_last_error(error_info->error_msg);

		tPacket.enPacketType = PT_POSITION_QUERY;
		tPacket.packetID = request_id;
		tPacket.nCount = 1;
		tPacket.nStatus = 0;
		tPacket.data = new char[1 * sizeof(XTPRI)];
		memcpy(tPacket.data, (char*)error_info, sizeof(XTPRI));
		theApp.m_orderDta.push_packet_res(tPacket);
	}
	else
	{
		static std::deque<XTPQueryStkPositionRsp> dequeQueryPosition;
		static int nCount = 0;
		++nCount;
		dequeQueryPosition.push_back(*investor_position);
		
		if (is_last)
		{
			tPacket.enPacketType = PT_POSITION_QUERY;
			tPacket.packetID = request_id;
			tPacket.nCount = nCount;
			tPacket.nStatus = 1;
			tPacket.data = new char[nCount * sizeof(XTPQueryStkPositionRsp)];

			for (int i = 0; i < nCount; ++i)
			{
				memcpy(tPacket.data + i * sizeof(XTPQueryStkPositionRsp), (char*)&dequeQueryPosition.front(), sizeof(XTPQueryStkPositionRsp));
				dequeQueryPosition.pop_front();
			}

			theApp.m_orderDta.push_packet_res(tPacket);
			nCount = 0;
		}
	}
}

void CXtpTraderRes::OnQueryAsset(XTPQueryAssetRsp * trading_account, XTPRI * error_info, int request_id, bool is_last, uint64_t session_id)
{
	T_Packet tPacket;

	if (error_info->error_id != 0)
	{
		printf("xtp asset query failed,cause:%s\n", error_info->error_msg);

		theApp.set_last_error(error_info->error_msg);

		tPacket.enPacketType = PT_ASSET_QUERY;
		tPacket.packetID = request_id;
		tPacket.nCount = 1;
		tPacket.nStatus = 0;
		tPacket.data = new char[1 * sizeof(XTPRI)];
		memcpy(tPacket.data, (char*)error_info, sizeof(XTPRI));
		theApp.m_orderDta.push_packet_res(tPacket);
	}
	else
	{
		static std::deque<XTPQueryAssetRsp> dequeQueryAsset;
		static int nCount = 0;
		++nCount;
		dequeQueryAsset.push_back(*trading_account);

		if (is_last)
		{
			tPacket.enPacketType = PT_ASSET_QUERY;
			tPacket.packetID = request_id;
			tPacket.nCount = nCount;
			tPacket.nStatus = 1;
			tPacket.data = new char[nCount * sizeof(XTPQueryAssetRsp)];

			for (int i = 0; i < nCount; ++i)
			{
				memcpy(tPacket.data + i * sizeof(XTPQueryAssetRsp), (char*)&dequeQueryAsset.front(), sizeof(XTPQueryAssetRsp));
				dequeQueryAsset.pop_front();
			}

			theApp.m_orderDta.push_packet_res(tPacket);
			nCount = 0;
			
		}
	}
}


