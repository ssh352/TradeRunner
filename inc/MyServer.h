#ifndef MY_SERVER_H_
#define MY_SERVER_H_
#include <set>
#include <fstream>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#ifdef _WINDOWS
#include <Windows.h>
#else

#endif

class CMySession;

typedef boost::shared_ptr<CMySession> SESSION_PTR;


class CMyServer : public boost::enable_shared_from_this<CMyServer>
{
public:
	CMyServer(boost::asio::io_service& io_service,const boost::asio::ip::tcp::endpoint& endpoint);
	~CMyServer();

	void start_accept();
	void handle_accept(SESSION_PTR session, const boost::system::error_code& error);

	void join(SESSION_PTR session);// 添加会话实例
	void leave(SESSION_PTR session);// 删除会话实例

	void run();

	const SESSION_PTR& get_session(size_t msgID);


private:
	boost::asio::io_service& m_ioService;
	boost::asio::ip::tcp::acceptor m_acceptor;
	boost::mutex m_mutexSession;

	std::set<SESSION_PTR> m_setSession;		// 存储所有交易会话实例

	SESSION_PTR m_sessionInvalid;

	
	//std::fstream m_ofs;
};

typedef boost::shared_ptr<CMyServer> server_ptr;


#endif // MY_SERVER_H_