#include <iostream>
#include "MyServer.h"
#include "MyApp.h"
#include "XtpTradeReq.h"
#include "XtpTradeRes.h"

void print();

int main(int argc, char *argv[])
{
	// 读取配置
	if (!theApp.m_config.read_config())
	{
		print();
		return -1;
	}

	// 连接xtp交易服务器
	if (!theApp.connect_xtp())
	{
		print();
		return -1;
	}

	// 启动收发线程
	theApp.m_orderDta.start_thread();
	
	T_Addr &tAddr = theApp.m_config.getTradeGetwayAddr();

	BOOST_LOG_SEV(theApp.m_log.get_logger_mt(), warning) << "listen to addr:" << tAddr.szIP << ":" <<tAddr.nPort << "[" << __FILE__ << ":" << __LINE__ << "]";

	// 启动监听
	try
	{
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(tAddr.szIP), tAddr.nPort);
		server_ptr server(new CMyServer(theApp.get_io_service(), endpoint));
		theApp.m_server = server;
		server.reset();

		printf("Trade getway start successfully, address is:%s:%d\n", tAddr.szIP, tAddr.nPort);


		theApp.m_server->run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Trade getway start failure, cause: " << e.what() << "\n";
	}

	print();

	return 0;
}

void print()
{
	std::cout << std::endl << "Press any key to exit." << std::endl;
	std::cin.get();
}