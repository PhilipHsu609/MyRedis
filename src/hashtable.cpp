#include "hashtable.hpp"

#include <cstddef> // std::size_t
#include <utility> // std::move

namespace {
std::int8_t next_exp(std::size_t size);
} // namespace

HashTable::HashTable() {
    reset(0);
    reset(1);
}

HashTable::HashTable(HashTable &&other) noexcept
    : table(other.table), used(other.used), size_exp(other.size_exp),
      rehash_idx(other.rehash_idx), hash_fn(other.hash_fn) {
    other.reset(0);
    other.reset(1);
}

HashTable &HashTable::operator=(HashTable &&other) noexcept {
    table = other.table;
    used = other.used;
    size_exp = other.size_exp;
    rehash_idx = other.rehash_idx;
    hash_fn = other.hash_fn;

    other.reset(0);
    other.reset(1);

    return *this;
}

HashTable::~HashTable() {
    clear(0);
    clear(1);
}

void HashTable::set(std::string key, std::string value) {
    const std::size_t hash = hash_fn(key);
    auto idx = static_cast<std::int64_t>(hash & HT_MASK(size_exp[0]));

    try_rehash(idx);
    try_expand();

    // Find existing key
    for (std::size_t htidx = 0; htidx <= 1; htidx++) {
        if (htidx == 0 && idx < rehash_idx) {
            continue;
        }

        idx = static_cast<std::int64_t>(hash & HT_MASK(size_exp[htidx]));
        HashNode *node = table[htidx][idx];
        while (node != nullptr) {
            if (node->key == key || cmp(node->key, key)) {
                node->value = std::move(value);
                return;
            }
            node = node->next;
        }

        if (!is_rehashing()) {
            break;
        }
    }

    /* If the key is not found, add a new node.
       If we are during rehashing, always add new node at the new table. */
    HashNode **bucket = &table[is_rehashing() ? 1 : 0][idx];

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    *bucket = new HashNode{std::move(key), std::move(value), *bucket};
    used[is_rehashing() ? 1 : 0]++;
}

HashNode *HashTable::get(const std::string &key) {
    if (is_empty()) {
        return nullptr;
    }

    const std::size_t hash = hash_fn(key);
    auto idx = static_cast<std::int64_t>(hash & HT_MASK(size_exp[0]));
    try_rehash(idx);

    for (std::size_t htidx = 0; htidx <= 1; htidx++) {
        if (htidx == 0 && idx < rehash_idx) {
            continue;
        }

        idx = static_cast<std::int64_t>(hash & HT_MASK(size_exp[htidx]));
        HashNode *node = table[htidx][idx];
        while (node != nullptr) {
            if (node->key == key || cmp(node->key, key)) {
                return node;
            }
            node = node->next;
        }

        if (!is_rehashing()) {
            break;
        }
    }

    return nullptr;
}

bool HashTable::remove(const std::string &key) {
    if (is_empty()) {
        return false;
    }

    const std::size_t hash = hash_fn(key);
    auto idx = static_cast<std::int64_t>(hash & HT_MASK(size_exp[0]));

    try_rehash(idx);

    for (std::size_t htidx = 0; htidx <= 1; htidx++) {
        if (htidx == 0 && idx < rehash_idx) {
            continue;
        }

        idx = static_cast<std::int64_t>(hash & HT_MASK(size_exp[htidx]));
        HashNode **node = &table[htidx][idx];
        while (*node != nullptr) {
            if ((*node)->key == key || cmp((*node)->key, key)) {
                HashNode *next = (*node)->next;
                delete *node; // NOLINT(cppcoreguidelines-owning-memory)
                *node = next;
                used[htidx]--;
                return true;
            }
            node = &(*node)->next;
        }

        if (!is_rehashing()) {
            break;
        }
    }

    return false;
}

std::vector<std::string> HashTable::keys() {
    std::vector<std::string> buf;
    for (std::size_t htidx = 0; htidx <= 1; htidx++) {
        for (std::size_t idx = 0; idx < HT_SIZE(size_exp[htidx]); idx++) {
            HashNode *node = table[htidx][idx];
            while (node != nullptr) {
                buf.push_back(node->key);
                node = node->next;
            }
        }
    }

    return buf;
}

bool HashTable::is_empty() const { return used[0] + used[1] == 0; }

HTState HashTable::state(std::size_t htidx) const {
    return {used[htidx], HT_SIZE(size_exp[htidx]), size_exp[htidx]};
}

std::size_t HashTable::size() const { return used[0] + used[1]; }
std::size_t HashTable::buckets() const {
    return HT_SIZE(size_exp[0]) + HT_SIZE(size_exp[1]);
}

void HashTable::set_cmp(KeyCompare cmp) { this->cmp = std::move(cmp); }
void HashTable::set_hash(Hash fn) { hash_fn = std::move(fn); }

void HashTable::force_rehash() {
    while (is_rehashing()) {
        rehash_steps(100);
        check_rehash_complete();
    }
}

void HashTable::reset(std::size_t htidx) {
    table[htidx] = nullptr;
    used[htidx] = 0;
    size_exp[htidx] = -1;
}

void HashTable::clear(std::size_t htidx) {
    for (std::size_t idx = 0; idx < HT_SIZE(size_exp[htidx]); idx++) {
        HashNode *node = table[htidx][idx];
        while (node != nullptr) {
            HashNode *next = node->next;
            delete node; // NOLINT(cppcoreguidelines-owning-memory)
            node = next;
        }
    }
    delete[] table[htidx]; // NOLINT(cppcoreguidelines-owning-memory)
    reset(htidx);
}

bool HashTable::is_rehashing() const { return rehash_idx != -1; }

bool HashTable::try_expand() {
    if (HT_SIZE(size_exp[0]) == 0) {
        expand(HT_INIT_SIZE);
        return true;
    }

    // If the number of keys is more than the number of slots
    if (used[0] >= HT_SIZE(size_exp[0])) {
        expand(used[0] + 1);
        return true;
    }

    return false;
}

bool HashTable::expand(std::size_t size) {
    if (is_rehashing() || used[0] > size || HT_SIZE(size_exp[0]) >= size) {
        return false;
    }

    const std::int8_t new_exp = next_exp(size);
    const std::size_t new_size = HT_SIZE(new_exp);

    if (new_exp == size_exp[0]) {
        return false;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto *new_table = new HashNode *[new_size]();

    table[1] = new_table;
    used[1] = 0;
    size_exp[1] = new_exp;
    rehash_idx = 0;

    // First initialization or first hash table is empty
    if (table[0] == nullptr || used[0] == 0) {
        if (table[0] != nullptr) {
            delete[] table[0]; // NOLINT(cppcoreguidelines-owning-memory)
        }
        table[0] = new_table;
        used[0] = 0;
        size_exp[0] = new_exp;
        rehash_idx = -1;
        reset(1);
        return true;
    }

    return true;
}

void HashTable::try_rehash(std::int64_t idx) {
    if (is_rehashing()) {
        if (idx >= rehash_idx && table[0][idx] != nullptr) {
            rehash_bucket(idx);
        } else {
            rehash_steps(1);
        }
        check_rehash_complete();
    }
}

void HashTable::rehash_bucket(std::size_t idx) {
    HashNode *node = table[0][idx];

    while (node != nullptr) {
        HashNode *next = node->next;
        const std::size_t new_idx = hash_fn(node->key) & HT_MASK(size_exp[1]);

        // Move node to the new table
        node->next = table[1][new_idx];
        table[1][new_idx] = node;

        used[0]--;
        used[1]++;
        node = next;
    }

    table[0][idx] = nullptr;
}

void HashTable::rehash_steps(std::size_t n) {
    std::size_t empty_visits = n * 10;

    while (n-- != 0 && used[0] != 0) {
        while (table[0][rehash_idx] == nullptr) {
            rehash_idx++;
            if (--empty_visits == 0) {
                return;
            }
        }
        rehash_bucket(rehash_idx++);
    }
}

bool HashTable::check_rehash_complete() {
    if (used[0] != 0) {
        return false;
    }

    delete[] table[0]; // NOLINT(cppcoreguidelines-owning-memory)

    table[0] = table[1];
    used[0] = used[1];
    size_exp[0] = size_exp[1];

    rehash_idx = -1;
    reset(1);

    return true;
}

namespace {
std::int8_t next_exp(std::size_t size) {
    if (size <= HT_INIT_SIZE) {
        return HT_INIT_EXP;
    }
    return 8 * sizeof(std::size_t) - __builtin_clzll(size - 1); // NOLINT
}
} // namespace