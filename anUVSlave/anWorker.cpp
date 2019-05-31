#include "pch.h"
#include "anWorker.h"
#include "anTcpSocket.h"
#include "anMee2.h"


anWorker::anWorker()
{
	
}


anWorker::~anWorker()
{
}

int anWorker::start()
{
	int r = 0;

	r = init();
	if (r) return r;

	
	r = uv_read_start((uv_stream_t*)&uv_server_, anWorker::alloc_buffer, anWorker::on_new_connection);
	if (r) {
		anuv::getlogger()->error("anWorker::start()--uv_read_start={}, {}", r, anuv::getUVError_Info(r));
		return r;
	}

	//r = message_handler_->start();
	r = message_handler2_->start();
	if (r) {
		anuv::getlogger()->error("anWorker::start()--message_handler_->start={}, {}", \
			r, anuv::getUVError_Info(r));
		return r;
	}

	return r;
}

int anWorker::run()
{
	int r = 0;

	r = uv_run(uv_loop_, UV_RUN_DEFAULT);

	return r;
}

int anWorker::wait_exit()
{
	int r = 0;

	//
	message_handler2_->stop();

	uv_walk(uv_loop_, anWorker::on_walk, nullptr);
	uv_run(uv_loop_, UV_RUN_DEFAULT);
	do {
		r = uv_loop_close(uv_loop_);
		if (UV_EBUSY == r) {
			uv_run(uv_loop_, UV_RUN_NOWAIT);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));//防占cpu
		//anuv::getlogger()->debug("anWorker::wait_exit(). uv_loop_close()={}", r);
	} while (r);

	uv_loop_ = nullptr;

	return r;
}

int anWorker::push_work(anTcpSocket * socket)
{
	//return message_handler_->push_work(socket);
	return message_handler2_->push_work(socket);
}


int anWorker::init()
{
	int r = 0;

	if (std::atomic_exchange(&init_, true)) return r;

	uv_loop_ = uv_default_loop();
	message_handler2_ = std::make_unique<anMee2>(uv_loop_);

	r = uv_pipe_init(uv_loop_, &uv_server_, 1/*ipc*/);
	if (r) {
		anuv::getlogger()->error("anWorker::init()--uv_pipe_init={}, {}", r, anuv::getUVError_Info(r));
		return r;
	}
	uv_server_.data = this;

	r = uv_pipe_open(&uv_server_, 0);//open stdin
	if (r) {
		anuv::getlogger()->error("anWorker::init()--uv_pipe_open={}, {}", r, anuv::getUVError_Info(r));
		return r;
	}

	r = uv_signal_init(uv_loop_, &sig_);
	if (r) {
		anuv::getlogger()->error("anWorker::init()--uv_signal_init={}, {}", r, anuv::getUVError_Info(r));
		//return r;
	}

	sig_.data = this;
	r = uv_signal_start(&sig_, anWorker::on_signal, SIGTERM);
	if (r) {
		anuv::getlogger()->error("anWorker::init()--uv_signal_start={}, {}", r, anuv::getUVError_Info(r));
		//return r;
	}

	return r;
}

void anWorker::on_new_connection(uv_stream_t * q, ssize_t nread, const uv_buf_t * buf)
{
	std::string log = fmt::format("anWorker::on_new_connection({:#08x}, {})", (int)q, nread);
	if (nread < 0) {
		log += anuv::getUVError_Info(nread);
		
		uv_close((uv_handle_t*)q, NULL);

		anuv::getlogger()->error(log);
		return;
	}
	int r = 0;
	uv_pipe_t *pipe = (uv_pipe_t*)q;
	
	r = uv_pipe_pending_count(pipe);
	if (!r) {
		log += fmt::format(", uv_pipe_pending_count({:#08x})={} No pending count, ", (int)pipe, r);;
		anuv::getlogger()->error(log);
		return;
	}

	uv_handle_type pending = uv_pipe_pending_type(pipe);
	log += fmt::format(", uv_pipe_pending_type({:#08x})={} ", (int)pipe, pending);;
	assert(pending == UV_TCP);
	
	
	anWorker * that = reinterpret_cast<anWorker*>(q->data);
	int sessionid = anWorker::get_sessionID();
	anTcpSocket *client = new anTcpSocket(sessionid);

	r = uv_tcp_init(that->uv_loop_, client);
	if (r) {
		log += fmt::format(",uv_tcp_init={}, {}", r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);

		delete client;
		return;
	}
	client->set_Server(that);

	r = uv_accept(q, (uv_stream_t*)client);
	if (r) {
		log += fmt::format(",uv_accept={}, {}", r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);

		uv_close(reinterpret_cast<uv_handle_t*>(client), nullptr);
		delete client;
		return;
	}

	that->client_lists_.insert(std::make_pair(sessionid, client));

	uv_os_fd_t fd;
	uv_fileno((const uv_handle_t*)client, &fd);

	r = uv_read_start(reinterpret_cast<uv_stream_t*>(client), anWorker::alloc_buffer, anWorker::on_read);
	if (r) {
		log += fmt::format(",uv_read_start={}, {}", r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);

		uv_close(reinterpret_cast<uv_handle_t*>(client), nullptr);
		delete client;
		return;
	}

	log += fmt::format(",sessionid={}, client={:#08x}, fd={:#08x}", sessionid, (int)client, (int)fd);
	anuv::getlogger()->info(log);
}

void anWorker::alloc_buffer(uv_handle_t * handle, size_t suggested_size, uv_buf_t * buf)
{
	//std::string log = fmt::format("anWorker::alloc_buffer({:#08x}, {})", (int)handle, suggested_size);
	if (handle->type == UV_TCP) {
		//log += fmt::format(", handle->type == UV_TCP");
		anTcpSocket * client = reinterpret_cast<anTcpSocket*>(handle);
		if (client) {
			*buf = client->read_buffer_;
		}
	}

	if (handle->type == UV_NAMED_PIPE) {
		//log += fmt::format(", handle->type == UV_NAMED_PIPE");
		buf->base = (char *)malloc(suggested_size);
		buf->len = suggested_size;
	}
	//anuv::getlogger()->info(log);
}

void anWorker::on_read(uv_stream_t * socket, ssize_t nread, const uv_buf_t * buf)
{
	std::string log = fmt::format("anWorker::on_read({:#08x}, {})", (int)socket, nread);
	anuv::getlogger()->info(log);

	anTcpSocket * client = reinterpret_cast<anTcpSocket*>(socket);
	if (nread < 0) {
		switch (nread) {
		case UV_EOF:	//主动断开
			if (!uv_is_closing((uv_handle_t*)socket)) {
				uv_close(reinterpret_cast<uv_handle_t*>(socket), anWorker::on_close);
			}
			break;
		case UV_ECONNRESET:	//异常断开
			if (!uv_is_closing((uv_handle_t*)socket)) {
				uv_close(reinterpret_cast<uv_handle_t*>(socket), anWorker::on_close);
			}
			break;
		default:
			break;
		}

	}
	else if (0 == nread) {

	}
	else if (nread > 0) {
		client->push_data(buf->base, nread);

		anWorker *server = client->get_Server();
		server->push_work(client);
	}

	anuv::getlogger()->info(log);
}

void anWorker::on_close(uv_handle_t * handle)
{
	std::string log = fmt::format("anWorker::on_close({:#08x}), ", (int)handle);

	if (UV_ASYNC == handle->type) {

	}
	else if (UV_TCP == handle->type) {
		//从连接中清除
		anTcpSocket * socket = reinterpret_cast<anTcpSocket *>(handle);
		//anTcpServer * server = reinterpret_cast<anTcpServer *>(handle->data);

		log += fmt::format("clear_session(sessionid={}, client={:#08x})", socket->sessionID_, (int)handle);

		//通知退出
		socket->get_Server()->clear_session(socket->sessionID_);
	}

	anuv::getlogger()->info(log);
}

void anWorker::on_walk(uv_handle_t * handle, void * arg)
{
	if (!uv_is_closing(handle)) {
		uv_close(handle, nullptr);
	}
}

void anWorker::on_signal(uv_signal_t * handle, int signum)
{
	std::string log = fmt::format("anWorker::on_signal({:#08x}, {}), ", (int)handle, signum);
	int r = 0;
	anWorker * that = reinterpret_cast<anWorker *>(handle->data);

	//shutdown
	//r = that->shutdown();
	//
	uv_stop(that->uv_loop_);

	uv_close((uv_handle_t*)&that->uv_server_, nullptr);

	uv_signal_stop(handle);

	anuv::getlogger()->info(log);
}

/*
void anWorker::on_shutdown(uv_shutdown_t * req, int status)
{
	std::string log = fmt::format("anWorker::on_shutdown({:#08x}, {}), ", (int)req, status);
	int r = 0;
	anTcpServer * that = reinterpret_cast<anTcpServer *>(req->data);

	//
	uv_stop(that->uv_loop_);

	delete req;

	anuv::getlogger()->info(log);

}
*/

void anWorker::clear_session(const int serssionid)
{
	auto it = client_lists_.find(serssionid);
	if (it != client_lists_.end()) {
		delete it->second;

		client_lists_.erase(it);
	}
}
