#ifndef CONFIG_H_
#define CONFIG_H_
#include "DataType.h"

class CConfig
{
public:
	CConfig();
	~CConfig();

	bool read_config();

	// 返回交易前置的地址
	T_Addr & getTradeGetwayAddr();

	// 返回交易系统的地址
	T_Addr & getTradeSysAddr();

	// 返回xtp服务器的地址
	T_XtpTradeInfo & getXtpTradeInfo(int pos);

	// 返回要创建的线程个数
	int get_thread_count();

	int get_client_id();

private:
	T_Addr			m_tTradeGetwayAddr;		// 交易前置的监听地址
	T_Addr			m_tTradeSysAddr;		// 交易系统地址
	T_XtpTradeInfo	m_xtpTradeInfo[2];		// xtp交易服务器的地址
	int				m_nThreadCount;			// 处理请求和应答的线程数
	int				m_nClientID;			// 连接xtp系统的client id
};

#endif