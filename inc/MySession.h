#ifndef XHT_TRADE_SESSION_H_
#define XHT_TRADE_SESSION_H_

#include <deque>
#include <set>
#include "boost/enable_shared_from_this.hpp"
#include "boost/asio.hpp"
#include "semaphore.h"
#include "message.h"
#include "DataType.h"
#include "document.h"
#include "stringbuffer.h"
#include "writer.h"


class CMyServer;

class CMySession : public boost::enable_shared_from_this<CMySession>
{
public:
	CMySession(boost::asio::io_service& io_service, CMyServer &server);
	~CMySession();

	boost::asio::ip::tcp::socket& get_socket();
	void start();

	void handle_read_header(const boost::system::error_code& error, std::size_t read_bytes);
	void handle_read_body(const boost::system::error_code& error, std::size_t read_bytes);

	/*
	 * @param error              发送结果
	 * @param bytes_transferred  实际发送长度
	 * @param expected_size      预计要发送的长度
	 * @param offset             待发送数据的偏移位置
	 */
	void handle_write(const boost::system::error_code& error, std::size_t bytes_transferred, std::size_t expected_size, std::size_t offset);

	// 向交易系统发送应答报文
	void send(message &msg);


	// 返回交易码
	std::string get_trdcode();

	// 组合订单查询应答报文
	void combo_order_query_packet(T_Packet & tPacket, size_t msgID, std::string &strPacket);
	
	// 组合成交查询应答报文
	void combo_trade_query_packet(T_Packet & tPacket, size_t msgID, std::string &strPacket);

	// 组合持仓应答报文
	void combo_position_packet(T_Packet & tPacket, size_t msgID, std::string &strPacket);

	// 组合资产信息应答报文
	void combo_asset_packet(T_Packet & tPacket, size_t msgID, std::string &strPacket);


	T_Addr & get_remote_addr();

	bool is_exists_msg_id(size_t msgID);

private:
	boost::asio::io_service			&m_io_service;
	boost::asio::ip::tcp::socket	m_socket;
	int m_nSessionID;
	CMyServer						&m_server;			// 用于从服务器中添加删除当前实例用的
	message							m_recv_buf;			// 数据接收缓冲区（接收从交易系统发送过来的请求报文）
	std::deque<message>				m_deque_res;		// 响应队列
	std::string						m_strTrdCode;		// 保存当前交易码
	T_Addr							m_tRemoteAddr;		// 客户端的地址

	std::set<size_t>				m_setMsgID;// 保存发送请求报文的唯一标示


};

#endif