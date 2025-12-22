/**
 * @file test_types.cpp
 * @brief Unit tests for type-safe abstractions
 */

#include "../include/keyhunt/core/types.h"

using namespace keyhunt::core;

// UInt256 Tests

TEST(UInt256, DefaultConstructor) {
    UInt256 n;
    EXPECT_TRUE(n.is_zero());
    return true;
}

TEST(UInt256, ValueConstructor) {
    UInt256 n(42);
    EXPECT_FALSE(n.is_zero());
    EXPECT_EQ(n.limb(0), 42ULL);
    EXPECT_EQ(n.limb(1), 0ULL);
    return true;
}

TEST(UInt256, FromHex) {
    auto n = UInt256::from_hex("ff");
    EXPECT_TRUE(n.has_value());
    EXPECT_EQ(n->limb(0), 255ULL);

    auto n2 = UInt256::from_hex("0x100");
    EXPECT_TRUE(n2.has_value());
    EXPECT_EQ(n2->limb(0), 256ULL);

    auto n3 = UInt256::from_hex("ffffffffffffffff");
    EXPECT_TRUE(n3.has_value());
    EXPECT_EQ(n3->limb(0), 0xFFFFFFFFFFFFFFFFULL);

    return true;
}

TEST(UInt256, ToHex) {
    UInt256 n(255);
    EXPECT_EQ(n.to_hex(), "ff");

    UInt256 n2(0x1234);
    EXPECT_EQ(n2.to_hex(), "1234");

    return true;
}

TEST(UInt256, Comparison) {
    UInt256 a(100);
    UInt256 b(200);
    UInt256 c(100);

    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(a == c);
    EXPECT_TRUE(a <= c);
    EXPECT_TRUE(a >= c);
    EXPECT_TRUE(a != b);

    return true;
}

TEST(UInt256, Addition) {
    UInt256 a(100);
    UInt256 b(200);
    UInt256 c = a + b;
    EXPECT_EQ(c.limb(0), 300ULL);

    // Test overflow to next limb
    UInt256 max64(0xFFFFFFFFFFFFFFFFULL);
    UInt256 one(1);
    UInt256 result = max64 + one;
    EXPECT_EQ(result.limb(0), 0ULL);
    EXPECT_EQ(result.limb(1), 1ULL);

    return true;
}

TEST(UInt256, Subtraction) {
    UInt256 a(300);
    UInt256 b(100);
    UInt256 c = a - b;
    EXPECT_EQ(c.limb(0), 200ULL);

    return true;
}

TEST(UInt256, BitOperations) {
    UInt256 n;
    n.set_bit(0);
    EXPECT_EQ(n.limb(0), 1ULL);

    n.set_bit(64);
    EXPECT_EQ(n.limb(1), 1ULL);

    EXPECT_TRUE(n.get_bit(0));
    EXPECT_TRUE(n.get_bit(64));
    EXPECT_FALSE(n.get_bit(1));

    return true;
}

TEST(UInt256, HighestBit) {
    UInt256 n(1);
    EXPECT_EQ(n.highest_bit(), 0);

    UInt256 n2(256);  // 2^8
    EXPECT_EQ(n2.highest_bit(), 8);

    UInt256 n3(0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(n3.highest_bit(), 63);

    return true;
}

TEST(UInt256, ToFromBytes) {
    UInt256 original(0x123456789ABCDEF0ULL);
    auto bytes = original.to_bytes();
    UInt256 restored = UInt256::from_bytes(bytes.data());
    EXPECT_EQ(original, restored);

    return true;
}

// ByteArray Tests

TEST(ByteArray, FromHex) {
    auto hash = Hash256::from_hex(
        "0000000000000000000000000000000000000000000000000000000000000001");
    EXPECT_TRUE(hash.has_value());
    EXPECT_EQ((*hash)[31], 1);  // Last byte should be 1

    return true;
}

TEST(ByteArray, ToHex) {
    Hash256 hash;
    hash[31] = 0xAB;
    std::string hex = hash.to_hex();
    EXPECT_TRUE(hex.find("ab") == hex.size() - 2);

    return true;
}

TEST(ByteArray, Comparison) {
    Hash256 a;
    Hash256 b;
    a[0] = 1;
    b[0] = 2;

    EXPECT_TRUE(a < b);
    EXPECT_TRUE(a != b);

    Hash256 c;
    c[0] = 1;
    EXPECT_TRUE(a == c);

    return true;
}

TEST(ByteArray, XOR) {
    Hash256 a;
    Hash256 b;
    a[0] = 0xFF;
    b[0] = 0x0F;

    Hash256 c = a ^ b;
    EXPECT_EQ(c[0], 0xF0);

    return true;
}

TEST(ByteArray, IsZero) {
    Hash256 zero;
    EXPECT_TRUE(zero.is_zero());

    Hash256 nonzero;
    nonzero[15] = 1;
    EXPECT_FALSE(nonzero.is_zero());

    return true;
}

TEST(ByteArray, SecureZero) {
    Hash256 data;
    data[0] = 0xFF;
    data[15] = 0xAB;
    data.secure_zero();
    EXPECT_TRUE(data.is_zero());

    return true;
}

// KeyRange Tests

TEST(KeyRange, ForBits) {
    auto range = KeyRange::for_bits(8);

    // Start should be 2^7 = 128
    EXPECT_EQ(range.start.limb(0), 128ULL);

    // End should be 2^8 - 1 = 255
    EXPECT_EQ(range.end.limb(0), 255ULL);

    return true;
}

TEST(KeyRange, Contains) {
    auto range = KeyRange::for_bits(8);

    UInt256 inside(200);
    UInt256 below(50);
    UInt256 above(300);

    EXPECT_TRUE(range.contains(inside));
    EXPECT_FALSE(range.contains(below));
    EXPECT_FALSE(range.contains(above));

    return true;
}

// BitcoinAddress Tests

TEST(BitcoinAddress, Validation) {
    // Valid mainnet address
    BitcoinAddress addr1("1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2");
    EXPECT_TRUE(addr1.validate());

    // Valid P2SH address
    BitcoinAddress addr2("3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy");
    EXPECT_TRUE(addr2.validate());

    return true;
}
