#include "pch.h"
#include "anTcpSocket.h"
#include <chrono>
#include <string>


anTcpSocket::anTcpSocket(const int sessionid) : sessionID_(sessionid)
{
	read_buffer_ = uv_buf_init((char*)malloc(AN_MAX_BUFFER_SIZE), AN_MAX_BUFFER_SIZE);
}


anTcpSocket::~anTcpSocket()
{
	if (read_buffer_.base) {
		free(read_buffer_.base);
		read_buffer_.len = 0;
		read_buffer_.base = nullptr;
	}
}

void anTcpSocket::push_data(const char * data, const size_t len)
{
	mx_lock lk(mtx_);
	datas_.insert(datas_.end(), data, data + len);

}

void anTcpSocket::package_handler()
{
	mx_lock lk(mtx_);

	while (!datas_.empty()) {
		antlv::antlv_buffer data;
		antlv::parse_package(datas_, data);

		make_echo(data);

		//回写
		if (send_req(data)) {
			break;
		}
	}

}

std::vector<antlv::antlv_buffer> anTcpSocket::package_handler2()
{
	mx_lock lk(mtx_);
	std::vector<antlv::antlv_buffer> result;
	while (!datas_.empty()) {
		antlv::antlv_buffer data;
		antlv::parse_package(datas_, data);

		make_echo(data);

		result.emplace_back(data);
	}

	return result;
}

int anTcpSocket::send_req(const antlv::antlv_buffer & data)
{
	mx_lock lk(write_mtx_);

	int r = 0;
	std::string log = fmt::format("anTcpSocket::send_req(), ");

	if (uv_is_closing(reinterpret_cast<uv_handle_t*>(this))) {
		log += fmt::format("client={:#08x} is close.", (int)this);
		anuv::getlogger()->error(log);

		return 1;
	}

	an_async_req * req = new an_async_req(this);
	uv_buf_t tmp = uv_buf_init((char*)malloc(data.size()), data.size());
	memcpy(tmp.base, data.data(), tmp.len);

	req->set_buffer(&tmp);

	log += fmt::format("an_async_req={:#08x}", (int)req);

	r = uv_async_init(this->loop, req, anTcpSocket::on_send_notify);
	if (r) {
		log += fmt::format("uv_async_init({:#08x})={}, {}", (int)req, r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);
		return r;
	}

	r = uv_async_send(req);
	if (r) {
		log += fmt::format("uv_async_send({:#08x})={}, {}", (int)req, r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);
		return r;
	}

	anuv::getlogger()->info(log);

	return r;
}

int anTcpSocket::write_socket(char * data, size_t len)
{
	int r = 0;
	//std::string log = fmt::format("anTcpSocket::write_socket({}), ", std::string(data, len));
	std::string log = fmt::format("anTcpSocket::write_socket(), ");

	if (uv_is_closing((uv_handle_t*)this)) {
		log += fmt::format("client={:#08x} is close.", (int)this);
		anuv::getlogger()->error(log);

		//free 
		if (data) free(data);

		return 1;
	}

	an_write_req * req = new an_write_req(this);
	
	//req->set_buffer(data, len);
	uv_buf_t tmp = uv_buf_init((char*)malloc(len), len);
	memcpy(tmp.base, data, tmp.len);
	req->set_buffer(tmp.base, tmp.len);


	log += fmt::format("an_write_req={:#08x}", (int)req);
	r = uv_write(req, (uv_stream_t*)this, &req->buf, 1, anTcpSocket::on_write);
	if (r) {
		log += fmt::format("uv_write({:#08x})={}, {}", (int)req, r, anuv::getUVError_Info(r));
		anuv::getlogger()->error(log);
		return r;
	}

	anuv::getlogger()->debug(log);


	return r;
}

void anTcpSocket::make_echo(antlv::antlv_buffer & resp)
{
	SYSTEMTIME st = { 0x00 };
	char date_tmp[30] = { 0x00 };

	//得到当时间
	::GetLocalTime(&st);
	sprintf_s(date_tmp, " <<<< %04d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, \
		st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	resp.insert(resp.end(), date_tmp, date_tmp + 30);

}

void anTcpSocket::on_send_notify(uv_async_t * handle)
{
	an_async_req *req = static_cast<an_async_req*>(handle);
	anTcpSocket * that = static_cast<anTcpSocket*>(uv_handle_get_data((uv_handle_t*)req));

	that->write_socket(req->buf.base, req->buf.len);

	//
	uv_close((uv_handle_t*)handle, anTcpSocket::on_close);
}

void anTcpSocket::on_close(uv_handle_t * handle)
{
	std::string log = fmt::format("anTcpSocket::on_close({:#08x})", (int)handle);

	if (UV_ASYNC == handle->type) {
		delete handle;
	}
	
	anuv::getlogger()->debug(log);
}

void anTcpSocket::on_write(uv_write_t * req, int status)
{
	std::string log = fmt::format("anTcpSocket::on_write({:#08x}, {})", (int)req, status);

	an_write_req *handle = reinterpret_cast<an_write_req*>(req);
	if (handle->buf.base) {
		free(handle->buf.base);
	}

	delete handle;
	anuv::getlogger()->debug(log);
}
