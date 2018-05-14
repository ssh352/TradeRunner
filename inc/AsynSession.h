#ifndef ASYN_SESSION_H_
#define ASYN_SESSION_H_

#include <deque>
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/asio.hpp"
#include "boost/thread.hpp"
#include "MyServer.h"
#include "semaphore.h"
#include "xtp_api_struct.h"
#include "DataType.h"
#include "message.h"
#include "document.h"
#include "stringbuffer.h"
#include "writer.h"
#include "DataType.h"

class COrderData;


class CAsynSession : public boost::enable_shared_from_this<CAsynSession>
{
public:
	CAsynSession(COrderData &owner, std::string strIP, int nPort, std::string &strPacket);
	~CAsynSession();
	void handle_connect(const boost::system::error_code& error);
	void handle_read_header(const boost::system::error_code& error, size_t read_bytes);
	void handle_read_body(const boost::system::error_code& error, size_t read_bytes);
	void handle_write(const boost::system::error_code& error, size_t write_bytes);

public:
	boost::asio::ip::tcp::socket& get_socket();

	// 组合应答报文头
	static void combo_head(rapidjson::Value &vHead, rapidjson::Document &doc, std::string strPacketID, std::string strTradeCode, std::string strResCode, std::string strMsg);
	
	// 组合推送的订单报文
	static void combo_push_order(T_Packet & tPacket, std::string & strPacket);
	static void combo_order_body(XTPOrderInfo* order_info, rapidjson::Value &vBody, rapidjson::Document &doc, std::string strFuncCode);

	// 组合推送的成交报文
	static void combo_push_trade(T_Packet & tPacket, std::string & strPacket);

	static void combo_push_cancel_order(T_Packet & tPacket, std::string & strPacket);

private:
	COrderData &m_owner;
	boost::asio::ip::tcp::socket m_socket;		// 当前用户的通信套接字
	message m_recv_buf;							// 接收缓冲区
	std::string m_strSendData;					// 要发送的报文
	uint64_t m_nRequestID;
	T_Addr m_tLocalAddr;						// 本地连接的地址
	T_Addr m_tAddrTradeSys;						// 交易系统的地址

	static long m_nCount;
};

typedef boost::shared_ptr<CAsynSession> ASYN_SESSION_PTR;

#endif // ASYN_SESSION_H_