#pragma once
#include "uv.h"
#include <map>
#include <atomic>

class anTcpSocket;
class anMee2;

class anWorker
{
public:
	anWorker();
	~anWorker();

	anWorker(const anWorker&) = delete;
	anWorker& operator=(const anWorker&) = delete;
	anWorker(anWorker&&);
	anWorker& operator=(anWorker&&) = delete;
public:
	int start();
	int run();
	int wait_exit();

	int push_work(anTcpSocket* socket);
private:
	int init();
	static void on_new_connection(uv_stream_t *q, ssize_t nread, const uv_buf_t *buf);
	static void alloc_buffer(uv_handle_t * handle, size_t suggested_size, uv_buf_t * buf);
	static void on_write(uv_write_t * req, int status);
	static void on_read(uv_stream_t *socket, ssize_t nread, const uv_buf_t *buf);
	static void on_notify(uv_async_t* handle);
	static void on_close(uv_handle_t* handle);
	static void on_walk(uv_handle_t* handle, void* arg);

	static void on_signal(uv_signal_t* handle, int signum);
	//static void on_shutdown(uv_shutdown_t* req, int status);

	static int get_sessionID() {
		static int sessionID = 0;
		return (++sessionID);
	}

	void clear_session(const int serssionid);

private:
	uv_pipe_t uv_server_ = { 0x00 };
	uv_loop_t * uv_loop_ = { nullptr };

	std::atomic_bool init_ = { ATOMIC_FLAG_INIT };
	std::map<int, anTcpSocket*> client_lists_;

	//std::unique_ptr<anMee> message_handler_;
	std::unique_ptr<anMee2> message_handler2_;

	uv_signal_t sig_;
};

