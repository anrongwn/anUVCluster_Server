#pragma once
#include "uv.h"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include "antlv.h"

#define AN_MAX_BUFFER_SIZE	(65536)


class anWorker;
class anTcpSocket :
	public uv_tcp_t
{
private:
	struct an_async_req : public uv_async_t {
		uv_buf_t buf;
		explicit an_async_req(anTcpSocket *p) {
			this->data = p;
			buf.base = nullptr;
			buf.len = 0;
		}

		char * set_buffer(const uv_buf_t* p) {
			if (p) {
				buf.base = p->base;
				buf.len = p->len;
			}

			return buf.base;
		}
		an_async_req() = delete;
		
		
	};

	struct an_write_req : public uv_write_t {
		uv_buf_t buf;

		explicit an_write_req(anTcpSocket* p) {
			this->data = p;
		}

		char * set_buffer(char *base, size_t len) {
			buf.base = base;
			buf.len = len;

			return buf.base;
		}


		an_write_req() = delete;
	};
public:
	explicit anTcpSocket(const int sessionid);
	~anTcpSocket();

	anTcpSocket() = delete;
	anTcpSocket(const anTcpSocket&) = delete;
	anTcpSocket& operator=(const anTcpSocket&) = delete;

	anTcpSocket(anTcpSocket&&) = delete;
	anTcpSocket& operator=(anTcpSocket&&) = delete;

	inline anWorker* get_Server() {
		return reinterpret_cast<anWorker*>(this->data);
	}
	inline void set_Server(anWorker* obj) {
		this->data = obj;
	}

	void push_data(const char *data, const size_t len);
	void package_handler();
	std::vector< antlv::antlv_buffer> package_handler2();

	int send_req(const antlv::antlv_buffer& data);
	int write_socket(char* data, size_t len);
private:
	using mx_lock = std::lock_guard<std::mutex>;
	void make_echo(antlv::antlv_buffer& resp);

	static void on_send_notify(uv_async_t* handle);
	static void on_close(uv_handle_t* handle);
	static void on_write(uv_write_t * req, int status);
public:
	int sessionID_;
	uv_buf_t read_buffer_;
	std::mutex mtx_;
	antlv::antlv_buffer datas_;

	std::mutex write_mtx_;
};

