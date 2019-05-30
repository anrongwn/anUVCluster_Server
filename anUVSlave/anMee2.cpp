#include "pch.h"
#include "anMee2.h"
#include "anTcpSocket.h"


anMee2::anMee2(uv_loop_t * loop) : loop_(loop)
{
}


anMee2::~anMee2()
{
}

void anMee2::on_work(uv_work_t * req)
{
	an_work_req * that = static_cast<an_work_req*>(req);
	std::string log = fmt::format("anMee2::on_work({:#08x}), tid={:#08x}", (int)that, (int)uv_thread_self());
	anTcpSocket *client = reinterpret_cast<anTcpSocket*>(that->data);

	that->set_result(client->package_handler2());

	anuv::getlogger()->debug(log);
}

void anMee2::on_after_work(uv_work_t * req, int status)
{
	an_work_req * that = static_cast<an_work_req*>(req);
	anTcpSocket *client = reinterpret_cast<anTcpSocket*>(that->data);
	std::string log = fmt::format("anMee2::on_after_work({:#08x}, {}), tid={:#08x}", (int)req, status, (int)uv_thread_self());

	for (auto& it : that->echo_result) {
		client->write_socket(it.data(), it.size());
	}

	delete that;

	anuv::getlogger()->debug(log);
}

int anMee2::start()
{
	int r = 0;
	
	return r;
}

int anMee2::stop()
{
	int r = 0;
	
	return r;
}

int anMee2::push_work(anTcpSocket * socket)
{
	int r = 0;
	std::string log = fmt::format("anMee2::push_work({:#08x}), ", (int)socket);


	an_work_req *work = new an_work_req(socket);
	r = uv_queue_work(this->loop_, work, anMee2::on_work, anMee2::on_after_work);
	if (r) {
		log += fmt::format("uv_queue_work()={}, {}", r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);
	}

	anuv::getlogger()->info(log);
	return r;
}
