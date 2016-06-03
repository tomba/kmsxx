#include "kms++util.h"
#include "strhelpers.h"

using namespace std;

namespace kms {

Connector* resolve_connector(Card& card, const string& str)
{
	if (str.length() == 0)
		return nullptr;

	auto connectors = card.get_connectors();

	if (str[0] == '@') {
		char* endptr;
		unsigned id = strtoul(str.c_str() + 1, &endptr, 10);
		if (*endptr == 0) {
			Connector* c = card.get_connector(id);
			if (!c)
				return nullptr;
			else
				return c;
		}
	} else {
		char* endptr;
		unsigned idx = strtoul(str.c_str(), &endptr, 10);
		if (*endptr == 0) {
			if (idx >= connectors.size())
				return nullptr;
			else
				return connectors[idx];
		}
	}

	for (Connector* conn : connectors) {
		if (to_lower(conn->fullname()).find(to_lower(str)) != string::npos)
			return conn;
	}

	return nullptr;
}

}
