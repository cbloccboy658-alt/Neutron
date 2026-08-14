#ifndef ARRAYQUEUE_HPP
#define ARRAYQUEUE_HPP

#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <cstdlib>

namespace shibatch {
  template<typename T>
  class ArrayQueue {
    std::deque<std::vector<T>> queue;
    size_t pos = 0, sumsize = 0;

  public:
    size_t size() { return sumsize - pos; }

    void write(std::vector<T> &&v) {
      sumsize += v.size();
      queue.push_back(std::move(v));
    }

    void write(T *ptr, size_t n) {
      std::vector<T> v(n);
      memcpy(v.data(), ptr, n * sizeof(T));
      write(std::move(v));
    }

    size_t read(T *ptr, size_t n) {
      size_t s = std::min(size(), n);

      for(size_t r = s;r > 0;) {
	size_t cs = std::min(queue.front().size() - pos, r);
	memcpy(ptr, queue.front().data() + pos, cs * sizeof(T));
	pos += cs;
	ptr += cs;
	r -= cs;
	if (pos >= queue.front().size()) {
	  sumsize -= queue.front().size();
	  queue.pop_front();
	  pos = 0;
	}
      }

      return s;
    }
  };

  template<typename T>
  class BlockingArrayQueue {
    const size_t capacity;
    bool closed = false;
    ArrayQueue<T> aq;
    std::mutex mtx;
    std::condition_variable condVar;

  public:
    BlockingArrayQueue(size_t c) : capacity(c) {}

    size_t size() { return aq.size(); }

    void close() {
      std::unique_lock lock(mtx);
      closed = true;
      condVar.notify_all();
    }

    void write(std::vector<T> &&v) {
      std::unique_lock lock(mtx);
      while(size() >= capacity && !closed) condVar.wait(lock);
      if (closed) return;
      condVar.notify_all();
      aq.write(std::move(v));
    }

    size_t write(T *ptr, size_t n) {
      std::unique_lock lock(mtx);
      while(size() >= capacity && !closed) condVar.wait(lock);
      if (closed) return 0;
      size_t z = capacity - size();
      aq.write(ptr, z);
      condVar.notify_all();
      return z;
    }

    size_t read(T *ptr, size_t n) {
      std::unique_lock lock(mtx);
      while(size() == 0 && !closed) condVar.wait(lock);
      condVar.notify_all();
      return aq.read(ptr, n);
    }
  };
}
#endif //#ifndef ARRAYQUEUE_HPP
