#include <chrono>

class Stopwatch
{
public:
	void start()
	{
		m_start = std::chrono::steady_clock::now();
	}

	double elapsed_s() const
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
	}

	double elapsed_ms() const
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_start).count();
	}

	double elapsed_us() const
	{
		return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - m_start).count();
	}

private:
	std::chrono::steady_clock::time_point m_start;
};
