#include <kms++util/strhelpers.h>

#include <algorithm>
#include <stdarg.h>

using namespace std;

string to_lower(const string& str)
{
	string data = str;
	transform(data.begin(), data.end(), data.begin(), ::tolower);
	return data;
}
