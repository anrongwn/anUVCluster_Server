// pch.cpp: 与预编译标头对应的源文件；编译成功所必需的

#include "pch.h"

// 一般情况下，忽略此文件，但如果你使用的是预编译标头，请保留它。
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Userenv.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "libuv.lib")

//日志
anuv::anlogger& anuv::getlogger() {
	static anuv::anlogger g_anlog;
	if (!g_anlog) {
		/*
		std::string logpath(R"(D:\MyTest\2019_C++\anUVServer\logs\anUVServer)");
		logpath += ".log";
		*/
		size_t len = MAX_PATH + 1;
		char szpath[MAX_PATH + 1] = { 0 };
		/*
		GetModuleFileName(NULL, szpath, MAX_PATH);
		std::string logpath(szpath);
		*/

		uv_exepath(szpath, &len);
		std::string logpath(szpath, len);
		auto pos = logpath.find_last_of('\\');
		logpath = logpath.substr(0, pos + 1);

		logpath += R"(logs\anUVServer.log)";

		//是否已启动日志线程池?
		auto tp = spdlog::thread_pool();
		if (!tp) {
			spdlog::init_thread_pool(65536, 1);
		}
		g_anlog = spdlog::daily_logger_mt<spdlog::async_factory>("anUVServer", logpath);

		g_anlog->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^---%L---%$] [%t] %v");
#ifdef DEBUG
		g_anlog->set_level(spdlog::level::trace);
		//g_anlog->set_level(spdlog::level::info);
#else
		g_anlog->set_level(spdlog::level::info);
#endif

		g_anlog->flush_on(spdlog::level::info);

	}

	return g_anlog;
}

std::string anuv::getUVError_Info(const int ec) {
	std::string err;

	err += uv_err_name(ec);
	err += " : ";
	err += uv_strerror(ec);

	return std::move(err);

}