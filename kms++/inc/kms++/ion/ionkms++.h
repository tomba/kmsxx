#pragma once

namespace kms
{
class Ion
{
public:
	Ion();
	Ion(const std::string& device);
	virtual ~Ion();

	Ion(const Ion& other) = delete;
	Ion& operator=(const Ion& other) = delete;

	int fd() const { return m_fd; }

private:
	int m_fd;
};
}
