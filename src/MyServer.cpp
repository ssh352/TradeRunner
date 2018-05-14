#include <algorithm>
#include "MyServer.h"
#include "MySession.h"
#include "MyApp.h"


CMyServer::CMyServer(boost::asio::io_service& io_service, const boost::asio::ip::tcp::endpoint& endpoint)
	: m_ioService(io_service),
	m_acceptor(io_service, endpoint)
{
	start_accept();

	m_ofs.open("onlineCount.txt", std::ios::out);
}

CMyServer::~CMyServer()
{
}

void CMyServer::start_accept()
{
	SESSION_PTR new_session(new CMySession(m_ioService, *this));
	m_acceptor.async_accept(new_session->get_socket(),
		boost::bind(&CMyServer::handle_accept, this, new_session, boost::asio::placeholders::error));
}

void CMyServer::handle_accept(SESSION_PTR session, const boost::system::error_code& error)
{
	if (!error)
	{
		session->start();
	}

	start_accept();
}


void CMyServer::join(SESSION_PTR session)
{
	boost::mutex::scoped_lock lock(m_mutexSession);
	m_setSession.insert(session);

	if (m_ofs.is_open())
	{
		m_ofs << m_setSession.size() << std::endl;
		m_ofs.flush();
	}
}

void CMyServer::leave(SESSION_PTR session)
{
	boost::mutex::scoped_lock lock(m_mutexSession);
	m_setSession.erase(session);

	if (m_ofs.is_open())
	{
		m_ofs << m_setSession.size() << std::endl;
		m_ofs.flush();
	}
	
}

void CMyServer::run()
{
	m_ioService.run();
}

const SESSION_PTR & CMyServer::get_session(size_t msgID)
{
	std::set<SESSION_PTR>::iterator it;
	for (it = m_setSession.begin(); it != m_setSession.end(); ++it)
	{
		if ((*it)->is_exists_msg_id(msgID))
			return *it;
	}

	return m_sessionInvalid;
}



