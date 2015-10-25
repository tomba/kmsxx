#pragma once

#include <string>
#include <vector>
#include <functional>

class Option
{
	friend class OptionSet;
public:
	Option(const std::string& str, std::function<void()> func);
	Option(const std::string& str, std::function<void(const std::string)> func);

private:
	void parse(const std::string& str);

	char m_short;
	std::string m_long;
	int m_has_arg;
	std::function<void()> m_void_func;
	std::function<void(const std::string)> m_func;
};

class OptionSet
{
public:
	OptionSet(std::initializer_list<Option> il);

	void parse(int argc, char** argv);

	const std::vector<std::string> params() const { return m_params; }

private:
	const Option& find_opt(int c);

	const std::vector<Option> m_opts;
	std::vector<std::string> m_params;
};
