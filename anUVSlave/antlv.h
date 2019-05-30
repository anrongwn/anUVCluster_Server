#pragma once
#include <vector>

namespace antlv {
	/*TLV:
	 * T type:      unsigned short
	 * L lenght:    unsigned int
	 * V value:     char *
	  */


	//包长度
	using u_len = union {
		char a[sizeof(size_t)];
		size_t x;
	};

	enum class package_type : unsigned short {
		heart_beat = 0xff00,
		cmd_requst,
		cmd_response,
		unknow,
	};


	using antlv_type = unsigned short;
	using antlv_length = size_t;
	using antlv_value = char *;

	using antlv_buffer = std::vector<char>;

	//编译期能得到的长度
	constexpr size_t head_size() noexcept {
		return (sizeof(antlv_type) + sizeof(antlv_length));
	}

	static antlv_buffer make_heartbeat_package() {
		static char data[head_size()] = { 0x00 };

		antlv_type type = static_cast<unsigned short>(package_type::heart_beat);
		memcpy(data, &type, sizeof(type));
		antlv_length length = 0;
		memcpy(data + sizeof(type), &length, sizeof(length));

		antlv_buffer cmd;
		cmd.assign(data, data + head_size());
		return cmd;
	}

	
	static antlv_buffer make_cmd_package(const char *v, size_t l) {

		antlv_buffer cmd(head_size() + l, 0x00);

		antlv_type type = static_cast<unsigned short>(package_type::cmd_requst);
		memcpy(cmd.data(), &type, sizeof(type));
		antlv_length length = l;
		memcpy(cmd.data() + sizeof(type), &length, sizeof(length));

		memcpy(cmd.data() + sizeof(type) + sizeof(length), v, l);

		return cmd;
	}

	
	static unsigned short parse_package(antlv_buffer& package, antlv_buffer& data) {
		antlv_type type = static_cast<unsigned short>(package_type::unknow);
		antlv_length length = 0;

		if (package.size() >= sizeof(type)) {
			memcpy(&type, package.data(), sizeof(type));

			if (static_cast<unsigned short>(package_type::heart_beat) == type) {
				package.erase(package.begin(), package.begin()+(sizeof(type) + sizeof(length)) );

				//
				antlv_buffer tmp = antlv::make_heartbeat_package();
				data.assign(tmp.begin(), tmp.end());
			}

			if (static_cast<unsigned short>(package_type::cmd_requst) == type) {
				if (package.size() >= (sizeof(type) + sizeof(length))) {
					memcpy(&length, package.data() + sizeof(type), sizeof(length));

					if (package.size() >= (sizeof(type) + sizeof(length) + length)) {
						antlv_buffer tmp;
						tmp.assign(package.data() + sizeof(type) + sizeof(length), package.data() + sizeof(type) + sizeof(length) + length);
						data.insert(data.end(), tmp.begin(), tmp.end());

						package.erase(package.begin(), package.begin() + (sizeof(type) + sizeof(length) + length));
					}
				}
				else {
					type = static_cast<unsigned short>(package_type::unknow);
				}
			}
		}

		return type;
	}	
	
}