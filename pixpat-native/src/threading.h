#pragma once

#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <exception>
#include <thread>
#include <vector>

namespace pixpat
{

inline unsigned default_thread_count()
{
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	if (n < 1)
		return 1;
	// Cap to keep per-stripe work meaningful and avoid heavy
	// oversubscription on large NUMA hosts.
	if (n > 16)
		n = 16;
	return static_cast<unsigned>(n);
}

/*
 * Run `fn(start_y, end_y)` over `[0, height)` partitioned into stripes
 * aligned to `v_sub`. Half-open ranges, matching the `for (by = 0;
 * by < H; by += bh)` block-loop style.
 *
 * `fn` must be callable as `void(size_t start_y, size_t end_y)` and is
 * invoked concurrently from multiple threads — it must be safe to call
 * with disjoint Y-ranges in parallel. Exceptions thrown from a worker
 * are captured and the first (by stripe index) is rethrown after all
 * workers join.
 *
 * When `n_threads <= 1`, `fn` is called inline on the calling thread —
 * no `std::thread` is spawned, no allocation occurs.
 */
template<typename F>
void run_stripes(size_t height, unsigned v_sub, unsigned n_threads, F&& fn)
{
	if (height == 0 || v_sub == 0)
		return;

	// Callers (pixpat_convert / pixpat_draw_pattern) validate divisibility
	// at the entry point.
	assert(height % v_sub == 0);

	const size_t max_useful = height / v_sub;
	if (n_threads == 0)
		n_threads = 1;
	if (static_cast<size_t>(n_threads) > max_useful)
		n_threads = static_cast<unsigned>(max_useful);

	if (n_threads <= 1) {
		fn(size_t{ 0 }, height);
		return;
	}

	// Stripe height rounded up to v_sub; last stripe absorbs the
	// remainder.
	size_t part_height = (height + n_threads - 1) / n_threads;
	part_height = (part_height + v_sub - 1) / v_sub * v_sub;

	std::vector<std::exception_ptr> errors(n_threads);
	std::vector<std::thread> workers;
	workers.reserve(n_threads);

	for (unsigned i = 0; i < n_threads; i++) {
		size_t start = i * part_height;
		if (start >= height)
			break;
		size_t end = start + part_height;
		if (i == n_threads - 1 || end > height)
			end = height;

		workers.emplace_back([&, i, start, end] {
				try {
					fn(start, end);
				} catch (...) {
					errors[i] = std::current_exception();
				}
			});
	}

	for (auto& t : workers)
		t.join();

	for (auto& e : errors)
		if (e)
			std::rethrow_exception(e);
}

} // namespace pixpat
