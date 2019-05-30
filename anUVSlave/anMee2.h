#pragma once
#include "uv.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include "antlv.h"

class anTcpSocket;

class anMee2
{
	using echo_type = std::vector< antlv::antlv_buffer>;
private:
	struct an_work_req :public uv_work_t {
		explicit an_work_req(anTcpSocket *socket) {
			this->data = socket;
		}
		echo_type echo_result ;

		void set_result(echo_type&& res) {
			echo_result = std::forward<echo_type>(res);
		}


		an_work_req() = delete;
	};

public:
	explicit anMee2(uv_loop_t * loop);
	~anMee2();

	anMee2(const anMee2&) = delete;
	anMee2& operator=(const anMee2&) = delete;
	anMee2(anMee2&&) = delete;
	anMee2& operator=(anMee2&&) = delete;
public:
	int start();
	int stop();

	int push_work(anTcpSocket* socket);
private:
	//static void thread_func(void * lp);
	//static void close_cb(uv_handle_t* handle);
	//static void on_walk(uv_handle_t* handle, void* arg);

	static void on_work(uv_work_t* req);
	static void on_after_work(uv_work_t* req, int status);

private:
	using mx_lock = std::lock_guard<std::mutex>;

	//°ü³¤¶È
	using u_len = union {
		char a[sizeof(size_t)];
		size_t x;
	};
private:
	//std::mutex work_mtx_;
	//std::atomic_bool flag_ = { ATOMIC_FLAG_INIT };
	//uv_thread_t engine_ = { nullptr };
	uv_loop_t * loop_ = {nullptr};

};

