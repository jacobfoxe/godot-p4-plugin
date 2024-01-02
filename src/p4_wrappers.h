#pragma once

#include <memory>

#include "godot_cpp/variant/string.hpp"

class P4Plugin;

struct CString {
	char *data = nullptr;

	CString(const godot::String &string);
	~CString();
	CString() = delete;
	CString(CString &&) = delete;
	CString &operator=(const CString &) = delete;
	CString &operator=(CString &&) = delete;
};