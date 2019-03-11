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
        mutex.lock();
        bool result = _simpleLRU->Put(key, value);
        mutex.unlock();
        return result;
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        mutex.lock();
        bool result = _simpleLRU->PutIfAbsent(key, value);
        mutex.unlock();
        return result;
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        mutex.lock();
        bool result = _simpleLRU->Set(key, value);
        mutex.unlock();
        return result;
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override {
        mutex.lock();
        bool result = _simpleLRU->Delete(key);
        mutex.unlock();
        return result;
        ;
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        mutex.lock();
        bool result = _simpleLRU->Get(key, value);
        mutex.unlock();
        return result;
    }

private:
    std::mutex mutex;
    std::unique_ptr<SimpleLRU> _simpleLRU;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
