#include "Log.h"



CLog::CLog()
{
}


CLog::~CLog()
{
}

void CLog::init(const char *szFileName, bool bOuputScreen/* = false*/)
{
	std::string strFileName(szFileName);

	// 获取当前日期
	std::string strDay = boost::gregorian::to_iso_string(boost::gregorian::day_clock::local_day());

	// 将日志输出到屏幕
	if (bOuputScreen)
	{
		boost::log::add_console_log(std::clog, boost::log::keywords::format = "%TimeStamp%: %Message%");
	}

	typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_file_backend > sink_t;
	boost::shared_ptr< boost::log::sinks::text_file_backend > backend;

	backend = boost::make_shared< boost::log::sinks::text_file_backend >(
		boost::log::keywords::file_name = "../log/" + strFileName +"%5N_" + strDay + ".log",
		boost::log::keywords::rotation_size = 5 * 1024 * 1024,
		boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(12, 0, 0)
		);
	
	boost::shared_ptr< sink_t > sink(new sink_t(backend));

	// 设置日志输出到文件中的格式
	sink->set_formatter(boost::log::expressions::format("[%1%]<%2%>: %3%")
		% boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
		% boost::log::expressions::attr<sign_severity_level>("Severity")
		% boost::log::expressions::smessage);

	// 日志过滤
	sink->set_filter(boost::log::expressions::attr<sign_severity_level>("Severity") > trace);

	//使日志实时更新
	sink->locked_backend()->auto_flush(true);

	boost::log::core::get()->add_sink(sink);

	boost::log::add_common_attributes();
}

logger_mt & CLog::get_logger_mt()
{
	return m_lg_mt;
}

