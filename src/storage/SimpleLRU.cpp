#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    std::size_t size = SizeOf(key, value);

    if(size > _max_size) {
        return false;
    }

    if(IsKeyExists(key)) {
        return Set(key, value);
    }

    if(size > FreeSize()) {
        DeleteFromHeadForSize(size);       // push lru nodes from list
    }

    auto *node = new lru_node;
    node->key = key;
    node->value = value;

    PutToTail(node);
    _in_use_size += SizeOf(key, value);
    _lru_index.insert(std::make_pair(std::reference_wrapper<const std::string>(node->key),
                                     std::reference_wrapper<lru_node>(*node)));

    return true;
}

bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if(!IsKeyExists((key))) {
        return Put(key, value);
    }

    return false;
}

bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if(!IsKeyExists(key)) { // don't set if no such key
        return false;
    }

    auto it = _lru_index.find(key);
    auto &node = it->second.get();
    auto old_size = SizeOf(node.key, node.value), diff = old_size - SizeOf(key, value);

    if(diff > 0){
        _in_use_size -= diff;
    } else {
        if(diff > FreeSize()) {
            return false;
        }
        _in_use_size += diff;
    }

    node.value = value;
    MoveToTail(&node);

    return true;
}

bool SimpleLRU::Delete(const std::string &key) { return false; }

bool SimpleLRU::Get(const std::string &key, std::string &value) {
    if(!IsKeyExists(key)) {
        return false;
    }

    auto it = _lru_index.find(key);
    auto &node = it->second.get();

    value = node.value;
    MoveToTail(&node);

    return true;
}

std::size_t SimpleLRU::SizeOf(const std::string &key, const std::string &value) const {
    return key.size() + value.size();
}

std::size_t SimpleLRU::FreeSize() const {
    return _max_size - _in_use_size;
}

bool SimpleLRU::IsKeyExists(const std::string &key) const {
    auto it = _lru_index.find(key);
    return it != _lru_index.end();
}

void SimpleLRU::PutToTail(lru_node *node) {
    if(_lru_tail == nullptr) { // empty list
        _lru_head = _lru_tail = node;
    } else {
        node->next = nullptr;
        node->prev = _lru_tail;
        _lru_tail->next = node;
        _lru_tail = node;
    }
}

void SimpleLRU::MoveToTail(lru_node *node) {
    if(node == _lru_tail) { // already in tail
        return;
    }

    if(node == _lru_head) {
        _lru_head = _lru_head->next;
        _lru_head->prev = nullptr;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    PutToTail(node);
}

void SimpleLRU::DeleteFromHeadForSize(const std::size_t &size) {
    while(size > FreeSize()) {
        if(_lru_head == nullptr) { // already empty list
            return;
        }

        _lru_index.erase(_lru_index.find(_lru_head->key));
        _in_use_size -= SizeOf(_lru_head->key, _lru_head->value);

        if(_lru_head->next == nullptr) { // it was last item, head == tail
            _lru_head = _lru_tail = nullptr;
        } else {
            delete _lru_head;
            _lru_head = _lru_head->next;
            _lru_head->prev = nullptr;
        }
    }
}
} // namespace Backend
} // namespace Afina
