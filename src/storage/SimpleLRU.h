#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {
public:
    SimpleLRU(size_t max_size = 1024) : _max_size(max_size), _in_use_size(0), _lru_head(nullptr), _lru_tail(nullptr) {}

    ~SimpleLRU() {
        _lru_index.clear();
        auto p = _lru_head;

        while(p != nullptr) {
            auto tmp = p->next;
            delete p;
            p = tmp;
        }
        // _lru_head = nullptr; // TODO: Here is stack overflow
    }

    // Implements Afina::Storage interface

    bool Put(const std::string &key, const std::string &value) override;

    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    bool Set(const std::string &key, const std::string &value) override;

    bool Delete(const std::string &key) override;

    bool Get(const std::string &key, std::string &value) override;

private:
    // LRU cache node
    using lru_node = struct lru_node {
        std::string key;
        std::string value;
        lru_node *prev;
        lru_node *next;

        lru_node(const std::string &k, const std::string &v) : prev(nullptr), next(nullptr), key(k), value(v) {}

        ~lru_node() {}
    };

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;
    std::size_t _in_use_size;

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    lru_node *_lru_head;
    lru_node *_lru_tail;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>> _lru_index;

    std::size_t FreeSize() const;
    std::size_t SizeOf(const std::string &key, const std::string &value) const;

    bool IsKeyExists(const std::string &key) const;
    void PutToTail(lru_node *node);
    void MoveToTail(lru_node *node);
    void DeleteFromHeadForSize(const std::size_t &size);
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
