#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    std::size_t size = SizeOf(key, value);

    if (size > _max_size) {
        return false;
    }

    if (size > FreeSize()) {
        DeleteFromHeadForSize(size); // push lru nodes from list
    }

    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {    // set if key exists
        return SetByIterator(it, value);
    }

    auto *node = new lru_node(key, value);
    PutToTail(node);

    _in_use_size += SizeOf(key, value);
    _lru_index.insert(
        std::make_pair(std::reference_wrapper<const std::string>(node->key), std::reference_wrapper<lru_node>(*node)));

    return true;
}

bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return Put(key, value);
    }

    return false;
}

bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) { // don't set if no such key
        return false;
    }

    return SetByIterator(it, value);
}

bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }

    auto &node = it->second.get();

    value = node.value;
    MoveToTail(&node);

    return true;
}

bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }

    auto &node = it->second.get();

    _lru_index.erase(it);
    _in_use_size -= SizeOf(node.key, node.value);

    if (_lru_head.get() == &node) {
        _lru_head = std::move(_lru_head->next);
        _lru_head->prev = nullptr;
    } else if (_lru_tail == &node) {
        _lru_tail = _lru_tail->prev;
        _lru_tail->next.reset();
    } else {
        auto tmp = std::move(node.prev->next);
        node.prev->next = std::move(tmp->next);
        node.next->prev = tmp->prev;
        tmp.reset();
    }

    return true;
}

std::size_t SimpleLRU::SizeOf(const std::string &key, const std::string &value) const {
    return key.size() + value.size();
}

std::size_t SimpleLRU::FreeSize() const { return _max_size - _in_use_size; }

bool SimpleLRU::SetByIterator(
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>::iterator it,
    const std::string &value) {
    auto &node = it->second.get();
    auto old_size = SizeOf(node.key, node.value);
    auto new_size = SizeOf(node.key, value);

    if (old_size > new_size) { // comparison of size_t vars
        _in_use_size -= old_size - new_size;
    } else {
        if (new_size - old_size > FreeSize()) {
            return false;
        }
        _in_use_size += new_size - old_size;
    }

    node.value = value;
    MoveToTail(&node);

    return true;
}

void SimpleLRU::PutToTail(lru_node *node) {
    if (_lru_tail == nullptr) { // empty list
        _lru_head = std::unique_ptr<lru_node>(node);
        _lru_tail = node;
    } else {
        node->prev = _lru_tail;
        _lru_tail->next.reset(node);
        _lru_tail = node;
    }
}

void SimpleLRU::MoveToTail(lru_node *node) {
    if (node == _lru_tail) { // already in tail
        return;
    }

    if (node == _lru_head.get()) {
        auto tmp = std::move(_lru_head->next);
        _lru_head->prev = _lru_tail;
        _lru_tail->next = std::move(_lru_head);
        _lru_tail = _lru_tail->next.get();

        _lru_head = std::move(tmp);
        _lru_head->prev = nullptr;

    } else {
        auto prev = node->prev;
        auto next = std::move(node->next);
        auto tmp = std::move(node->prev->next); // to save node

        next->prev = prev;
        prev->next = std::move(next);

        tmp->prev = _lru_tail;
        _lru_tail->next = std::move(tmp);
        _lru_tail = _lru_tail->next.get();
    }
}

void SimpleLRU::DeleteFromHeadForSize(const std::size_t &size) {
    while (size > FreeSize()) {
        if (_lru_head == nullptr) { // already empty list
            return;
        }

        _lru_index.erase(_lru_index.find(_lru_head->key));
        _in_use_size -= SizeOf(_lru_head->key, _lru_head->value);

        if (_lru_head->next == nullptr) { // it was last item, head == tail
            _lru_head.reset();
            _lru_tail = nullptr;
        } else {
            _lru_head = std::move(_lru_head->next);
            _lru_head->prev = nullptr;
        }
    }
}
} // namespace Backend
} // namespace Afina
