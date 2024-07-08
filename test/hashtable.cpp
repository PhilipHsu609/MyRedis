#include "hashtable.hpp"

#include <gtest/gtest.h>

#include <string>  // std::to_string
#include <utility> // std::move

TEST(HashTable, BasicOperations) {
    HashTable ht;
    EXPECT_EQ(ht.get("key"), nullptr);

    ht.set("key", "value");
    ASSERT_FALSE(ht.is_empty());

    auto node = ht.get("key");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->key, "key");
    EXPECT_EQ(node->value, "value");

    ht.remove("key");
    ASSERT_TRUE(ht.is_empty());

    EXPECT_EQ(ht.get("key"), nullptr);
    EXPECT_FALSE(ht.remove("not exist"));
}

TEST(HashTable, Resize) {
    HashTable ht;
    for (int i = 0; i < 16; i++) {
        ht.set(std::to_string(i), std::to_string(i));
    }

    ht.force_rehash();

    EXPECT_EQ(ht.size(), 16);
    EXPECT_EQ(ht.buckets(), 16);

    for (int i = 0; i < 16; i++) {
        auto node = ht.get(std::to_string(i));
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->key, std::to_string(i));
        EXPECT_EQ(node->value, std::to_string(i));
    }
}

TEST(HashTable, MoreResize) {
    HashTable ht;
    for (int i = 0; i < 128; i++) {
        ht.set(std::to_string(i), std::to_string(i));
    }

    ht.force_rehash();

    EXPECT_EQ(ht.size(), 128);
    EXPECT_EQ(ht.buckets(), 128);

    for (int i = 0; i < 128; i++) {
        auto node = ht.get(std::to_string(i));
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->key, std::to_string(i));
        EXPECT_EQ(node->value, std::to_string(i));
    }
}