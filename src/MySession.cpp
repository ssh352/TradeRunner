#include <map>
#include "boost/thread.hpp"
#include "boost/filesystem.hpp"
#include "boost/locale.hpp"
#include "boost/format.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "MySession.h"
#include "MyServer.h"
#include "DataType.h"
#include "MyApp.h"



CMySession::CMySession(boost::asio::io_service& io_service, CMyServer &server)
	: m_socket(io_service),
	m_io_service(io_service),
	m_server(server)
{
	m_recv_buf.realloc(1024);

	printf("CMySession::CMySession() called\n");
}

CMySession::~CMySession()
{
	printf("CMySession::~CMySession() called\n");
}

boost::asio::ip::tcp::socket& CMySession::get_socket()
{
	return m_socket;
}

void CMySession::start()
{
	std::string strAddr = m_socket.remote_endpoint().address().to_string();
	unsigned short uPort = m_socket.remote_endpoint().port();
	strncpy(m_tRemoteAddr.szIP, strAddr.c_str(), strAddr.length());
	m_tRemoteAddr.nPort = uPort;

	// 将当前会话实例添加到队列中
	m_server.join(shared_from_this());

	boost::asio::async_read(m_socket,
		boost::asio::buffer(m_recv_buf.data(), message::header_length),
		boost::bind(&CMySession::handle_read_header, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void CMySession::handle_read_header(const boost::system::error_code& error, std::size_t read_bytes)
{
	if (!error)
	{
		m_recv_buf.fill_end(read_bytes);
		int nSize = atoi(m_recv_buf.data());
		m_recv_buf.set_body_length(nSize);

		// 如果收到的数据大小比现有的缓存大小还要大，那么重新分配内存
		if (nSize >= m_recv_buf.buf_length())
		{
			m_recv_buf.realloc(nSize);
		}

		boost::asio::async_read(m_socket,
			boost::asio::buffer(m_recv_buf.body(), nSize),
			boost::bind(&CMySession::handle_read_body, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		m_server.leave(shared_from_this());
	}
}

void CMySession::handle_read_body(const boost::system::error_code& error, std::size_t read_bytes)
{
	if (!error)
	{
		
		m_recv_buf.fill_end(read_bytes);

		int nPacketId = theApp.generate_msg_id();

		m_setMsgID.insert(nPacketId);
		
		m_recv_buf.set_msg_id(nPacketId);

		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), debug) << "(req) [" << this << "]" << m_recv_buf.data() << "[" << __FILE__ << ":" << __LINE__ << "]";

		theApp.m_orderDta.push_packet_req(m_recv_buf);

		boost::asio::async_read(m_socket,
			boost::asio::buffer(m_recv_buf.data(), message::header_length),
			boost::bind(&CMySession::handle_read_header, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		m_server.leave(shared_from_this());
	}
}

void CMySession::handle_write(const boost::system::error_code& error, std::size_t bytes_transferred, std::size_t expected_size, std::size_t offset)
{
	if (!error)
	{
		size_t resend_size = expected_size - bytes_transferred;
		if (resend_size > 0)
		{
			message & msg_send = m_deque_res.front();

			size_t new_offset = offset + bytes_transferred;

			boost::asio::async_write(m_socket,
				boost::asio::buffer(msg_send.data(resend_size), msg_send.length()),
				boost::bind(&CMySession::handle_write,
					shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred,
					resend_size, new_offset));
		}
		else
		{
			message & msg_send = m_deque_res.front();

			BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), debug) << "(res) [" << this << "] send response success, len:" << bytes_transferred << ", content is:" << msg_send.data() << "[" << __FILE__ << ":" << __LINE__ << "]";

			m_deque_res.pop_front();

			if (!m_deque_res.empty())
			{
				message & msg_send = m_deque_res.front();

				boost::asio::async_write(m_socket,
					boost::asio::buffer(msg_send.data(), msg_send.length()),
					boost::bind(&CMySession::handle_write,
						shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred,
						msg_send.length(), 0));
			}
		}
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "(res) send response failed" << "[" << __FILE__ << ":" << __LINE__ << "]";
		
		m_server.leave(shared_from_this());
	}
}

void CMySession::send(message &msg)
{
	try
	{
		bool write_in_progress = !m_deque_res.empty();
		m_deque_res.push_back(msg);
		if (!write_in_progress)
		{
			message & msg_send = m_deque_res.front();

			boost::asio::async_write(m_socket,
				boost::asio::buffer(msg_send.data(), msg_send.length()),
				boost::bind(&CMySession::handle_write, 
					         shared_from_this(), 
					         boost::asio::placeholders::error, 
					         boost::asio::placeholders::bytes_transferred,
							 msg_send.length(), 0));
		}
	}
	catch (std::exception &e)
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "(res) Failure of sync message send, cause:" << e.what() << "[" << __FILE__ << ":" << __LINE__ << "]";
		
		m_server.leave(shared_from_this());
	}
}


std::string CMySession::get_trdcode()
{
	return m_strTrdCode;
}

void CMySession::combo_order_query_packet(T_Packet & tPacket, size_t msgID, std::string & strPacket)
{
	rapidjson::Document doc;
	rapidjson::Document::AllocatorType &allocator = doc.GetAllocator();
	doc.SetObject();

	rapidjson::Value vHead(rapidjson::kObjectType);
	rapidjson::Value vBodyArray(rapidjson::kArrayType);

	std::string strResCode, strResMsg;

	if (0 == tPacket.nStatus)
	{
		XTPRI * xtpRI = (XTPRI*)tPacket.data;
		strResCode = TS_RES_CODE_QUERY_ORDER_FAILED;
		strResMsg = xtpRI->error_msg;
	}
	else
	{
		strResCode = TS_RES_CODE_SUCCESS;
		strResMsg = "";
	}
		
	if (strResCode == TS_RES_CODE_SUCCESS)
	{
		for (int i = 0; i < tPacket.nCount; ++i)
		{
			XTPQueryOrderRsp & order_info = *((XTPQueryOrderRsp*)tPacket.data + i);
			rapidjson::Value vBody(rapidjson::kObjectType);

			std::string order_xtp_id((boost::format("%u") % order_info.order_xtp_id).str());
			rapidjson::Value vOrder_xtp_id;
			vOrder_xtp_id.SetString(order_xtp_id.c_str(), allocator);
			vBody.AddMember("order_xtp_id", vOrder_xtp_id, allocator);

			std::string order_client_id((boost::format("%u") % order_info.order_client_id).str());
			rapidjson::Value vPrder_client_id;
			vPrder_client_id.SetString(order_client_id.c_str(), allocator);
			vBody.AddMember("order_client_id", vPrder_client_id, doc.GetAllocator());

			std::string order_cancel_client_id((boost::format("%u") % order_info.order_cancel_client_id).str());
			rapidjson::Value vOrder_cancel_client_id;
			vOrder_cancel_client_id.SetString(order_cancel_client_id.c_str(), allocator);
			vBody.AddMember("order_cancel_client_id", vOrder_cancel_client_id, doc.GetAllocator());

			std::string order_cancel_xtp_id((boost::format("%u") % order_info.order_cancel_xtp_id).str());
			rapidjson::Value vOrder_cancel_xtp_id;
			vOrder_cancel_xtp_id.SetString(order_cancel_xtp_id.c_str(), allocator);
			vBody.AddMember("order_cancel_xtp_id", vOrder_cancel_xtp_id, doc.GetAllocator());

			rapidjson::Value vTicker;
			vTicker.SetString(order_info.ticker, allocator);
			vBody.AddMember("ticker", vTicker, doc.GetAllocator());

			std::string market((boost::format("%d") % order_info.market).str());
			rapidjson::Value vMarket;
			vMarket.SetString(market.c_str(), allocator);
			vBody.AddMember("market", vMarket, doc.GetAllocator());

			std::string price((boost::format("%.4f") % order_info.price).str());
			rapidjson::Value vPrice;
			vPrice.SetString(price.c_str(), allocator);
			vBody.AddMember("price", vPrice, doc.GetAllocator());

			std::string quantity((boost::format("%lld") % order_info.quantity).str());
			rapidjson::Value vQuantity;
			vQuantity.SetString(quantity.c_str(), allocator);
			vBody.AddMember("quantity", vQuantity, doc.GetAllocator());

			std::string price_type((boost::format("%d") % order_info.price_type).str());
			rapidjson::Value vPrice_type;
			vPrice_type.SetString(price_type.c_str(), allocator);
			vBody.AddMember("price_type", vPrice_type, doc.GetAllocator());

			std::string side((boost::format("%d") % order_info.side).str());
			rapidjson::Value vSide;
			vSide.SetString(side.c_str(), allocator);
			vBody.AddMember("side", vSide, doc.GetAllocator());

			std::string business_type((boost::format("%d") % order_info.business_type).str());
			rapidjson::Value vBusiness_type;
			vBusiness_type.SetString(business_type.c_str(), allocator);
			vBody.AddMember("business_type", vBusiness_type, doc.GetAllocator());

			std::string qty_traded((boost::format("%lld") % order_info.qty_traded).str());
			rapidjson::Value vQty_traded;
			vQty_traded.SetString(qty_traded.c_str(), allocator);
			vBody.AddMember("qty_traded", vQty_traded, doc.GetAllocator());

			std::string qty_left((boost::format("%lld") % order_info.qty_left).str());
			rapidjson::Value vQty_left;
			vQty_left.SetString(qty_left.c_str(), allocator);
			vBody.AddMember("qty_left", vQty_left, doc.GetAllocator());

			std::string insert_time((boost::format("%lld") % order_info.insert_time).str());
			rapidjson::Value vInsert_time;
			vInsert_time.SetString(insert_time.c_str(), allocator);
			vBody.AddMember("insert_time", vInsert_time, doc.GetAllocator());

			std::string update_time((boost::format("%lld") % order_info.update_time).str());
			rapidjson::Value vUpdate_time;
			vUpdate_time.SetString(update_time.c_str(), allocator);
			vBody.AddMember("update_time", vUpdate_time, doc.GetAllocator());

			std::string cancel_time((boost::format("%lld") % order_info.cancel_time).str());
			rapidjson::Value vCancel_time;
			vCancel_time.SetString(cancel_time.c_str(), allocator);
			vBody.AddMember("cancel_time", vCancel_time, doc.GetAllocator());

			std::string trade_amount((boost::format("%.4f") % order_info.trade_amount).str());
			rapidjson::Value vTrade_amount;
			vTrade_amount.SetString(trade_amount.c_str(), allocator);
			vBody.AddMember("trade_amount", vTrade_amount, doc.GetAllocator());

			rapidjson::Value vOrder_local_id;
			vOrder_local_id.SetString(order_info.order_local_id, allocator);
			vBody.AddMember("order_local_id", vOrder_local_id, doc.GetAllocator());

			std::string order_status((boost::format("%d") % order_info.order_status).str());
			rapidjson::Value vOrder_status;
			vOrder_status.SetString(order_status.c_str(), allocator);
			vBody.AddMember("order_status", vOrder_status, doc.GetAllocator());

			std::string order_submit_status((boost::format("%d") % order_info.order_submit_status).str());
			rapidjson::Value vOrder_submit_status;
			vOrder_submit_status.SetString(order_submit_status.c_str(), allocator);
			vBody.AddMember("order_submit_status", vOrder_submit_status, doc.GetAllocator());

			std::string order_type((boost::format("%d") % order_info.order_type).str());
			rapidjson::Value vOrder_type;
			vOrder_type.SetString(order_type.c_str(), allocator);
			vBody.AddMember("order_type", vOrder_type, doc.GetAllocator());

			vBodyArray.PushBack(vBody, allocator);
		}
	}
	

	std::string strMsgID((boost::format("%u") % msgID).str());
	COrderData::combo_head(vHead, doc, TS_FUNC_ORDER_QUERY, strMsgID.c_str(), strResCode, strResMsg);
	doc.AddMember("head", vHead, allocator);
	doc.AddMember("body", vBodyArray, allocator);
	
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	strPacket.clear();
	strPacket += buffer.GetString();

	delete[] tPacket.data;
}

void CMySession::combo_trade_query_packet(T_Packet & tPacket, size_t msgID, std::string & strPacket)
{
	rapidjson::Document doc;
	doc.SetObject();
	rapidjson::Document::AllocatorType &allocator = doc.GetAllocator();
	std::string strResCode, strDesc;

	rapidjson::Value vHead(rapidjson::kObjectType);
	rapidjson::Value vBodyArray(rapidjson::kArrayType);

	if (0 == tPacket.nStatus)
	{
		XTPRI * xtpRI = (XTPRI*)tPacket.data;
		strResCode = TS_RES_CODE_QUERY_TRADE_FAILED;
		strDesc = xtpRI->error_msg;
	}
	else
	{
		strResCode = TS_RES_CODE_SUCCESS;
		strDesc = "";

		for (int i = 0; i < tPacket.nCount; ++i)
		{
			XTPQueryTradeRsp * trade_info = (XTPQueryTradeRsp*)tPacket.data + i;

			rapidjson::Value vBody(rapidjson::kObjectType);

			std::string order_xtp_id((boost::format("%u") % trade_info->order_xtp_id).str());
			rapidjson::Value vOrder_xtp_id;
			vOrder_xtp_id.SetString(order_xtp_id.c_str(), allocator);
			vBody.AddMember("order_xtp_id", vOrder_xtp_id, allocator);

			std::string order_client_id((boost::format("%u") % trade_info->order_client_id).str());
			rapidjson::Value vorder_client_id;
			vorder_client_id.SetString(order_client_id.c_str(), allocator);
			vBody.AddMember("order_client_id", vorder_client_id, doc.GetAllocator());

			rapidjson::Value vticker;
			vticker.SetString(trade_info->ticker, allocator);
			vBody.AddMember("ticker", vticker, doc.GetAllocator());

			std::string market(((boost::format("%d") % trade_info->market).str()));
			rapidjson::Value vmarket;
			vmarket.SetString(market.c_str(), allocator);
			vBody.AddMember("market", vmarket, doc.GetAllocator());

			std::string local_order_id(((boost::format("%u") % trade_info->local_order_id).str()));
			rapidjson::Value vlocal_order_id;
			vlocal_order_id.SetString(local_order_id.c_str(), allocator);
			vBody.AddMember("local_order_id", vlocal_order_id, doc.GetAllocator());

			std::string exec_id(((boost::format("%s") % trade_info->exec_id).str()));
			rapidjson::Value vexec_id;
			vexec_id.SetString(exec_id.c_str(), allocator);
			vBody.AddMember("exec_id", vexec_id, doc.GetAllocator());

			std::string price((boost::format("%.4f") % trade_info->price).str());
			rapidjson::Value vprice;
			vprice.SetString(price.c_str(), allocator);
			vBody.AddMember("price", vprice, doc.GetAllocator());

			std::string quantity((boost::format("%lld") % trade_info->quantity).str());
			rapidjson::Value vquantity;
			vquantity.SetString(quantity.c_str(), allocator);
			vBody.AddMember("quantity", vquantity, doc.GetAllocator());

			std::string trade_time((boost::format("%lld") % trade_info->trade_time).str());
			rapidjson::Value vtrade_time;
			vtrade_time.SetString(trade_time.c_str(), allocator);
			vBody.AddMember("trade_time", vtrade_time, doc.GetAllocator());

			std::string trade_amount((boost::format("%.0f") % trade_info->trade_amount).str());
			rapidjson::Value vtrade_amount;
			vtrade_amount.SetString(trade_amount.c_str(), allocator);
			vBody.AddMember("trade_amount", vtrade_amount, doc.GetAllocator());

			std::string report_index((boost::format("%u") % trade_info->report_index).str());
			rapidjson::Value vreport_index;
			vreport_index.SetString(report_index.c_str(), allocator);
			vBody.AddMember("report_index", vreport_index, doc.GetAllocator());

			std::string order_exch_id((boost::format("%s") % trade_info->order_exch_id).str());
			rapidjson::Value vorder_exch_id_index;
			vorder_exch_id_index.SetString(order_exch_id.c_str(), allocator);
			vBody.AddMember("order_exch_id", vorder_exch_id_index, doc.GetAllocator());

			std::string trade_type((boost::format("%d") % trade_info->trade_type).str());
			rapidjson::Value vtrade_type;
			vtrade_type.SetString(trade_type.c_str(), allocator);
			vBody.AddMember("trade_type", vtrade_type, doc.GetAllocator());

			std::string side((boost::format("%d") % trade_info->side).str());
			rapidjson::Value vside;
			vside.SetString(side.c_str(), allocator);
			vBody.AddMember("side", vside, doc.GetAllocator());

			std::string business_type((boost::format("%d") % trade_info->business_type).str());
			rapidjson::Value vbusiness_type;
			vbusiness_type.SetString(business_type.c_str(), allocator);
			vBody.AddMember("business_type", vbusiness_type, doc.GetAllocator());

			std::string branch_pbu((boost::format("%s") % trade_info->branch_pbu).str());
			rapidjson::Value vbranch_pbu;
			vbranch_pbu.SetString(branch_pbu.c_str(), allocator);
			vBody.AddMember("branch_pbu", vbranch_pbu, doc.GetAllocator());

			vBodyArray.PushBack(vBody, doc.GetAllocator());
		}
	}

	COrderData::combo_head(vHead, doc, TS_FUNC_TRADE_QUERY, "", strResCode, strDesc);

	doc.AddMember("head", vHead, doc.GetAllocator());
	doc.AddMember("body", vBodyArray, doc.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	strPacket.clear();
	strPacket = buffer.GetString();

	delete[]tPacket.data;
	
}

void CMySession::combo_position_packet(T_Packet & tPacket, size_t msgID, std::string & strPacket)
{
	rapidjson::Document doc;
	doc.SetObject();
	rapidjson::Document::AllocatorType &allocator = doc.GetAllocator();

	rapidjson::Value vHead(rapidjson::kObjectType);
	rapidjson::Value vBody(rapidjson::kObjectType);

	std::string strResCode, strDesc;

	if (0 == tPacket.nStatus)
	{
		XTPRI * xtpRI = (XTPRI*)tPacket.data;
		strDesc = xtpRI->error_msg;
		strResCode = TS_RES_CODE_QUERY_HOLD_FAILED;
	}
	else
	{
		strResCode = TS_RES_CODE_SUCCESS;
		strDesc = "";
	}

	if (strResCode == TS_RES_CODE_SUCCESS)
	{
		rapidjson::Value vOprdcode, vAccount, vDetail_num;

		std::string detail_num((boost::format("%u") % tPacket.nCount).str());

		vOprdcode.SetString("", allocator);
		vAccount.SetString("", allocator);
		vDetail_num.SetString(detail_num.c_str(), allocator);

		vBody.AddMember("oprdcode", vOprdcode, allocator);

		T_XtpTradeInfo & xtpTradeInfo = theApp.m_config.getXtpTradeInfo(0);
		vAccount.SetString(xtpTradeInfo.szUserName, allocator);
		vBody.AddMember("account", vAccount, allocator);

		std::string strNum((boost::format("%d") % tPacket.nCount).str());
		vDetail_num.SetString(strNum.c_str(), allocator),
		vBody.AddMember("detail_num", vDetail_num, allocator);

		rapidjson::Value vDetailArray(rapidjson::kArrayType);

		for (int i = 0; i < tPacket.nCount; ++i)
		{
			XTPQueryStkPositionRsp * investor_position = (XTPQueryStkPositionRsp*)tPacket.data + i;

			rapidjson::Value vDetail(rapidjson::kObjectType);

			// 证券代码
			rapidjson::Value vticker;
			vticker.SetString(investor_position->ticker, allocator);
			vDetail.AddMember("ticker", vticker, allocator);

			// 证券名称
			rapidjson::Value vtickerName;
			std::string strGBK = boost::locale::conv::between(investor_position->ticker_name, "GBK", "UTF-8");
			vtickerName.SetString(strGBK.c_str(), allocator);
			vDetail.AddMember("ticker_name", vtickerName, allocator);

			// 交易市场
			std::string order_xtp_id((boost::format("%d") % investor_position->market).str());
			rapidjson::Value vmarket;
			vmarket.SetString(order_xtp_id.c_str(), allocator);
			vDetail.AddMember("market", vmarket, allocator);

			// 当前持仓
			std::string total_qty((boost::format("%lld") % investor_position->total_qty).str());
			rapidjson::Value vtotal_qty;
			vtotal_qty.SetString(total_qty.c_str(), allocator);
			vDetail.AddMember("total_qty", vtotal_qty, allocator);

			// 可用股份数
			std::string sellable_qty((boost::format("%lld") % investor_position->sellable_qty).str());
			rapidjson::Value vsellable_qty;
			vsellable_qty.SetString(sellable_qty.c_str(), allocator);
			vDetail.AddMember("sellable_qty", vsellable_qty, allocator);

			// 买入成本价
			std::string avg_price((boost::format("%.4f") % investor_position->avg_price).str());
			rapidjson::Value vavg_price;
			vavg_price.SetString(avg_price.c_str(), allocator);
			vDetail.AddMember("avg_price", vavg_price, allocator);

			// 浮动盈亏（保留字段）
			rapidjson::Value vunrealized_pnl;
			std::string unrealized_pnl((boost::format("%.4f") % investor_position->unrealized_pnl).str());
			vunrealized_pnl.SetString(unrealized_pnl.c_str(), allocator);
			vDetail.AddMember("unrealized_pnl", vunrealized_pnl, allocator);

			//sprintf(szBuf, "%f", request_id);
			//vBody.AddMember("request_id", rapidjson::StringRef(szBuf, strlen(szBuf)), doc.GetAllocator());

			vDetailArray.PushBack(vDetail, allocator);
		}

		vBody.AddMember("details", vDetailArray, allocator);
	}

	std::string strMsgID((boost::format("%u") % msgID).str());
	COrderData::combo_head(vHead, doc, TS_FUNC_POSITION_QUERY, strMsgID.c_str(), strResCode, strDesc);
	doc.AddMember("head", vHead, allocator);
	doc.AddMember("body", vBody, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	strPacket.clear();

	strPacket = boost::locale::conv::between(buffer.GetString(), "UTF-8", "GBK");

	delete[]tPacket.data;
}

void CMySession::combo_asset_packet(T_Packet & tPacket, size_t msgID, std::string & strPacket)
{
	rapidjson::Document doc;
	doc.SetObject();
	rapidjson::Document::AllocatorType &allocator = doc.GetAllocator();

	rapidjson::Value vHead(rapidjson::kObjectType);

	std::string strMsgID((boost::format("%u") % msgID).str());

	if (0 == tPacket.nStatus)
	{
		XTPRI * xtpRI = (XTPRI*)tPacket.data;
		COrderData::combo_head(vHead, doc, TS_FUNC_ASSET, strMsgID.c_str(), TS_RES_CODE_QUERY_ASSERT_FAILED, xtpRI->error_msg);

		doc.AddMember("head", vHead, doc.GetAllocator());
	}
	else
	{
		COrderData::combo_head(vHead, doc, TS_FUNC_ASSET, strMsgID.c_str(), TS_RES_CODE_SUCCESS, "");
		rapidjson::Value vBodyArray(rapidjson::kArrayType);

		for (int i = 0; i < tPacket.nCount; ++i)
		{
			XTPQueryAssetRsp * asset_info = (XTPQueryAssetRsp*)tPacket.data + i;

			rapidjson::Value vBody(rapidjson::kObjectType);

			// 资金账号
			rapidjson::Value vAccount;
			T_XtpTradeInfo & xtpTradeInfo = theApp.m_config.getXtpTradeInfo(0);
			vAccount.SetString(xtpTradeInfo.szUserName, allocator);
			vBody.AddMember("account", vAccount, doc.GetAllocator());

			// 产品子账号
			std::string subacc;
			vBody.AddMember("subacc", "", doc.GetAllocator());

			// 总资产
			std::string total_asset((boost::format("%.4f") % asset_info->total_asset).str());
			rapidjson::Value vtotal_asset;
			vtotal_asset.SetString(total_asset.c_str(), allocator);
			vBody.AddMember("total_asset", vtotal_asset, doc.GetAllocator());

			// 可用资金
			std::string buying_power((boost::format("%.4f") % asset_info->buying_power).str());
			rapidjson::Value vbuying_power;
			vbuying_power.SetString(buying_power.c_str(), allocator);
			vBody.AddMember("buying_power", vbuying_power, doc.GetAllocator());

			// 证券资产
			std::string security_asset((boost::format("%.4f") % asset_info->security_asset).str());
			rapidjson::Value vsecurity_asset;
			vsecurity_asset.SetString(security_asset.c_str(), allocator);
			vBody.AddMember("security_asset", vsecurity_asset, doc.GetAllocator());

			// 累计买入成交证券占用资金
			std::string fund_buy_amount((boost::format("%.4f") % asset_info->fund_buy_amount).str());
			rapidjson::Value vfund_buy_amount;
			vfund_buy_amount.SetString(fund_buy_amount.c_str(), allocator);
			vBody.AddMember("fund_buy_amount", vfund_buy_amount, doc.GetAllocator());

			// 累计买入成交交易费用
			std::string fund_buy_fee((boost::format("%.4f") % asset_info->fund_buy_fee).str());
			rapidjson::Value vfund_buy_fee;
			vfund_buy_fee.SetString(fund_buy_fee.c_str(), allocator);
			vBody.AddMember("fund_buy_fee", vfund_buy_fee, doc.GetAllocator());

			// 累计卖出成交证券所得资金
			std::string fund_sell_amount((boost::format("%.4f") % asset_info->fund_sell_amount).str());
			rapidjson::Value vfund_sell_amount;
			vfund_sell_amount.SetString(fund_sell_amount.c_str(), allocator);
			vBody.AddMember("fund_sell_amount", vfund_sell_amount, doc.GetAllocator());

			// 累计卖出成交交易费用
			std::string fund_sell_fee((boost::format("%.4f") % asset_info->fund_sell_fee).str());
			rapidjson::Value vfund_sell_fee;
			vfund_sell_fee.SetString(fund_sell_fee.c_str(), allocator);
			vBody.AddMember("fund_sell_fee", vfund_sell_fee, doc.GetAllocator());

			// XTP系统预扣的资金
			std::string withholding_amount((boost::format("%.4f") % asset_info->withholding_amount).str());
			rapidjson::Value vwithholding_amount;
			vwithholding_amount.SetString(withholding_amount.c_str(), allocator);
			vBody.AddMember("withholding_amount", vwithholding_amount, doc.GetAllocator());

			vBodyArray.PushBack(vBody, doc.GetAllocator());
		}

		doc.AddMember("head", vHead, doc.GetAllocator());
		doc.AddMember("body", vBodyArray, doc.GetAllocator());
	}

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	strPacket.clear();
	strPacket = buffer.GetString();

	delete[]tPacket.data;
}

T_Addr & CMySession::get_remote_addr()
{
	return m_tRemoteAddr;
}

bool CMySession::is_exists_msg_id(size_t msgID)
{
	std::set<size_t>::iterator it = m_setMsgID.find(msgID);
	if (it == m_setMsgID.end())
		return false;

	m_setMsgID.erase(it);

	return true;
}




