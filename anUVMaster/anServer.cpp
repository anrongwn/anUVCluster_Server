#include "pch.h"
#include "anServer.h"


anServer::anServer()
{
}


anServer::~anServer()
{
	free_workers();
}

int anServer::init()
{
	int r = 0;

	if (std::atomic_exchange(&init_, true)) return r;

	loop_ = uv_default_loop();

	r = uv_tcp_init(loop_, &server_);
	if (r) {
		anuv::getlogger()->error("anServer::init()--uv_tcp_init={}, {}", r, anuv::getUVError_Info(r));
		return r;
	}
	uv_handle_set_data(reinterpret_cast<uv_handle_t*>(&server_), this);
	//uv_server_.data = this;

	r = uv_tcp_nodelay(&server_, 1);
	if (r) {
		anuv::getlogger()->error("anServer::init()--uv_tcp_nodelay(1)={}, {}", r, anuv::getUVError_Info(r));
		//return r;
	}

	r = uv_tcp_keepalive(&server_, 0, 0);
	if (r) {
		anuv::getlogger()->error("anServer::init()--uv_tcp_keepalive(0, 0)={}, {}", r, anuv::getUVError_Info(r));
		//return r;
	}

	r = uv_signal_init(loop_, &sig_);
	if (r) {
		anuv::getlogger()->error("anServer::init()--uv_signal_init={}, {}", r, anuv::getUVError_Info(r));
		//return r;
	}

	sig_.data = this;
	r = uv_signal_start(&sig_, anServer::on_signal, SIGINT);
	if (r) {
		anuv::getlogger()->error("anServer::init()--uv_signal_start={}, {}", r, anuv::getUVError_Info(r));
		//return r;
	}

	//得到cpu信息
	uv_cpu_info_t *cpuinfos=nullptr;
	r = uv_cpu_info(&cpuinfos, &worker_count_);
	if (r) {
		anuv::getlogger()->error("anServer::init()--uv_cpu_info={}, {}", r, anuv::getUVError_Info(r));
		//return r;
	}
	else
	{
		uv_free_cpu_info(cpuinfos, worker_count_);
	}
	
	//worker_count_ = 1;
	return r;
}


int anServer::setup_workers()
{
	std::string log = fmt::format("anServer::setup_workers(), ");
	int r = 0;
	size_t pathlen = 261;
	char worker_path[261] = { 0x00 };

	uv_exepath(worker_path, &pathlen);
	std::string tmp(worker_path, pathlen);
	auto pos = tmp.find_last_of('\\');
	tmp = tmp.substr(0, pos + 1);
	tmp += std::string(R"(anUVSlave.exe)");

	char *args[2] = { nullptr };
	args[0] = const_cast<char*>(tmp.c_str());
	log += tmp;
	

	//workers_ = (anWorker_handle*)calloc(worker_count_, sizeof(anWorker_handle));
	calloc_workers(worker_count_);
	int count = worker_count_;
	while (count--) {
		anWorker_handle* worker = &workers_[count];
		uv_pipe_init(loop_, &worker->pipe_, 1/*ipc*/);

		uv_stdio_container_t child_stdio[3];
		child_stdio[0].flags = (uv_stdio_flags)(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE); //UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE
		child_stdio[0].data.stream = (uv_stream_t*)&worker->pipe_;
		child_stdio[1].flags = (uv_stdio_flags)(UV_IGNORE);
		child_stdio[2].flags = (uv_stdio_flags)(UV_INHERIT_FD);
		child_stdio[2].data.fd=2;

		worker->options_.stdio = child_stdio;
		worker->options_.stdio_count = 3;
		worker->options_.exit_cb = anServer::close_process_handle;
		worker->options_.file = args[0];
		worker->options_.args = args;
		worker->options_.flags = UV_PROCESS_WINDOWS_HIDE;

		r = uv_spawn(loop_, &worker->req_, &worker->options_);
		if (r) {
			anuv::getlogger()->error(", uv_spawn()={}, {}", r, anuv::getUVError_Info(r));
		}
		else {
			log += fmt::format(",worker[{}] pid={} ", count, worker->req_.pid);
		}
	}

	anuv::getlogger()->info(log);

	return r;
}

int anServer::kill_workers()
{
	int r = 0;
	std::string log = fmt::format("anServer::kill_workers, ");
	anWorker_handle * worker = nullptr;
	for (int i = 0; i < worker_count_; ++i) {
		worker = &workers_[i];
		if (worker) {
			r = uv_process_kill(&worker->req_, SIGTERM);
			log += fmt::format(",worker[{}] pid={}, r={} ", i, worker->req_.pid, r);
		}
	}
	anuv::getlogger()->info(log);
	return r;
}

int anServer::start(const char * addr, const unsigned short port)
{
	int r = 0;

	r = init();
	if (r) return r;

	struct sockaddr_in bind_addr;
	r = uv_ip4_addr(addr, port, &bind_addr);
	if (r) {
		anuv::getlogger()->error("anServer::start({}, {})--uv_ip4_addr={}, {}", \
			addr, port, r, anuv::getUVError_Info(r));
		return r;
	}

	r = uv_tcp_bind(&server_, reinterpret_cast<struct sockaddr*>(&bind_addr), 0);
	if (r) {
		anuv::getlogger()->error("anServer::start({}, {})--uv_tcp_bind={}, {}", \
			addr, port, r, anuv::getUVError_Info(r));
		return r;
	}

	r = uv_listen(reinterpret_cast<uv_stream_t*>(&server_), 1024, anServer::on_new_connection);
	if (r) {
		anuv::getlogger()->error("anServer::start({}, {})--uv_listen={}, {}", \
			addr, port, r, anuv::getUVError_Info(r));
		return r;
	}

	r = setup_workers();

	return r;
}

int anServer::run()
{
	int r = 0;

	r = uv_run(loop_, UV_RUN_DEFAULT);

	return r;
}

int anServer::wait_exit()
{
	int r = 0;

	
	uv_walk(loop_, anServer::on_walk, nullptr);
	uv_run(loop_, UV_RUN_DEFAULT);
	do {
		r = uv_loop_close(loop_);
		if (UV_EBUSY == r) {
			uv_run(loop_, UV_RUN_NOWAIT);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));//防占cpu
		//anuv::getlogger()->debug("anTcpServer::wait_exit(). uv_loop_close()={}", r);
	} while (r);

	loop_ = nullptr;

	return r;
}

void anServer::on_new_connection(uv_stream_t * server, int status)
{
	std::string log = fmt::format("anServer::on_new_connection({:#08x}, {}), ", (int)server, status);
	if (status < 0) {
		if (UV_EOF == status) {
			uv_close(reinterpret_cast<uv_handle_t*>(server), nullptr);
		}

		log += anuv::getUVError_Info(status);
		anuv::getlogger()->error(log);

		return;
	}

	int r = 0;
	anServer * that = reinterpret_cast<anServer*>(server->data);
	uv_tcp_t  *client = new uv_tcp_t;

	r = uv_tcp_init(that->loop_, client);
	if (r) {
		log += fmt::format("uv_tcp_init={}, {}", r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);

		delete client;
		return;
	}

	client->data=that;
	r = uv_accept(reinterpret_cast<uv_stream_t*>(&that->server_), reinterpret_cast<uv_stream_t*>(client));
	if (r) {
		log += fmt::format("uv_accept={}, {}", r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);

		uv_close(reinterpret_cast<uv_handle_t*>(client), nullptr);
		delete client;
		return;
	}

	anWorker_handle * server_worker = that->get_server_worker();
	an_write_req * write_req = new an_write_req(that);

	log += fmt::format("client={:#08x}, write_req={:#08x}, worker pid={}", (int)client, (int)write_req, server_worker->req_.pid);
	r = uv_write2((uv_write_t*)write_req, (uv_stream_t*)&server_worker->pipe_, \
		&write_req->buf, 1, (uv_stream_t*)client, anServer::on_write);	//send connect socket handle
	if (r) {
		log += fmt::format("uv_write2({:#08x}, {:#08x})={},{}", (int)write_req, (int)client, r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);

		delete write_req;
		return;
	}

	anuv::getlogger()->info(log);

}

void anServer::alloc_buffer(uv_handle_t * handle, size_t suggested_size, uv_buf_t * buf)
{
	buf->base = (char *)malloc(suggested_size);
	buf->len = suggested_size;
}

void anServer::on_write(uv_write_t * req, int status)
{
	std::string log = fmt::format("anSocket::on_write({:#08x}, {})", (int)req, status);

	delete req;

	anuv::getlogger()->debug(log);
}


void anServer::on_close(uv_handle_t * handle)
{
	std::string log = fmt::format("anServer::on_close({:#08x}), ", (int)handle);

	if (UV_ASYNC == handle->type) {

	}
	else if (UV_TCP == handle->type) {
		
	}

	anuv::getlogger()->info(log);
}

void anServer::on_walk(uv_handle_t * handle, void * arg)
{
	if (!uv_is_closing(handle)) {
		uv_close(handle, nullptr);
	}
}
void anServer::on_signal(uv_signal_t * handle, int signum)
{
	std::string log = fmt::format("anServer::on_signal({:#08x}, {}), ", (int)handle, signum);
	int r = 0;
	
	anServer * that = reinterpret_cast<anServer *>(handle->data);


	r = that->kill_workers();

	//shutdown
	//r = that->shutdown();
	//
	uv_stop(that->loop_);

	uv_close((uv_handle_t*)&that->server_, nullptr);

	uv_signal_stop(handle);

	that->free_workers();

	anuv::getlogger()->info(log);
}

void anServer::close_process_handle(uv_process_t * req, int64_t exit_status, int term_signal)
{
	std::string log = fmt::format("anServer::close_process_handle({:#08x}, {}, {}， pid={}), ", \
		(int)req, exit_status, term_signal, req->pid);

	uv_close((uv_handle_t*)req, nullptr);

	anuv::getlogger()->info(log);
}
