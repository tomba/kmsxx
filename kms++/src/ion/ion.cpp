#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <kms++/kms++.h>
#include "../../inc/kms++/ion/ionkms++.h"

#include "ion.h"

using namespace std;

namespace kms
{

Ion::Ion()
	: Ion("/dev/ion")
{
}

Ion::Ion(const std::string& device)
{
	int fd = open(device.c_str(), O_RDWR);
	if (fd < 0)
		throw invalid_argument(string(strerror(errno)) + " opening " + device);
	m_fd = fd;
}

Ion::~Ion()
{
	close(m_fd);
}

}
