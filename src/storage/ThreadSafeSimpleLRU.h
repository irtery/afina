#ifndef AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
#define AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H

#include <map>
#include <mutex>
#include <string>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

/**
 * # SimpleLRU thread safe version
 *
 *
 */
class ThreadSafeSimplLRU : public Afina::Storage {
public:
    explicit ThreadSafeSimplLRU(size_t max_size = 1024) {
        _simpleLRU = std::unique_ptr<SimpleLRU>(new SimpleLRU(max_size));
    }
    ~ThreadSafeSimplLRU() override = default;

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> lock(mutex);
        return _simpleLRU->Put(key, value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> lock(mutex);
        return _simpleLRU->PutIfAbsent(key, value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> lock(mutex);
        return _simpleLRU->Set(key, value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override {
        std::lock_guard<std::mutex> lock(mutex);
        return _simpleLRU->Delete(key);
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        std::lock_guard<std::mutex> lock(mutex);
        sleep(60);
        return _simpleLRU->Get(key, value);
    }

private:
    std::mutex mutex;
    std::unique_ptr<SimpleLRU> _simpleLRU;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
