#ifndef BLOCKINGQUEUE_HPP
#define BLOCKINGQUEUE_HPP

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace shibatch {
  /**
   * @brief A class that implements a thread-safe queue. pop() method
   * can be used to retrieve an item from the queue, and this method
   * blocks until an item becomes available.
   */
  template <typename T> class BlockingQueue {
    std::queue<T> que;
    std::mutex mtx;
    std::condition_variable condVar;
  public:
    bool empty() {
      std::unique_lock lock(mtx);
      return que.empty();
    }

    size_t size() {
      std::unique_lock lock(mtx);
      return que.size();
    }

    void push(const T& val) {
      std::unique_lock lock(mtx);
      que.push(val);
      if (que.size() == 1) condVar.notify_all();
    }

    void push(T&& val) {
      std::unique_lock lock(mtx);
      que.push(std::move(val));
      if (que.size() == 1) condVar.notify_all();
    }

    /** @brief This method blocks until an item becomes available. */
    T pop() {
      std::unique_lock lock(mtx);
      while(que.empty()) condVar.wait(lock);
      T ret = std::move(que.front());
      que.pop();
      return ret;
    }

    void clear() {
      std::unique_lock lock(mtx);
      while(!que.empty()) que.pop();
    }

    BlockingQueue() {}
    BlockingQueue(const BlockingQueue &p) = delete;
    BlockingQueue(BlockingQueue &p) = delete;
    BlockingQueue& operator=(const BlockingQueue &) = delete;
  };
}
#endif // #ifndef BLOCKINGQUEUE_HPP
