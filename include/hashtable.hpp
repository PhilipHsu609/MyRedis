#pragma once

#include <array>
#include <cstdint>    // std::uint64_t
#include <functional> // std::function
#include <string>
#include <utility> // std::hash

class HashTable;
extern HashTable map;

constexpr std::size_t HT_INIT_EXP = 2;
constexpr std::size_t HT_INIT_SIZE = 1 << HT_INIT_EXP;
constexpr std::size_t HT_SIZE(std::int8_t exp) { return exp == -1 ? 0 : 1 << exp; }
constexpr std::size_t HT_MASK(std::int8_t exp) {
    return exp == -1 ? 0 : HT_SIZE(exp) - 1;
}

struct HashNode {
    std::string key;
    std::string value;
    HashNode *next = nullptr;
};

struct HTState {
    std::size_t used = 0;
    std::size_t size = 0;
    std::int8_t size_exp = 0;
};

class HashTable {
  public:
    using KeyCompare = std::function<bool(const std::string &, const std::string &)>;
    using Hash = std::function<std::size_t(const std::string &)>;

    HashTable();
    HashTable(const HashTable &) = delete;
    HashTable(HashTable &&other) noexcept;

    HashTable &operator=(const HashTable &) = delete;
    HashTable &operator=(HashTable &&other) noexcept;

    ~HashTable();

    void set(std::string key, std::string value);
    HashNode *get(const std::string &key);
    bool remove(const std::string &key);

    bool is_empty() const;
    HTState state(std::size_t htidx) const;
    std::size_t size() const;
    std::size_t buckets() const;

    void set_cmp(KeyCompare cmp);
    void set_hash(Hash fn);
    void force_rehash();

  private:
    void reset(std::size_t htidx);
    void clear(std::size_t htidx);
    bool is_rehashing() const;
    bool try_expand();
    bool expand(std::size_t size);

    void try_rehash(std::int64_t idx);
    void rehash_bucket(std::size_t idx);
    void rehash_steps(std::size_t n);
    bool check_rehash_complete();

    // 0: old, 1: new
    std::array<HashNode **, 2> table{};
    std::array<std::size_t, 2> used{};
    std::array<std::int8_t, 2> size_exp{};

    std::int64_t rehash_idx = -1;
    Hash hash_fn = std::hash<std::string>{};
    KeyCompare cmp = std::equal_to<std::string>{};
};
