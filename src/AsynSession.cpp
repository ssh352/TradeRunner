#include <map>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/locale.hpp"
#include "boost/format.hpp"
#include "document.h"
#include "stringbuffer.h"
#include "writer.h"
#include "AsynSession.h"
#include "OrderData.h"
#include "MyApp.h"

long CAsynSession::m_nCount = 0;

CAsynSession::CAsynSession(COrderData &owner, std::string strIP, int nPort, std::string &strPacket)
	: m_socket(owner.m_io_service),
	m_owner(owner),
	m_strSendData(strPacket),
	m_nRequestID(0)
{
	m_recv_buf.realloc(1024);
	++m_nCount;

	//std::string str((boost::format("准备发送第[%d]个异步应答报文") %m_nCount).str());
	//BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), debug) << str << "[" << __FILE__ << ":" << __LINE__ << "]";

	strncpy(m_tAddrTradeSys.szIP, strIP.c_str(), MAX_IP);
	m_tAddrTradeSys.nPort = nPort;

	try
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), debug) << "第[" << m_nCount << "]个异步应答报文开始连接交易系统，连接地址为：" << m_tAddrTradeSys.szIP << ":" << m_tAddrTradeSys.nPort << "[" << __FILE__ << ":" << __LINE__ << "]";

		boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(strIP.c_str()), nPort);
		m_socket.async_connect(ep, boost::bind(&CAsynSession::handle_connect, this, boost::asio::placeholders::error));
	}
	catch (std::exception &e)
	{
		std::string strMsg("连接交易系统失败，原因：");
		strMsg += e.what();
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "连接交易系统失败，原因：" << e.what() << "[" << __FILE__ << ":" << __LINE__ << "]";
	}
}

CAsynSession::~CAsynSession()
{
}

void CAsynSession::handle_connect(const boost::system::error_code & error)
{
	if (!error)
	{
		std::string strAddr = m_socket.local_endpoint().address().to_string();
		unsigned short uPort = m_socket.local_endpoint().port();
		strncpy(m_tLocalAddr.szIP, strAddr.c_str(), strAddr.length());
		m_tLocalAddr.nPort = uPort;

		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), debug) << "(res) send async response packet is:" << m_strSendData << "[" << __FILE__ << ":" << __LINE__ << "]";

		boost::asio::async_write(m_socket, boost::asio::buffer(m_strSendData, m_strSendData.length()),
			boost::bind(&CAsynSession::handle_write, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "连接交易系统[" << m_tAddrTradeSys.szIP << ":" << m_tAddrTradeSys.nPort << "]失败，原因:" << error.message() << "[" << __FILE__ << ":" << __LINE__ << "]";
		m_owner.leave_packet_res(shared_from_this());
		
	}
}

void CAsynSession::handle_read_header(const boost::system::error_code& error, size_t read_bytes)
{
	if (!error)
	{
		m_recv_buf.fill_end(read_bytes);
		int nLen = atoi(m_recv_buf.body());
		boost::asio::async_read(m_socket, boost::asio::buffer(m_recv_buf.data(), nLen),
			boost::bind(&CAsynSession::handle_read_body, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		
	}
}

void CAsynSession::handle_read_body(const boost::system::error_code& error, size_t read_bytes)
{
	
}

void CAsynSession::handle_write(const boost::system::error_code& error, size_t write_bytes)
{
	m_owner.leave_packet_res(shared_from_this());
}

boost::asio::ip::tcp::socket& CAsynSession::get_socket()
{
	return m_socket;
}

void CAsynSession::combo_head(rapidjson::Value &vHead, rapidjson::Document &doc, std::string strPacketID, std::string strTradeCode, std::string strResCode, std::string strMsg)
{
	rapidjson::Value vTrdid, vTrdcode, vTrdDatetime, vTrdRescode, vTrdDesc;

	std::string strTime = boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time());
	boost::algorithm::replace_first(strTime, "T", "");

	vTrdid.SetString(strPacketID.c_str(), strPacketID.length(), doc.GetAllocator());
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

void CAsynSession::combo_push_order(T_Packet & tPacket, std::string & strPacket)
{
	std::string strFunc, strRescode(TS_RES_CODE_SUCCESS), strMsg;

	rapidjson::Document doc;
	doc.SetObject();

	rapidjson::Value vHead(rapidjson::kObjectType);
	rapidjson::Value vBody(rapidjson::kObjectType);

	if (tPacket.nStatus == 0)// 如果该报文是失败的应答
	{
		XTPRI *xtpRI = (XTPRI *)tPacket.data;
		strMsg = xtpRI->error_msg;
		strRescode = TS_RES_CODE_ENTRUST_FAILED;
	}
	else
	{
		XTPOrderInfo* order_info = (XTPOrderInfo*)tPacket.data;

		if (XTP_ORDER_STATUS_ALLTRADED == order_info->order_status)// 全部成交
		{
			strFunc = TS_FUNC_ALL_TRADE_RES;
		}
		else if (XTP_ORDER_STATUS_PARTTRADEDNOTQUEUEING == order_info->order_status)// 部分撤单
		{
			strFunc = TS_FUNC_LEFT_CANCEL_ORDER_RES;
		}
		else if (XTP_ORDER_STATUS_CANCELED == order_info->order_status)// 已撤单
		{
			strFunc = TS_FUNC_CANCEL_ORDER_RES;
		}
		else if (XTP_ORDER_STATUS_NOTRADEQUEUEING == order_info->order_status)// 未成交(不推送：已经在xtp回调接口里拦截了，这里永远都不会执行)
		{
			strFunc = TS_FUNC_NO_TRADE_RES;
		}
		else if (XTP_ORDER_STATUS_REJECTED == order_info->order_status)// 已拒单（报单被拒单了）
		{
			strMsg = theApp.get_last_error();
			strRescode = TS_RES_CODE_ENTRUST_FAILED;
			strFunc = TS_FUNC_INSERT_ORDER_FAILED_RES;
		}
		else
		{
			printf("Unknown entrust state：%d", order_info->order_status);
			return;
		}

		combo_order_body(order_info, vBody, doc, strFunc);

	}// end if

	COrderData::combo_head(vHead, doc, TS_FUNC_ASYN_PUSH, "", strRescode, strMsg);
	doc.AddMember("head", vHead, doc.GetAllocator());
	doc.AddMember("body", vBody, doc.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	char szJsonLen[9] = { 0 };
	sprintf(szJsonLen, "%8zd", buffer.GetLength());

	strPacket.clear();
	strPacket += szJsonLen;
	strPacket += buffer.GetString();

	delete[] tPacket.data;
}

void CAsynSession::combo_order_body(XTPOrderInfo * order_info, rapidjson::Value &vBody, rapidjson::Document &doc, std::string strFuncCode)
{
	rapidjson::Value vFuncCode;
	vFuncCode.SetString(strFuncCode.c_str(), doc.GetAllocator());
	vBody.AddMember("funccode", vFuncCode, doc.GetAllocator());

	// 委托ID
	rapidjson::Value vOrder_xtp_id;
	std::string order_xtp_id((boost::format("%u") % order_info->order_xtp_id).str());
	vOrder_xtp_id.SetString(order_xtp_id.c_str(), doc.GetAllocator());
	vBody.AddMember("order_xtp_id", vOrder_xtp_id, doc.GetAllocator());

	// 客户ID 
	rapidjson::Value vOrder_client_id;
	std::string order_client_id((boost::format("%u") % order_info->order_client_id).str());
	vOrder_client_id.SetString(order_client_id.c_str(), doc.GetAllocator());
	vBody.AddMember("order_client_id", vOrder_client_id, doc.GetAllocator());

	// 撤单客户ID 
	rapidjson::Value vOrder_cancel_client_id;
	std::string order_cancel_client_id((boost::format("%u") % order_info->order_cancel_client_id).str());
	vOrder_cancel_client_id.SetString(order_cancel_client_id.c_str(), doc.GetAllocator());
	vBody.AddMember("order_cancel_client_id", vOrder_cancel_client_id, doc.GetAllocator());

	// 撤单ID
	rapidjson::Value vOrder_cancel_xtp_id;
	std::string order_cancel_xtp_id((boost::format("%u") % order_info->order_cancel_xtp_id).str());
	vOrder_cancel_xtp_id.SetString(order_cancel_xtp_id.c_str(), doc.GetAllocator());
	vBody.AddMember("order_cancel_xtp_id", vOrder_cancel_xtp_id, doc.GetAllocator());

	// 合约号
	rapidjson::Value vTicker;
	vTicker.SetString(order_info->ticker, doc.GetAllocator());
	vBody.AddMember("ticker", vTicker, doc.GetAllocator());

	// 市场
	rapidjson::Value vMarket;
	std::string market((boost::format("%d") % order_info->market).str());
	vMarket.SetString(market.c_str(), doc.GetAllocator());
	vBody.AddMember("market", vMarket, doc.GetAllocator());

	// 报单价格
	rapidjson::Value vReq_price;
	std::string price((boost::format("%.4f") % order_info->price).str());
	vReq_price.SetString(price.c_str(), doc.GetAllocator());
	vBody.AddMember("req_price", vReq_price, doc.GetAllocator());

	// 委托数量
	rapidjson::Value vQuantity;
	std::string quantity((boost::format("%lld") % order_info->quantity).str());
	vQuantity.SetString(quantity.c_str(), doc.GetAllocator());
	vBody.AddMember("quantity", vQuantity, doc.GetAllocator());

	// 价格类型
	rapidjson::Value vPrice_type;
	std::string price_type((boost::format("%d") % order_info->price_type).str());
	vPrice_type.SetString(price_type.c_str(), doc.GetAllocator());
	vBody.AddMember("price_type", vPrice_type, doc.GetAllocator());

	// 买卖方向
	rapidjson::Value vSide;
	std::string side;
	if (XTP_SIDE_BUY == order_info->side)
		side = "B";
	else if (XTP_SIDE_SELL == order_info->side)
		side = "S";
	else
		side = "未知类型";
	vSide.SetString(side.c_str(), doc.GetAllocator());
	vBody.AddMember("side", vSide, doc.GetAllocator());

	// 业务类型
	rapidjson::Value vBusiness_type;
	std::string business_type((boost::format("%d") % order_info->business_type).str());
	vBusiness_type.SetString(business_type.c_str(), doc.GetAllocator());
	vBody.AddMember("business_type", vBusiness_type, doc.GetAllocator());

	// 成交数量
	rapidjson::Value vQty_traded;
	std::string qty_traded((boost::format("%lld") % order_info->qty_traded).str());
	vQty_traded.SetString(qty_traded.c_str(), doc.GetAllocator());
	vBody.AddMember("qty_traded", vQty_traded, doc.GetAllocator());

	// 委托时间
	rapidjson::Value vInsert_time;
	std::string insert_time((boost::format("%lld") % order_info->insert_time).str());
	insert_time = insert_time.substr(0, insert_time.length()-3);
	vInsert_time.SetString(insert_time.c_str(), doc.GetAllocator());
	vBody.AddMember("insert_time", vInsert_time, doc.GetAllocator());

	// 最后修改时间
	rapidjson::Value vUpdate_time;
	std::string update_time;
	if (order_info->update_time != 0)
		update_time = ((boost::format("%lld") % order_info->update_time).str());
	else if (strFuncCode == TS_FUNC_CANCEL_ORDER_RES)
		update_time = (boost::format("%lld") % order_info->cancel_time).str();
	else
		update_time = (boost::format("%lld") % order_info->insert_time).str();
	update_time = update_time.substr(0, update_time.length() - 3);
	vUpdate_time.SetString(update_time.c_str(), doc.GetAllocator());
	vBody.AddMember("update_time", vUpdate_time, doc.GetAllocator());

	// 撤单数量
	rapidjson::Value vCancel_num;
	std::string qty_left((boost::format("%lld") % order_info->qty_left).str());
	vCancel_num.SetString(qty_left.c_str(), doc.GetAllocator());
	vBody.AddMember("cancel_num", vCancel_num, doc.GetAllocator());

	// 撤单时间
	rapidjson::Value vCancel_time;
	std::string cancel_time((boost::format("%lld") % order_info->cancel_time).str());
	cancel_time = cancel_time.substr(0, cancel_time.length() - 3);
	vCancel_time.SetString(cancel_time.c_str(), doc.GetAllocator());
	vBody.AddMember("cancel_time", vCancel_time, doc.GetAllocator());

	// 总成交金额
	rapidjson::Value vTrade_amount;
	std::string trade_amount((boost::format("%.0f") % order_info->trade_amount).str());
	vTrade_amount.SetString(trade_amount.c_str(), doc.GetAllocator());
	vBody.AddMember("trade_amount", vTrade_amount, doc.GetAllocator());

	// 本地报单编号
	rapidjson::Value vOrder_local_id;
	vOrder_local_id.SetString(order_info->order_local_id, doc.GetAllocator());
	vBody.AddMember("order_local_id", vOrder_local_id, doc.GetAllocator());

	// 报单类型
	rapidjson::Value vOrder_type;
	std::string order_type((boost::format("%d") % order_info->order_type).str());
	vOrder_type.SetString(order_type.c_str(), doc.GetAllocator());
	vBody.AddMember("order_type", vOrder_type, doc.GetAllocator());

	// 委托状态
	rapidjson::Value vOrder_status;
	std::string order_status((boost::format("%d") % order_info->order_status).str());
	vOrder_status.SetString(order_status.c_str(), doc.GetAllocator());
	vBody.AddMember("order_status", vOrder_status, doc.GetAllocator());

	// 报单提交状态
	rapidjson::Value vOrder_submit_status;
	std::string order_submit_status((boost::format("%d") % order_info->order_submit_status).str());
	vOrder_submit_status.SetString(order_submit_status.c_str(), doc.GetAllocator());
	vBody.AddMember("order_submit_status", vOrder_submit_status, doc.GetAllocator());

	// 在订单推送中没有下面这些字段
	rapidjson::Value vTrdade_sucprice, vTrdade_quantity, vTrade_time;
	rapidjson::Value vExec_id, vReport_index, vOrder_exch_id;
	rapidjson::Value vTrade_type, vCancel_price, vCancel_amount, vTrd_tfee;

	vTrdade_sucprice.SetString("", doc.GetAllocator());
	vTrdade_quantity.SetString("", doc.GetAllocator());
	vTrade_time.SetString("", doc.GetAllocator());
	vExec_id.SetString("", doc.GetAllocator());
	vReport_index.SetString("", doc.GetAllocator());
	vOrder_exch_id.SetString("", doc.GetAllocator());
	vTrade_type.SetString("", doc.GetAllocator());
	vCancel_price.SetString("", doc.GetAllocator());
	vCancel_amount.SetString("", doc.GetAllocator());
	vTrd_tfee.SetString("", doc.GetAllocator());

	vBody.AddMember("trdade_sucprice", vTrdade_sucprice, doc.GetAllocator());	// 成交价格
	vBody.AddMember("trdade_quantity", vTrdade_quantity, doc.GetAllocator());	// 成交数量
	vBody.AddMember("trade_time", vTrade_time, doc.GetAllocator());				// 成交时间
	vBody.AddMember("exec_id", vExec_id, doc.GetAllocator());					// 成交编号
	vBody.AddMember("report_index", vReport_index, doc.GetAllocator());			// 成交序号
	vBody.AddMember("order_exch_id", vOrder_exch_id, doc.GetAllocator());		// 报单编号
	vBody.AddMember("trade_type", vTrade_type, doc.GetAllocator());				// 成交类型

	// xtp应答中没有没有下面字段
	vBody.AddMember("cancel_price", vCancel_price, doc.GetAllocator());			// 撤单价格
	vBody.AddMember("cancel_amount", vCancel_amount, doc.GetAllocator());		// 撤单金额
	vBody.AddMember("trd_tfee", vTrd_tfee, doc.GetAllocator());					// 总费用
}

void CAsynSession::combo_push_trade(T_Packet & tPacket, std::string & strPacket)
{
	rapidjson::Document doc;
	doc.SetObject();

	rapidjson::Value vHead(rapidjson::kObjectType);
	rapidjson::Value vBody(rapidjson::kObjectType);

	XTPTradeReport* trade_info = (XTPTradeReport*)tPacket.data;
	COrderData::combo_head(vHead, doc, TS_FUNC_ASYN_PUSH, "", TS_RES_CODE_SUCCESS, "");

	rapidjson::Value vFuncCode;
	vFuncCode.SetString(TS_FUNC_TRADE_RES, doc.GetAllocator());
	vBody.AddMember("funccode", vFuncCode, doc.GetAllocator());

	// 委托ID
	std::string order_xtp_id((boost::format("%u") % trade_info->order_xtp_id).str());
	rapidjson::Value vOrder_xtp_id;
	vOrder_xtp_id.SetString(order_xtp_id.c_str(), doc.GetAllocator());
	vBody.AddMember("order_xtp_id", vOrder_xtp_id, doc.GetAllocator());

	// 客户ID 
	std::string order_client_id((boost::format("%u") % trade_info->order_client_id).str());
	rapidjson::Value vOrder_client_id;
	vOrder_client_id.SetString(order_client_id.c_str(), doc.GetAllocator());
	vBody.AddMember("order_client_id", vOrder_client_id, doc.GetAllocator());

	// 合约号
	rapidjson::Value vTicker;
	vTicker.SetString(trade_info->ticker, doc.GetAllocator());
	vBody.AddMember("ticker", vTicker, doc.GetAllocator());

	// 市场
	std::string market((boost::format("%d") % trade_info->market).str());
	rapidjson::Value vMarket;
	vMarket.SetString(market.c_str(), doc.GetAllocator());
	vBody.AddMember("market", vMarket, doc.GetAllocator());

	// 报单价格
	std::string price((boost::format("%.4f") % trade_info->price).str());
	rapidjson::Value vReq_price;
	vReq_price.SetString(price.c_str(), doc.GetAllocator());
	vBody.AddMember("req_price", vReq_price, doc.GetAllocator());

	// 委托数量
	std::string quantity((boost::format("%lld") % trade_info->quantity).str());
	rapidjson::Value vQuantity;
	vQuantity.SetString(quantity.c_str(), doc.GetAllocator());
	vBody.AddMember("quantity", vQuantity, doc.GetAllocator());

	// 买卖方向
	std::string side;
	if (XTP_SIDE_BUY == trade_info->side)
		side = "B";
	else if (XTP_SIDE_SELL == trade_info->side)
		side = "S";
	else
		side = "未知类型";

	rapidjson::Value vSide;
	vSide.SetString(side.c_str(), doc.GetAllocator());
	vBody.AddMember("side", vSide, doc.GetAllocator());

	// 业务类型
	std::string business_type((boost::format("%d") % trade_info->business_type).str());
	rapidjson::Value vBusiness_type;
	vBusiness_type.SetString(business_type.c_str(), doc.GetAllocator());
	vBody.AddMember("business_type", vBusiness_type, doc.GetAllocator());

	// 总成交金额
	std::string trade_amount((boost::format("%.0f") % trade_info->trade_amount).str());
	rapidjson::Value vTrade_amount;
	vTrade_amount.SetString(trade_amount.c_str(), doc.GetAllocator());
	vBody.AddMember("trade_amount", vTrade_amount, doc.GetAllocator());

	// 成交价格
	std::string trdade_sucprice((boost::format("%.4f") % trade_info->price).str());
	rapidjson::Value vTrdade_sucprice;
	vTrdade_sucprice.SetString(trdade_sucprice.c_str(), doc.GetAllocator());
	vBody.AddMember("trdade_sucprice", vTrdade_sucprice, doc.GetAllocator());

	// 成交数量
	std::string trdade_quantity((boost::format("%lld") % trade_info->quantity).str());
	rapidjson::Value vTrdade_quantity;
	vTrdade_quantity.SetString(trdade_quantity.c_str(), doc.GetAllocator());
	vBody.AddMember("trdade_quantity", vTrdade_quantity, doc.GetAllocator());

	// 成交时间
	std::string trade_time((boost::format("%lld") % trade_info->trade_time).str());
	trade_time = trade_time.substr(0, trade_time.length() - 3);
	rapidjson::Value vTrade_time;
	vTrade_time.SetString(trade_time.c_str(), doc.GetAllocator());
	vBody.AddMember("trade_time", vTrade_time, doc.GetAllocator());

	// 成交编号
	rapidjson::Value vExec_id;
	vExec_id.SetString(trade_info->exec_id, doc.GetAllocator());
	vBody.AddMember("exec_id", vExec_id, doc.GetAllocator());

	// 成交序号
	std::string report_index((boost::format("%u") % trade_info->report_index).str());
	rapidjson::Value vReport_index;
	vReport_index.SetString(report_index.c_str(), doc.GetAllocator());
	vBody.AddMember("report_index", vReport_index, doc.GetAllocator());

	// 报单编号
	rapidjson::Value vOrder_exch_id;
	vOrder_exch_id.SetString(trade_info->order_exch_id, doc.GetAllocator());
	vBody.AddMember("order_exch_id", vOrder_exch_id, doc.GetAllocator());

	// 成交类型
	std::string trade_type((boost::format("%c") % trade_info->trade_type).str());
	rapidjson::Value vTrade_type;
	vTrade_type.SetString(trade_type.c_str(), doc.GetAllocator());
	vBody.AddMember("trade_type", vTrade_type, doc.GetAllocator());

	// 成交推送中没下面的字段
	rapidjson::Value vOrder_cancel_client_id, vOrder_cancel_xtp_id, vPrice_type;
	rapidjson::Value vQty_traded, vInsert_time, vUpdate_time;
	rapidjson::Value vCancel_num, vCancel_time, vOrder_local_id;
	rapidjson::Value vOrder_type, vOrder_status, vOrder_submit_status;
	rapidjson::Value vCancel_price, vCancel_amount, vTrd_tfee;

	vOrder_cancel_client_id.SetString("", doc.GetAllocator());
	vOrder_cancel_xtp_id.SetString("", doc.GetAllocator());
	vPrice_type.SetString("", doc.GetAllocator());
	vQty_traded.SetString("", doc.GetAllocator());
	vInsert_time.SetString("", doc.GetAllocator());
	vUpdate_time.SetString("", doc.GetAllocator());
	vCancel_num.SetString("", doc.GetAllocator());
	vCancel_time.SetString("", doc.GetAllocator());
	vOrder_local_id.SetString("", doc.GetAllocator());
	vOrder_type.SetString("", doc.GetAllocator());
	vOrder_status.SetString("", doc.GetAllocator());
	vOrder_submit_status.SetString("", doc.GetAllocator());
	vCancel_price.SetString("", doc.GetAllocator());
	vCancel_amount.SetString("", doc.GetAllocator());
	vTrd_tfee.SetString("", doc.GetAllocator());

	vBody.AddMember("order_cancel_client_id", vOrder_cancel_client_id, doc.GetAllocator());		// 撤单客户ID 
	vBody.AddMember("order_cancel_xtp_id", vOrder_cancel_xtp_id, doc.GetAllocator());			// 撤单ID
	vBody.AddMember("price_type", vPrice_type, doc.GetAllocator());								// 价格类型
	vBody.AddMember("qty_traded", vQty_traded, doc.GetAllocator());								// 成交数量(累计成交数量)
	vBody.AddMember("insert_time", vInsert_time, doc.GetAllocator());							// 委托时间
	vBody.AddMember("update_time", vUpdate_time, doc.GetAllocator());							// 最后修改时间
	vBody.AddMember("cancel_num", vCancel_num, doc.GetAllocator());								// 撤单数量
	vBody.AddMember("cancel_time", vCancel_time, doc.GetAllocator());							// 撤单时间
	vBody.AddMember("order_local_id", vOrder_local_id, doc.GetAllocator());						// 本地报单编号
	vBody.AddMember("order_type", vOrder_type, doc.GetAllocator());								// 报单类型
	vBody.AddMember("order_status", vOrder_status, doc.GetAllocator());							// 委托状态
	vBody.AddMember("order_submit_status", vOrder_submit_status, doc.GetAllocator());			// 报单提交状态

	// xtp应答中没有下面字段
	vBody.AddMember("cancel_price", vCancel_price, doc.GetAllocator());							// 撤单价格
	vBody.AddMember("cancel_amount", vCancel_amount, doc.GetAllocator());						// 撤单金额
	vBody.AddMember("trd_tfee", vTrd_tfee, doc.GetAllocator());									// 总费用

	doc.AddMember("head", vHead, doc.GetAllocator());
	doc.AddMember("body", vBody, doc.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	char szJsonLen[9] = { 0 };
	sprintf(szJsonLen, "%8zd", buffer.GetLength());

	strPacket.clear();
	strPacket += szJsonLen;
	strPacket += buffer.GetString();

	delete[] tPacket.data;
}

void CAsynSession::combo_push_cancel_order(T_Packet & tPacket, std::string & strPacket)
{
	rapidjson::Document doc;
	doc.SetObject();
	rapidjson::Document::AllocatorType &alloctor = doc.GetAllocator();

	rapidjson::Value vHead(rapidjson::kObjectType);
	rapidjson::Value vBody(rapidjson::kObjectType);

	XTPOrderCancelInfo* trade_info = (XTPOrderCancelInfo*)tPacket.data;
	COrderData::combo_head(vHead, doc, TS_FUNC_ASYN_PUSH, "", TS_RES_CODE_CANCEL_ORDER_FAILED, "");

	rapidjson::Value vfunccode;
	rapidjson::Value vorder_xtp_id;//原始订单XTPID
	rapidjson::Value vorder_cancel_xtp_id;//撤单XTPID

	vfunccode.SetString(TS_FUNC_CANCEL_FAILED_RES, alloctor);
	vorder_xtp_id.SetString((boost::format("%u") % trade_info->order_xtp_id).str().c_str(), alloctor);
	vorder_cancel_xtp_id.SetString((boost::format("%u") % trade_info->order_cancel_xtp_id).str().c_str(), alloctor);

	vBody.AddMember("funccode", vfunccode, alloctor);
	vBody.AddMember("order_xtp_id", vorder_xtp_id, alloctor);
	vBody.AddMember("order_cancel_xtp_id", vorder_cancel_xtp_id, alloctor);

	vBody.AddMember("order_client_id", "", alloctor);
	vBody.AddMember("order_cancel_client_id", "", alloctor);
	vBody.AddMember("ticker", "", alloctor);
	vBody.AddMember("market", "", alloctor);
	vBody.AddMember("req_price", "", alloctor);
	vBody.AddMember("quantity", "", alloctor);
	vBody.AddMember("price_type", "", alloctor);
	vBody.AddMember("side", "", alloctor);
	vBody.AddMember("business_type", "", alloctor);
	vBody.AddMember("qty_traded", "", alloctor);
	vBody.AddMember("insert_time", "", alloctor);
	vBody.AddMember("update_time", "", alloctor);
	vBody.AddMember("cancel_num", "", alloctor);
	vBody.AddMember("cancel_price", "", alloctor);
	vBody.AddMember("cancel_amount", "", alloctor);
	vBody.AddMember("cancel_time", "", alloctor);
	vBody.AddMember("trade_amount", "", alloctor);	
	vBody.AddMember("trdade_sucprice", "", alloctor);
	vBody.AddMember("trdade_quantity", "", alloctor);
	vBody.AddMember("trade_time", "", alloctor);
	vBody.AddMember("exec_id", "", alloctor);
	vBody.AddMember("report_index", "", alloctor);
	vBody.AddMember("order_exch_id", "", alloctor);
	vBody.AddMember("trade_type", "", alloctor);
	vBody.AddMember("trd_tfee", "", alloctor);
	vBody.AddMember("order_local_id", "", alloctor);
	vBody.AddMember("order_type", "", alloctor);
	vBody.AddMember("order_status", "", alloctor);
	vBody.AddMember("order_submit_status", "", alloctor);

	doc.AddMember("head", vHead, alloctor);
	doc.AddMember("body", vBody, alloctor);

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	char szJsonLen[9] = { 0 };
	sprintf(szJsonLen, "%8zd", buffer.GetLength());

	strPacket.clear();
	strPacket += szJsonLen;
	strPacket += buffer.GetString();

	delete[] tPacket.data;
	
}
