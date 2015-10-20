#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <map>

using namespace std;

#define NO_PARAM(h) (CmdOption(false, h))
#define HAS_PARAM(h) (CmdOption(true, h))

class CmdOption
{
public:
	CmdOption(bool has_param, string help) :
	        m_has_param(has_param), m_help(help), m_is_set(false) { }
        bool has_param() const { return m_has_param; }
	const string& help() const { return m_help; }

	void oset() { m_is_set = true; }
	void pset(const string& p) { m_param = p; oset(); }
        bool is_set() const { return m_is_set; }
        const string& param() const { return m_param; }
private:
	bool m_has_param;
	string m_help;

	bool m_is_set;
	string m_param;
};

class CmdOptions
{
public:
	CmdOptions(int argc, char **argv, map<string, CmdOption>& opts) :
		m_opts(opts), m_cmd(argv[0]) {
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				auto ii = m_opts.find(&argv[i][1]);
				if (ii == m_opts.end()) {
					m_error += m_cmd + ": " +
						string(argv[i]) +
						": unknown option\n";
					continue;
				}
				if ((*ii).second.has_param()) {
					if (++i == argc) {
						m_error += m_cmd + ": -" +
							(*ii).first +
							": parameter missing\n";
						continue;
					}
					(*ii).second.pset(argv[i]);
				} else {
					(*ii).second.oset();
				}
			} else {
				m_params.push_back(argv[i]);
			}
		}
	}
	const string& error() const { return m_error; }
	const string& cmd() const { return m_cmd; }

	bool is_set(const string& name) const {
		return m_opts.at(name).is_set();
	}
	const string& opt_param(const string& name) const {
		return m_opts.at(name).param();
	}
	const vector<string>& params() const { return m_params; }
	CmdOption& get_option(const string& name) { return m_opts.at(name); }

	int num_options() const {
		int ret(0);
		for (const auto& p : m_opts)
			if (p.second.is_set())
				ret++;
		return ret;
	}

	const string usage() const {
		string ret("usage:\n");
		for (const auto& p : m_opts)
			ret += "-" + p.first +
				(p.second.has_param() ? " <>: " : ": ") +
				p.second.help() + "\n";
		return ret;
	}

private:
	map<string, CmdOption>& m_opts;
	string m_cmd;
	vector<string> m_params;
	string m_error;
};
