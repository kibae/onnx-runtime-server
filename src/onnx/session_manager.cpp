//
// Created by Kibae Shin on 2023/09/01.
//

#include "../onnxruntime_server.hpp"

Orts::onnx::session_manager::session_manager(model_bin_getter_t model_bin_getter) : model_bin_getter(model_bin_getter) {
	assert(model_bin_getter != nullptr);
}

Orts::onnx::session_manager::~session_manager() {
}

std::shared_ptr<Orts::onnx::session>
Orts::onnx::session_manager::get_session(const std::string &model_name, const std::string &model_version) {
	auto key = session_key(model_name, model_version);
	return get_session(key);
}

std::shared_ptr<Orts::onnx::session> Orts::onnx::session_manager::get_session(const Orts::onnx::session_key &key) {
	std::lock_guard<std::recursive_mutex> lock(mutex);
	auto it = sessions.find(key);
	if (it == sessions.end())
		return nullptr;
	return it->second;
}

std::shared_ptr<Orts::onnx::session> Orts::onnx::session_manager::create_session(
	const std::string &model_name, const std::string &model_version, const json &option, const char *model_data,
	size_t model_data_length
) {
	auto key = session_key(model_name, model_version);

	std::shared_ptr<Orts::onnx::session> session = nullptr;
	std::lock_guard<std::recursive_mutex> lock(mutex);

	auto current_session = get_session(key);
	if (current_session != nullptr)
		throw conflict_error("session already exists");

	if (model_data != nullptr && model_data_length > 0) {
		session = std::make_shared<onnx::session>(key, model_data, model_data_length, option);
	} else if (option.contains("path") && option["path"].is_string()) {
		session = std::make_shared<onnx::session>(key, option["path"].get<std::string>(), option);
	} else {
		auto model_bin = model_bin_getter(model_name, model_version);
		model_data = model_bin.data();
		model_data_length = model_bin.size();
		session = std::make_shared<onnx::session>(key, model_data, model_data_length, option);
	}
	sessions.emplace(key, session);
	return session;
}

void Orts::onnx::session_manager::remove_session(const std::string &model_name, const std::string &model_version) {
	auto key = session_key(model_name, model_version);
	remove_session(key);
}

void Orts::onnx::session_manager::remove_session(const Orts::onnx::session_key &key) {
	std::lock_guard<std::recursive_mutex> lock(mutex);
	auto it = sessions.find(key);
	if (it == sessions.end()) {
		throw not_found_error("session not found");
	}
	sessions.erase(it);
}
