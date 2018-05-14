#include "boost/shared_ptr.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/format.hpp"
#include "OrderData.h"
#include "MySession.h"
#include "AsynSession.h"
#include "MyApp.h"



COrderData::COrderData()
	:m_semaphoreSynRes(0),
	m_semaphoreReq(0)
{
}

COrderData::~COrderData()
{
	m_io_service.stop();
}

void COrderData::listen_io(COrderData * pThis)
{
	//printf("开始监听异步应答数据包!\n");
	boost::asio::io_service::work wk(pThis->m_io_service);
	pThis->m_io_service.run();
	//printf("结束监听异步应答数据包!\n");
}

void COrderData::thread_handler_res(COrderData * pThis)
{
	while (true)
	{
		pThis->m_semaphoreSynRes.wait();

		T_Packet tPacket;
		pThis->pop_packet_res(tPacket);

		pThis->handle_res_packet(tPacket);
	}
}

void COrderData::thread_handler_req(COrderData * pThis)
{
	while (true)
	{
		pThis->m_semaphoreReq.wait();// 等待请求报文

		message msg;
		pThis->pop_packet_req(msg);

		pThis->handle_req(msg);// 处理请求报文
	}
}

void COrderData::run()
{
	boost::thread th(boost::bind(&COrderData::listen_io, this));
}

void COrderData::push_packet_req(message & msg)
{
	{
		boost::mutex::scoped_lock lock(m_mutexReq);
		m_dequePacketReq.push_back(msg);
	}
	m_semaphoreReq.signal();
}

void COrderData::pop_packet_req(message &msg)
{
	boost::mutex::scoped_lock lock(m_mutexReq);
	msg = m_dequePacketReq.front();
	m_dequePacketReq.pop_front();
}

void COrderData::push_packet_res(T_Packet &tPacket)
{
	{
		boost::mutex::scoped_lock lock(m_mutexSynRes);
		m_dequePacketRes.push_back(tPacket);
	}
	m_semaphoreSynRes.signal();
}

void COrderData::pop_packet_res(T_Packet & tPacket)
{
	boost::mutex::scoped_lock lock(m_mutexSynRes);

	tPacket = m_dequePacketRes.front();
	m_dequePacketRes.pop_front();
}

void COrderData::add_packet_res(ASYN_SESSION_PTR asynSession)
{
	//boost::mutex::scoped_lock lock(m_mutexAsynRes);
	m_setAsynMsg.insert(asynSession);
}

void COrderData::leave_packet_res(ASYN_SESSION_PTR asynSession)
{
	//boost::mutex::scoped_lock lock(m_mutexAsynRes);
	m_setAsynMsg.erase(asynSession);
}

void COrderData::handle_res_packet(T_Packet & tPacket)
{
	switch (tPacket.enPacketType)
	{
	case PT_ORDER:				 // 订单
	case PT_TRADE:				 // 成交
	case PT_CANCEL_ORDER:		 // 撤单
	{
		std::string strPacket;

		if (PT_ORDER == tPacket.enPacketType)
		{
			CAsynSession::combo_push_order(tPacket, strPacket);
		}
		else if (PT_TRADE == tPacket.enPacketType)
		{
			CAsynSession::combo_push_trade(tPacket, strPacket);
		}
		else if (PT_CANCEL_ORDER == tPacket.enPacketType)
		{
			CAsynSession::combo_push_cancel_order(tPacket, strPacket);
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "Unknown response packet type[" << tPacket.enPacketType << "] " << "[" << __FILE__ << ":" << __LINE__ << "]";
			return;
		}

		if (strPacket.empty())
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "Combinatorial asynchronous response packet failure!" << "[" << __FILE__ << ":" << __LINE__ << "]";
			return;
		}

		T_Addr &addr = theApp.m_config.getTradeSysAddr();
		ASYN_SESSION_PTR session(new CAsynSession(*this, addr.szIP, addr.nPort, strPacket));
		add_packet_res(session);
	}
	break;
	case PT_ORDER_QUERY:		 // 订单查询
	case PT_TRADE_QUERY:		 // 成交查询
	case PT_POSITION_QUERY:		 // 持仓查询
	case PT_ASSET_QUERY:		 // 资产查询
	{
		const SESSION_PTR &session = theApp.m_server->get_session(tPacket.packetID);
		if (session)
		{
			std::string strPacket;
			if (PT_ORDER_QUERY == tPacket.enPacketType)
				session->combo_order_query_packet(tPacket, tPacket.packetID, strPacket);
			else if (PT_TRADE_QUERY == tPacket.enPacketType)
				session->combo_trade_query_packet(tPacket, tPacket.packetID, strPacket);
			else if (PT_POSITION_QUERY == tPacket.enPacketType)
				session->combo_position_packet(tPacket, tPacket.packetID, strPacket);
			else if (PT_ASSET_QUERY == tPacket.enPacketType)
				session->combo_asset_packet(tPacket, tPacket.packetID, strPacket);

			message msg;
			msg.fill_msg(strPacket.c_str(), strPacket.length());
			session->send(msg);
		}
		else
		{

		}
	}
	break;
	default:
		break;
	}
}

void COrderData::handle_req(message &msg)
{
	std::string strPacket(msg.body(), msg.body() + msg.body_length());

	rapidjson::Document doc;
	doc.Parse(strPacket.c_str());
	if (!doc.HasParseError())
	{
		rapidjson::Value &vHead = doc["head"];
		std::string strTradeCode = vHead["s_trdcode"].GetString();

		if (strTradeCode.compare(TS_FUNC_INSERT_ORDER) == 0)// 报单
		{
			handle_req_insert_order(doc, msg.get_msg_id());
		}
		else if (strTradeCode.compare(TS_FUNC_CANCEL_ORDER) == 0)// 撤单
		{
			handle_req_cancel_order(doc, msg.get_msg_id());
		}
		else if (strTradeCode.compare(TS_FUNC_ORDER_QUERY_FOR_ID) == 0)// 订单id查询
		{
			handle_req_order_query_for_id(doc, msg.get_msg_id());
		}
		else if (strTradeCode.compare(TS_FUNC_ORDER_QUERY) == 0)// 订单查询
		{
			handle_req_order_query(doc, msg.get_msg_id());
		}
		else if (strTradeCode.compare(TS_FUNC_TRADE_QUERY_FOR_ID) == 0)// 成交id查询
		{
			handle_req_trade_query_for_id(doc, msg.get_msg_id());
		}
		else if (strTradeCode.compare(TS_FUNC_TRADE_QUERY) == 0)// 交易查询
		{
			handle_req_trade_query(doc, msg.get_msg_id());
		}
		else if (strTradeCode.compare(TS_FUNC_POSITION_QUERY) == 0)// 持仓查询
		{
			handle_req_position_query(doc, msg.get_msg_id());
		}
		else if (strTradeCode.compare(TS_FUNC_ASSET) == 0)// 资产查询
		{
			handle_req_asset(doc, msg.get_msg_id());
		}
	}
	else
	{
		rapidjson::ParseErrorCode eCode = doc.GetParseError();// 获取解析出错时的原因
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), debug) << "请求报文的格式错误，错误码：" << doc.GetParseError() << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::handle_req_insert_order(rapidjson::Document &doc, size_t msgId)
{
	rapidjson::Value &vHead = doc["head"];
	rapidjson::Value &vBody = doc["body"];

	std::string strTicker = vBody["ticker"].GetString();
	std::string strMarket = vBody["market"].GetString();
	std::string strPrice = vBody["price"].GetString();
	std::string strStopPrice = vBody["stop_price"].GetString();
	std::string strQuantity = vBody["quantity"].GetString();
	std::string strPriceType = vBody["price_type"].GetString();
	std::string strSide = vBody["side"].GetString();
	std::string strBusinessType = vBody["business_type"].GetString();

	strTicker = strTicker.substr(0, strTicker.length()-3);

	XTP_MARKET_TYPE market;
	boost::algorithm::to_lower(strMarket);
	if (strMarket == "sz")
	{
		market = XTP_MKT_SZ_A;
	}
	else if (strMarket == "sh")
	{
		market = XTP_MKT_SH_A;
	}

	XTPOrderInsertInfo order;
	strncpy(order.ticker, strTicker.c_str(), XTP_TICKER_LEN);
	order.order_client_id = 0;
	order.market = market;
	order.price = atof(strPrice.c_str());
	order.stop_price = atof(strStopPrice.c_str());
	order.quantity = CMyApp::stringToNum<int64_t>(strQuantity);
	order.price_type = (XTP_PRICE_TYPE)atoi(strPriceType.c_str());
	order.side = (XTP_SIDE_TYPE)atoi(strSide.c_str());
	order.business_type = (XTP_BUSINESS_TYPE)atoi(strBusinessType.c_str());

	if (theApp.is_login())
	{
		char szBuf[256] = {0};
		sprintf(szBuf, "ticker:%s, order_client_id:0, market:%d, price:%.4f, stop_price:%.4f, quantity:%lld, price_type:%d, side:%d, business_type:%d", 
			order.ticker, order.market, order.price, order.stop_price, order.quantity, order.price_type, order.side, order.business_type);
	
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), debug) << "The content of the order sent to XTP is:" << szBuf << "[" << __FILE__ << ":" << __LINE__ << "]";

		rapidjson::Document doc;
		doc.SetObject();

		rapidjson::Value vHead(rapidjson::kObjectType);
		rapidjson::Value vBody(rapidjson::kObjectType);
		rapidjson::Value vErrorInfo(rapidjson::kObjectType);

		std::string strPacketID((boost::format("%u") % msgId).str());

		uint64_t orderID=0;
		if (!theApp.m_xtpReq.doEntrustOrder(&order, orderID))
		{
			std::string strErrorMsg = theApp.get_last_error();
			COrderData::combo_head(vHead, doc, TS_FUNC_INSERT_ORDER, strPacketID, TS_RES_CODE_ENTRUST_FAILED, strErrorMsg);

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doEntrustOrder() call failed, cause:" << strErrorMsg << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
		else
		{
			COrderData::combo_head(vHead, doc, TS_FUNC_INSERT_ORDER, strPacketID, TS_RES_CODE_SUCCESS, "");

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doEntrustOrder() call success!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}

		const SESSION_PTR &session = theApp.m_server->get_session(msgId);

		if (session)
		{
			rapidjson::Document doc;
			doc.SetObject();
			rapidjson::Document::AllocatorType &alloctor = doc.GetAllocator();

			std::string order_xtp_id((boost::format("%u") % orderID).str());
			std::string order_client_id((boost::format("%u") %order.order_client_id).str());

			rapidjson::Value vtrd_seqno, vorder_xtp_id, vorder_client_id;
			rapidjson::Value vorder_cancel_client_id, vorder_cancel_xtp_id, vdate_time, vorder_local_id;

			vtrd_seqno.SetString(strPacketID.c_str(), alloctor);
			vorder_xtp_id.SetString(order_xtp_id.c_str(), alloctor);
			vorder_client_id.SetString(order_client_id.c_str(), alloctor);
			vorder_cancel_client_id.SetString("", alloctor);
			vorder_cancel_xtp_id.SetString("", alloctor);

			std::string strTime = boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time());
			boost::algorithm::replace_first(strTime, "T", "");
			vdate_time.SetString(strTime.c_str(), alloctor);

			vorder_local_id.SetString("", alloctor);

			vBody.AddMember("trd_seqno", vtrd_seqno, alloctor);
			vBody.AddMember("order_xtp_id", vorder_xtp_id, alloctor);
			vBody.AddMember("order_client_id", vorder_client_id, alloctor);
			vBody.AddMember("order_cancel_client_id", vorder_cancel_client_id, alloctor);
			vBody.AddMember("order_cancel_xtp_id", vorder_cancel_xtp_id, alloctor);
			vBody.AddMember("date_time", vdate_time, alloctor);
			vBody.AddMember("order_local_id", vorder_local_id, alloctor);
			vBody.AddMember("error_id", "", alloctor);
			vBody.AddMember("error_msg", "", alloctor);

			doc.AddMember("head", vHead, alloctor);
			doc.AddMember("body", vBody, alloctor);

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);

			message msg;
			msg.fill_msg(buffer.GetString(), buffer.GetLength());
			session->send(msg);
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The entrust of the session object not found!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The entrust failed, cause:No connection to the XTP trade server or Connection disconnection!" << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::handle_req_cancel_order(rapidjson::Document &doc, size_t msgId)
{
	std::string strErrorMsg;
	std::string strResCode;

	rapidjson::Value &vHead = doc["head"];
	std::string strPacketID = vHead["s_trdid"].GetString();

	rapidjson::Value &vBody = doc["body"];
	std::string strEntrustID = vBody["order_xtp_id"].GetString();
	uint64_t entrustID = CMyApp::stringToNum<uint64_t>(strEntrustID);

	if (theApp.is_login())
	{
		const SESSION_PTR &session = theApp.m_server->get_session(msgId);

		rapidjson::Document doc;
		doc.SetObject();
		rapidjson::Document::AllocatorType &alloctor = doc.GetAllocator();

		rapidjson::Value vHeadRes(rapidjson::kObjectType), vBodyRes(rapidjson::kObjectType);
		rapidjson::Value vorder_xtp_id;
		rapidjson::Value vorder_client_id;
		rapidjson::Value vorder_cancel_client_id;
		rapidjson::Value vorder_cancel_xtp_id;
		rapidjson::Value vupdate_time;
		rapidjson::Value vorder_local_id;

		uint64_t cancelOrderID = theApp.m_xtpReq.doCancelEntrust(entrustID);
		if (!cancelOrderID)
		{
			strResCode = TS_RES_CODE_ENTRUST_FAILED;
			strErrorMsg = theApp.get_last_error();

			vorder_xtp_id.SetString("", alloctor);
			vorder_client_id.SetString("", alloctor);
			vorder_cancel_client_id.SetString("", alloctor);
			vorder_cancel_xtp_id.SetString("", alloctor);
			vupdate_time.SetString("", alloctor);
			vorder_local_id.SetString("", alloctor);

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doCancelEntrust() call failed, cause:" << strErrorMsg << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doCancelEntrust() call success!" << "[" << __FILE__ << ":" << __LINE__ << "]";

			strResCode = TS_RES_CODE_SUCCESS;

			std::string order_xtp_id((boost::format("%u") %entrustID).str());
			vorder_xtp_id.SetString(order_xtp_id.c_str(), alloctor);

			std::string order_client_id((boost::format("%d") %theApp.m_config.get_client_id()).str());
			vorder_client_id.SetString(order_client_id.c_str(), alloctor);

			vorder_cancel_client_id.SetString("", alloctor);

			std::string order_cancel_xtp_id((boost::format("%u") %cancelOrderID).str());
			vorder_cancel_xtp_id.SetString(order_cancel_xtp_id.c_str(), alloctor);

			vorder_local_id.SetString("", alloctor);
		}

		std::string strTime = boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time());
		boost::algorithm::replace_first(strTime, "T", "");

		vBodyRes.AddMember("order_xtp_id", vorder_xtp_id, alloctor);
		vBodyRes.AddMember("order_client_id", vorder_client_id, alloctor);
		vBodyRes.AddMember("order_cancel_client_id", vorder_cancel_client_id, alloctor);
		vBodyRes.AddMember("order_cancel_xtp_id", vorder_cancel_xtp_id, alloctor);

		vupdate_time.SetString(strTime.c_str(), alloctor);
		vBodyRes.AddMember("update_time", vupdate_time, alloctor);

		vBodyRes.AddMember("order_local_id", vorder_local_id, alloctor);
		vBodyRes.AddMember("error_id", "", alloctor);
		vBodyRes.AddMember("error_msg", "", alloctor);

		COrderData::combo_head(vHeadRes, doc, TS_FUNC_CANCEL_ORDER, strPacketID, strResCode, strErrorMsg);

		doc.AddMember("head", vHeadRes, doc.GetAllocator());
		doc.AddMember("body", vBodyRes, doc.GetAllocator());

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);

		if (session)
		{
			message msg;
			msg.fill_msg(buffer.GetString(), buffer.GetLength());
			session->send(msg);
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The cancellation of the session object not found!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The cancel the order failed, cause:No connection to the XTP trade server or Connection disconnection!" << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::handle_req_order_query_for_id(rapidjson::Document &doc, size_t msgId)
{
	rapidjson::Value &vHead = doc["head"];
	std::string strPacketID = vHead["s_trdid"].GetString();

	rapidjson::Value &vBody = doc["body"];

	std::string strEntrustID = vBody["order_xtp_id"].GetString();
	uint64_t entrustID = CMyApp::stringToNum<uint64_t>(strEntrustID);

	m_strEntrustQueryFunc = TS_FUNC_ORDER_QUERY_FOR_ID;

	if (theApp.is_login())
	{
		if (!theApp.m_xtpReq.doQueryEntrustForID(entrustID, msgId))
		{
			const SESSION_PTR &session = theApp.m_server->get_session(msgId);

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryEntrustForID() call failed!";

			rapidjson::Document doc;
			doc.SetObject();
			
			rapidjson::Value vHeadRes(rapidjson::kObjectType), vBodyRes(rapidjson::kObjectType);

			std::string strErrorMsg = theApp.get_last_error();
			COrderData::combo_head(vHeadRes, doc, TS_FUNC_ORDER_QUERY_FOR_ID, strPacketID, TS_RES_CODE_ENTRUST_FAILED, strErrorMsg);

			doc.AddMember("head", vHeadRes, doc.GetAllocator());
			doc.AddMember("body", vBodyRes, doc.GetAllocator());

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);

			if (session)
			{
				message msg;
				msg.fill_msg(buffer.GetString(), buffer.GetLength());
				session->send(msg);
			}
			else
			{
				BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The id order query session object has not been found!" << "[" << __FILE__ << ":" << __LINE__ << "]";
			}
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryEntrustForID() call success!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The entrust id query failed, cause:No connection to the XTP trade server or Connection disconnection!" << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::handle_req_order_query(rapidjson::Document &doc, size_t msgId)
{
	rapidjson::Value &vHead = doc["head"];
	std::string strPacketID = vHead["s_trdid"].GetString();

	rapidjson::Value &vBody = doc["body"];
	rapidjson::Value &vQueryParam = vBody["query_param"];

	std::string strTicker = vQueryParam["ticker"].GetString();
	std::string strBeginTime = vQueryParam["begin_time"].GetString();
	std::string strEndTime = vQueryParam["end_time"].GetString();
	int64_t beginTime = CMyApp::stringToNum<int64_t>(strBeginTime);
	int64_t endTime = CMyApp::stringToNum<int64_t>(strEndTime);

	XTPQueryOrderReq queryParam;
	//strncpy(queryParam.ticker, strTicker.c_str(), XTP_TICKER_LEN);
	//queryParam.begin_time = beginTime;
	//queryParam.end_time = endTime;
	memset(&queryParam, 0, sizeof(XTPQueryOrderReq));

	m_strEntrustQueryFunc = TS_FUNC_ORDER_QUERY;

	if (theApp.is_login())
	{
		if (!theApp.m_xtpReq.doQueryEntrust(&queryParam, msgId))
		{
			const SESSION_PTR &session = theApp.m_server->get_session(msgId);

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryEntrust() call failed!";

			rapidjson::Document doc;
			doc.SetObject();

			rapidjson::Value vHeadRes(rapidjson::kObjectType), vBodyRes(rapidjson::kObjectType);

			std::string strErrorMsg = theApp.get_last_error();
			COrderData::combo_head(vHead, doc, TS_FUNC_ORDER_QUERY, strPacketID, TS_RES_CODE_ENTRUST_FAILED, strErrorMsg);

			doc.AddMember("head", vHeadRes, doc.GetAllocator());
			doc.AddMember("body", vBodyRes, doc.GetAllocator());

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);

			if (session)
			{
				message msg;
				msg.fill_msg(buffer.GetString(), buffer.GetLength());
				session->send(msg);
			}
			else
			{
				BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The order query session object has not been found!" << "[" << __FILE__ << ":" << __LINE__ << "]";
			}
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryEntrust() call succeed!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The entrust query failed, cause:No connection to the XTP trade server or Connection disconnection!" << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::handle_req_trade_query_for_id(rapidjson::Document &doc, size_t msgId)
{
	rapidjson::Value &vHead = doc["head"];
	std::string strPacketID = vHead["s_trdid"].GetString();

	rapidjson::Value &vBody = doc["body"];
	std::string strTradeID = vBody["order_xtp_id"].GetString();
	uint64_t tradeID = CMyApp::stringToNum<uint64_t>(strTradeID);

	m_strEntrustQueryFunc = TS_FUNC_TRADE_QUERY_FOR_ID;

	if (theApp.is_login())
	{
		const SESSION_PTR &session = theApp.m_server->get_session(msgId);

		if (!theApp.m_xtpReq.doQueryTradeForID(tradeID, msgId))
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryTradeForID() call failed!";

			rapidjson::Document doc;
			doc.SetObject();

			rapidjson::Value vHeadRes(rapidjson::kObjectType), vBodyRes(rapidjson::kObjectType);

			std::string strErrorMsg = theApp.get_last_error();
			COrderData::combo_head(vHead, doc, TS_FUNC_TRADE_QUERY_FOR_ID, strPacketID, TS_RES_CODE_ENTRUST_FAILED, strErrorMsg);

			doc.AddMember("head", vHeadRes, doc.GetAllocator());
			doc.AddMember("body", vBodyRes, doc.GetAllocator());

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);

			if (session)
			{
				message msg;
				msg.fill_msg(buffer.GetString(), buffer.GetLength());
				session->send(msg);
			}
			else
			{
				BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The id trade query session object has not been found!" << "[" << __FILE__ << ":" << __LINE__ << "]";
			}
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryTradeForID() call succeed!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The trade id query failed, cause:No connection to the XTP trade server or Connection disconnection!" << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::handle_req_trade_query(rapidjson::Document &doc, size_t msgId)
{
	rapidjson::Value &vHead = doc["head"];
	std::string strPacketID = vHead["s_trdid"].GetString();

	rapidjson::Value &vBody = doc["body"];
	rapidjson::Value &vQueryParam = vBody["query_param"];

	std::string strTicker = vQueryParam["ticker"].GetString();
	std::string strBeginTime = vQueryParam["begin_time"].GetString();
	std::string strEndTime = vQueryParam["end_time"].GetString();
	//int64_t beginTime = boost::lexical_cast<int64_t>(strBeginTime);
	//int64_t endTime = boost::lexical_cast<int64_t>(strEndTime);

	XTPQueryTraderReq queryParam;
	//strncpy(queryParam.ticker, strTicker.c_str(), XTP_TICKER_LEN);
	//queryParam.begin_time = beginTime;
	//queryParam.end_time = endTime;
	memset(&queryParam, 0, sizeof(XTPQueryTraderReq));

	m_strEntrustQueryFunc = TS_FUNC_TRADE_QUERY;

	if (theApp.is_login())
	{
		if (!theApp.m_xtpReq.doQueryTrade(&queryParam, msgId))
		{
			const SESSION_PTR &session = theApp.m_server->get_session(msgId);

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryTrade() call failed!";

			rapidjson::Document doc;
			doc.SetObject();

			rapidjson::Value vHeadRes(rapidjson::kObjectType), vBodyRes(rapidjson::kObjectType);

			std::string strErrorMsg = theApp.get_last_error();
			COrderData::combo_head(vHead, doc, TS_FUNC_TRADE_QUERY, strPacketID, TS_RES_CODE_ENTRUST_FAILED, strErrorMsg);

			doc.AddMember("head", vHeadRes, doc.GetAllocator());
			doc.AddMember("body", vBodyRes, doc.GetAllocator());

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);

			if (session)
			{
				message msg;
				msg.fill_msg(buffer.GetString(), buffer.GetLength());
				session->send(msg);
			}
			else
			{
				BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The trade query session object has not been found!" << "[" << __FILE__ << ":" << __LINE__ << "]";
			}
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryTrade() call succeed!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The trade query failed, cause:No connection to the XTP trade server or Connection disconnection!" << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::handle_req_position_query(rapidjson::Document &doc, size_t msgId)
{
	rapidjson::Value &vHead = doc["head"];
	std::string strPacketID = vHead["s_trdid"].GetString();

	rapidjson::Value &vBody = doc["body"];
	std::string strTicker = vBody["ticker"].GetString();

	if (theApp.is_login())
	{
		if (!theApp.m_xtpReq.doQueryHold(strTicker.c_str(), msgId))
		{
			const SESSION_PTR &session = theApp.m_server->get_session(msgId);

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryHold() call failed!";

			rapidjson::Document doc;
			doc.SetObject();

			rapidjson::Value vHeadRes(rapidjson::kObjectType), vBodyRes(rapidjson::kObjectType);

			std::string strErrorMsg = theApp.get_last_error();
			COrderData::combo_head(vHead, doc, TS_FUNC_POSITION_QUERY, strPacketID, TS_RES_CODE_ENTRUST_FAILED, strErrorMsg);

			doc.AddMember("head", vHeadRes, doc.GetAllocator());
			doc.AddMember("body", vBodyRes, doc.GetAllocator());

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);

			if (session)
			{
				message msg;
				msg.fill_msg(buffer.GetString(), buffer.GetLength());
				session->send(msg);
			}
			else
			{
				BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The position query session object has not been found!" << "[" << __FILE__ << ":" << __LINE__ << "]";
			}
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryHold() call succeed!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The position query failed, cause:No connection to the XTP trade server or Connection disconnection!" << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::handle_req_asset(rapidjson::Document &doc, size_t msgId)
{
	rapidjson::Value &vHead = doc["head"];
	std::string strPacketID = vHead["s_trdid"].GetString();

	if (theApp.is_login())
	{
		if (!theApp.m_xtpReq.doQueryAssert(msgId))
		{
			const SESSION_PTR &session = theApp.m_server->get_session(msgId);

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryAssert() call failed!";

			rapidjson::Document doc;
			doc.SetObject();

			rapidjson::Value vHeadRes(rapidjson::kObjectType), vBodyRes(rapidjson::kObjectType);

			std::string strErrorMsg = theApp.get_last_error();
			COrderData::combo_head(vHead, doc, TS_FUNC_ASSET, strPacketID, TS_RES_CODE_ENTRUST_FAILED, strErrorMsg);

			doc.AddMember("head", vHeadRes, doc.GetAllocator());
			doc.AddMember("body", vBodyRes, doc.GetAllocator());

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);

			if (session)
			{
				message msg;
				msg.fill_msg(buffer.GetString(), buffer.GetLength());
				session->send(msg);
			}
			else
			{
				BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The asset query session object has not been found!" << "[" << __FILE__ << ":" << __LINE__ << "]";
			}
		}
		else
		{
			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "doQueryAssert() call succeed!" << "[" << __FILE__ << ":" << __LINE__ << "]";
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "The asset query failed, cause:No connection to the XTP trade server or Connection disconnection!" << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

void COrderData::combo_head(rapidjson::Value & vHead, rapidjson::Document & doc, std::string strTradeCode, std::string strTrdid, std::string strResCode, std::string strMsg)
{
	rapidjson::Value vTrdid, vTrdcode, vTrdDatetime, vTrdRescode, vTrdDesc;

	std::string strTime = boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time());
	boost::algorithm::replace_first(strTime, "T", "");

	vTrdid.SetString(strTrdid.c_str(), strTrdid.length(), doc.GetAllocator());
	vTrdcode.SetString(strTradeCode.c_str(), strTradeCode.length(), doc.GetAllocator());
	vTrdDatetime.SetString(strTime.c_str(), strTime.length(), doc.GetAllocator());
	vTrdRescode.SetString(strResCode.c_str(), strResCode.length(), doc.GetAllocator());
	vTrdDesc.SetString(strMsg.c_str(), strMsg.length(), doc.GetAllocator());

	vHead.AddMember("s_trdid", vTrdid, doc.GetAllocator());
	vHead.AddMember("s_trdcode", vTrdcode, doc.GetAllocator());
	vHead.AddMember("s_trddatetime", vTrdDatetime, doc.GetAllocator());
	vHead.AddMember("s_respcode", vTrdRescode, doc.GetAllocator());
	vHead.AddMember("s_respdesc", vTrdDesc, doc.GetAllocator());
}

void COrderData::start_thread()
{
	int nThreadCount = theApp.m_config.get_thread_count();
	for (int i = 0; i < nThreadCount; ++i)
	{
		boost::thread thRes(boost::bind(&COrderData::thread_handler_res, this));
		boost::thread thReq(boost::bind(&COrderData::thread_handler_req, this));
	}

	printf("Start listening to the reply message from the XTP server.\n");
	printf("Start listening to the request message from the trading system.\n");
	run();
}

