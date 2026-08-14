#ifndef OBJECTCACHE_HPP
#define OBJECTCACHE_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <sleef.h>
#include <sleefdft.h>

namespace ssrc {
  template<typename T>
  class ObjectCache {
    struct Internal {
      std::unordered_map<std::string, std::shared_ptr<T>> theCache;
      std::mutex mtx;
    };
    static Internal internal;
  public:
    static size_t count(const std::string &key) {
      std::unique_lock lock(internal.mtx);
      return internal.theCache.count(key);
    }

    static std::shared_ptr<T> at(const std::string &key) {
      std::unique_lock lock(internal.mtx);
      if (internal.theCache.count(key) == 0) return nullptr;
      return internal.theCache.at(key);
    }

    static void insert(const std::string &key, std::shared_ptr<T> value) {
      std::unique_lock lock(internal.mtx);
      internal.theCache[key] = value;
    }

    static void erase(const std::string &key) {
      std::unique_lock lock(internal.mtx);
      internal.theCache.erase(key);
    }
  };

  template<typename T> ObjectCache<T>::Internal ssrc::ObjectCache<T>::internal;

  namespace {
    template<typename T, typename std::enable_if<(std::is_same<T, double>::value), int>::type = 0>
    SleefDFT *SleefDFT_init(uint64_t mode, uint32_t n) {
      return SleefDFT_double_init1d(n, NULL, NULL, mode);
    }

    template<typename T, typename std::enable_if<(std::is_same<T, float>::value), int>::type = 0>
    SleefDFT *SleefDFT_init(uint64_t mode, uint32_t n) {
      return SleefDFT_float_init1d(n, NULL, NULL, mode);
    }
  }

  template<typename T, typename std::enable_if<(std::is_same<T, double>::value || std::is_same<T, float>::value), int>::type = 0>
  std::shared_ptr<SleefDFT> constructSleefDFT(uint64_t mode, uint32_t n) {
    std::string key = "SleefDFT<" + std::string(typeid(T).name()) + ">(" + std::to_string(mode) + ", " + std::to_string(n) + ")";
    std::shared_ptr<SleefDFT> ret = ObjectCache<SleefDFT>::at(key);

    if (!ret) {
      ret = std::shared_ptr<SleefDFT>(SleefDFT_init<T>(mode, n), SleefDFT_dispose);;
      ObjectCache<SleefDFT>::insert(key, ret);
    }

    return ret;
  }
}
#endif //#ifndef OBJECTCACHE_HPP
