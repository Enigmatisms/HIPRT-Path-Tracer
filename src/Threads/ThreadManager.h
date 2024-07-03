/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

/**
 * Singleton class so that threads are accessible everywhere to be .join()
 * whenever we want without having to pass them around in function calls etc...
 * 
 * This class works by creating threads and storing them in std::vectors.
 * Which std::vector is the thread going to be stored in depends on the key that is given.
 * Keys are basically used to give some kind of "name" to threads. The main use for that
 * is that all threads with the same key can be joined at the same time. So for example,
 * if you add 2 threads, both with the key 'MY_THREAD_KEY', they will both be added to
 * the same std::vector (these std::vectors are in the thread_map which is an attribute
 * of this class). Then, when you decide to join threads with the 'MY_THREAD_KEY' key,
 * all threads of the corresponding std::vector will be .join()
 */
class ThreadManager
{
public:
	static std::string COMPILE_KERNEL_THREAD_KEY;
	static std::string TEXTURE_THREADS_KEY;

	static ThreadManager& instance();

	template <typename T>
	static void add_state(const std::string& key, std::shared_ptr<T> state)
	{
		m_threads_states[key] = std::static_pointer_cast<void>(state);
	}

	template <class _Fn, class... _Args>
	static void start_thread(std::string key, _Fn function, _Args... args)
	{
		// We're going to start a thread and store its handle in the map that corresponds to the given key

		auto find = m_threads_map.find(key);
		if (find == m_threads_map.end())
			// TODO May not be necessary at all with the [] operator
			// If the list of threads handles for the given key doesn't exist yet, create it
			m_threads_map[key] = std::vector<std::thread>();

		std::vector<std::thread>& threads_vec = m_threads_map.find(key)->second;

		// Starting the thread and adding it to the list of threads for the given key
		threads_vec.push_back(std::thread(function, args...));
	}

	/**
	 * This function starts a thread on the main thread i.e. not asynchronously and waits for
	 * the completion of the given function before returning
	 */
	template <class _Fn, class... _Args>
	static void start_serial_thread(std::string key, _Fn function, _Args... args)
	{
		function(args...);
	}

	static void join_threads(std::string key);

private:
	// The states are used to keep the data that the threads need alive
	static std::unordered_map<std::string, std::shared_ptr<void>> m_threads_states;

	// The ThreadManager can hold as many thread as we want and to find the thread
	// we want amongst all the threads stored, we use keys, hence the unordered_map
	static std::unordered_map<std::string, std::vector<std::thread>> m_threads_map;
};

#endif
