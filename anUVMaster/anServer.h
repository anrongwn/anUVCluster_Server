#pragma once
#include "uv.h"
#include <atomic>

struct anWorker_t {
	uv_process_t req_;
	uv_process_options_t options_;
	uv_pipe_t pipe_;
};
using anWorker_handle = struct anWorker_t;

class anServer
{
public:
	anServer();
	~anServer();

	anServer(const anServer&) = delete;
	anServer& operator=(const anServer&) = delete;
	anServer(anServer&&) = delete;
	anServer& operator=(anServer&&) = delete;

	int start(const char* addr, const unsigned short port);
	int run();
	int wait_exit();
private:
	struct an_write_req : public uv_write_t {
		uv_buf_t buf;

		explicit an_write_req(anServer* p) {
			this->data = p;

			buf = uv_buf_init(const_cast<char*>("."), 1);
		}
		an_write_req() = delete;
	};
private:
	int init();
	int setup_workers();
	int kill_workers();

	static void on_new_connection(uv_stream_t* server, int status);
	static void alloc_buffer(uv_handle_t * handle, size_t suggested_size, uv_buf_t * buf);
	static void on_write(uv_write_t * req, int status);
	//static void on_read(uv_stream_t *socket, ssize_t nread, const uv_buf_t *buf);
	//static void on_notify(uv_async_t* handle);
	static void on_close(uv_handle_t* handle);
	static void on_walk(uv_handle_t* handle, void* arg);

	static void on_signal(uv_signal_t* handle, int signum);
	static void close_process_handle(uv_process_t *req, int64_t exit_status, int term_signal);

	anWorker_handle * get_server_worker(){
		static int counter = 0;

		counter = ((counter + 1) % worker_count_);

		return &workers_[counter];
	}
private:
	anWorker_handle * workers_ = { nullptr };
	int worker_count_ = 4;//cpu Êý

	uv_loop_t *loop_ = { nullptr };
	uv_tcp_t server_;
	uv_signal_t sig_;

	std::atomic_bool init_ = { ATOMIC_FLAG_INIT };
	
	
};

