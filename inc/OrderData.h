/*
 * COrderData
 *
 * @brief 保存委托应答、全部成交应答、撤单应答、剩撤回报应答数据
 */
#ifndef ORDER_DATA_H_
#define ORDER_DATA_H_
#include <set>
#include <boost/asio.hpp>
#include "semaphore.h"
#include "DataType.h"
#include "AsynSession.h"
#include "document.h"
#include "stringbuffer.h"
#include "writer.h"


class COrderData
{
public:
	COrderData();
	~COrderData();

public:
	static void listen_io(COrderData *pThis);// 监听异步应答报文（向交易系统发送的异步应答报文）
	static void thread_handler_res(COrderData *pThis);// 监听来自xtp服务端的应答线程
	static void thread_handler_req(COrderData *pThis);// 监听来自交易系统的请求线程
	void run();// 开始监听异步应答数据

	// 请求报文的添加和获取
	void push_packet_req(message &msg);
	void pop_packet_req(message &msg);

	// 应答报文的添加和获取
	void push_packet_res(T_Packet &tPacket);
	void pop_packet_res(T_Packet &tPacket);

	void add_packet_res(ASYN_SESSION_PTR asynSession);
	// 删除推送的应答数据包
	void leave_packet_res(ASYN_SESSION_PTR asynSession);

	// 处理来自xtp服务端的应答数据包
	void handle_res_packet(T_Packet &tPacket);
	
	// 处理来自交易系统的请求
	void handle_req(message &msg);
	void handle_req_insert_order(rapidjson::Document &doc, size_t msgId);
	void handle_req_cancel_order(rapidjson::Document &doc, size_t msgId);
	void handle_req_order_query_for_id(rapidjson::Document &doc, size_t msgId);
	void handle_req_order_query(rapidjson::Document &doc, size_t msgId);
	void handle_req_trade_query_for_id(rapidjson::Document &doc, size_t msgId);
	void handle_req_trade_query(rapidjson::Document &doc, size_t msgId);
	void handle_req_position_query(rapidjson::Document &doc, size_t msgId);
	void handle_req_asset(rapidjson::Document &doc, size_t msgId);

	// 组合向交易系统发送的应答报文头
	static void combo_head(rapidjson::Value & vHead, rapidjson::Document & doc, std::string strTradeCode, 
		                   std::string strTrdid, std::string strResCode, std::string strMsg);

	// 启动和交易系统的收发线程
	void start_thread();

	
	

public:
	boost::asio::io_service m_io_service;// 用于监听向交易系统发送的异步应答报文

private:
	typedef std::set<ASYN_SESSION_PTR> SET_ASYNMSG;
	boost::mutex			m_mutexAsynRes;			// 异步应答互斥量
	boost::mutex			m_mutexSynRes;			// 同步应答互斥量
	boost::mutex			m_mutexReq;				// 请求互斥量
	semaphore				m_semaphoreSynRes;		// 同步应答信号量
	semaphore				m_semaphoreReq;			// 请求信号量
	std::deque<T_Packet>	m_dequePacketRes;		// 保存来自xtp服务端的应答报文
	std::deque<message>		m_dequePacketReq;		// 保存来自交易系统的请求报文
	std::string				m_strTradeQueryFunc;	// 保存成交查询交易码
	std::string				m_strEntrustQueryFunc;	// 保存委托查询交易码
	SET_ASYNMSG				m_setAsynMsg;			// 异步应答数据报文

};

#endif //ORDER_DATA_H_
