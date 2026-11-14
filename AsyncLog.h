#pragma once

#include <iostream>
#include <queue>
#include <mutex>
#include <sstream>
#include <fstream>
#include <condition_variable>

template <typename T>
std::string to_string_helper(T&& str) {
	std::ostringstream oss{};
	oss << std::forward<T>(str);
	return oss.str();
}

class LogQueue {
public:
	void push(const std::string& str) {
		std::lock_guard<std::mutex> lock(_mutex);
		_queue.push(str);
		_cond_var.notify_one();
	}

	bool pop(std::string& str) {
		std::unique_lock<std::mutex> lock(_mutex);
		_cond_var.wait(lock, [this]() {
			return !_queue.empty() || _is_shutdown;
			});

		if (_queue.empty() || _is_shutdown) {
			return false;
		}

		str = _queue.front();
		_queue.pop();

		return true;
	}

	void shutdown() {
		std::lock_guard<std::mutex> lock(_mutex);
		_is_shutdown = true;
		_cond_var.notify_all();
	}

private:
	std::queue<std::string> _queue;
	std::mutex _mutex;
	std::condition_variable _cond_var;
	bool _is_shutdown = false;
};

class Logger {
public:
	Logger(const std::string& filename) : _log_file(filename, std::ios::out | std::ios::app) {
		_work_thread = std::thread([this]() {
			if (!_log_file.is_open()) {
				throw std::runtime_error("无法打开日志文件");
			}
			std::string str = "";
			while (_log_queue.pop(str)) {
				_log_file << str << "\n";
			}
			});
	}

	~Logger() {
		_log_queue.shutdown();
		if (_work_thread.joinable()) {
			_work_thread.join();
		}

		if (_log_file.is_open()) {
			_log_file.close();
		}
	}

	template <typename ...Args>
	void log(const std::string& format, Args&& ...args) {
		_log_queue.push(formatMessage(format, std::forward<Args>(args)...));
	}

private:
	template <typename ...Args> 
	std::string formatMessage(const std::string& format, Args&& ...args) {
		std::vector<std::string> arg_strings = { to_string_helper(std::forward<Args>(args))... };
		std::ostringstream oss{};
		std::size_t pos = 0;
		std::size_t arg_index = 0;
		std::size_t placeholder = format.find("{}", pos);
		while (placeholder != std::string::npos) {
			oss << format.substr(pos, placeholder - pos);
			if (arg_index < arg_strings.size()) {
				oss << arg_strings[arg_index++];
			}
			else {
				oss << "{}";
			}
			pos = placeholder + 2;
			placeholder = format.find("{}", pos);
		}

		oss << format.substr(pos);

		while (arg_index < arg_strings.size()) {
			oss << arg_strings[arg_index++];
		}
		return oss.str();
	}

private:
	std::ofstream _log_file;
	LogQueue _log_queue;
	std::thread _work_thread;
};