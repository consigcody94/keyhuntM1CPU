/**
 * @file types.h
 * @brief Type-safe abstractions for cryptographic primitives
 *
 * Provides strongly-typed wrappers for keys, addresses, hashes,
 * and other cryptographic data to prevent type confusion errors.
 */

#ifndef KEYHUNT_CORE_TYPES_H
#define KEYHUNT_CORE_TYPES_H

#include <array>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <optional>
#include <sstream>
#include <iomanip>

#include "error.h"

namespace keyhunt {
namespace core {

/**
 * @brief Fixed-size byte array with type safety
 */
template<size_t N, typename Tag>
class ByteArray {
public:
    static constexpr size_t SIZE = N;

    ByteArray() noexcept {
        data_.fill(0);
    }

    explicit ByteArray(const uint8_t* src) {
        if (src) {
            std::memcpy(data_.data(), src, N);
        } else {
            data_.fill(0);
        }
    }

    explicit ByteArray(const std::array<uint8_t, N>& arr) : data_(arr) {}

    // From hex string
    static std::optional<ByteArray> from_hex(std::string_view hex) {
        // Remove optional 0x prefix
        if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
            hex = hex.substr(2);
        }

        if (hex.size() != N * 2) {
            return std::nullopt;
        }

        ByteArray result;

        for (size_t i = 0; i < N; ++i) {
            int high = hex_char_to_int(hex[i * 2]);
            int low = hex_char_to_int(hex[i * 2 + 1]);

            if (high < 0 || low < 0) {
                return std::nullopt;
            }

            result.data_[i] = static_cast<uint8_t>((high << 4) | low);
        }

        return result;
    }

    // To hex string
    std::string to_hex() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < N; ++i) {
            oss << std::setw(2) << static_cast<int>(data_[i]);
        }
        return oss.str();
    }

    // Data access
    uint8_t* data() noexcept { return data_.data(); }
    const uint8_t* data() const noexcept { return data_.data(); }

    size_t size() const noexcept { return N; }

    uint8_t& operator[](size_t i) { return data_[i]; }
    const uint8_t& operator[](size_t i) const { return data_[i]; }

    // Iterators
    auto begin() noexcept { return data_.begin(); }
    auto end() noexcept { return data_.end(); }
    auto begin() const noexcept { return data_.begin(); }
    auto end() const noexcept { return data_.end(); }

    // Comparison
    bool operator==(const ByteArray& other) const {
        return data_ == other.data_;
    }

    bool operator!=(const ByteArray& other) const {
        return data_ != other.data_;
    }

    bool operator<(const ByteArray& other) const {
        return data_ < other.data_;
    }

    // Check if all zeros
    bool is_zero() const {
        return std::all_of(data_.begin(), data_.end(), [](uint8_t b) { return b == 0; });
    }

    // Secure zero
    void secure_zero() {
        volatile uint8_t* p = data_.data();
        for (size_t i = 0; i < N; ++i) {
            p[i] = 0;
        }
    }

    // XOR operation
    ByteArray operator^(const ByteArray& other) const {
        ByteArray result;
        for (size_t i = 0; i < N; ++i) {
            result.data_[i] = data_[i] ^ other.data_[i];
        }
        return result;
    }

    ByteArray& operator^=(const ByteArray& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] ^= other.data_[i];
        }
        return *this;
    }

private:
    static int hex_char_to_int(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    std::array<uint8_t, N> data_;
};

// Type tags for different byte array types
struct PrivateKeyTag {};
struct PublicKeyTag {};
struct PublicKeyCompressedTag {};
struct Hash256Tag {};
struct Hash160Tag {};
struct AddressTag {};

// Concrete types
using PrivateKey = ByteArray<32, PrivateKeyTag>;
using PublicKey = ByteArray<65, PublicKeyTag>;  // Uncompressed: 04 + X + Y
using PublicKeyCompressed = ByteArray<33, PublicKeyCompressedTag>;  // 02/03 + X
using Hash256 = ByteArray<32, Hash256Tag>;  // SHA-256 hash
using Hash160 = ByteArray<20, Hash160Tag>;  // RIPEMD-160 hash (used in Bitcoin addresses)
using AddressHash = ByteArray<25, AddressTag>;  // Base58Check encoded address

/**
 * @brief 256-bit unsigned integer for key range calculations
 */
class UInt256 {
public:
    static constexpr size_t NUM_LIMBS = 4;  // 4 x 64-bit

    UInt256() noexcept {
        limbs_.fill(0);
    }

    explicit UInt256(uint64_t value) noexcept {
        limbs_.fill(0);
        limbs_[0] = value;
    }

    // From hex string
    static std::optional<UInt256> from_hex(std::string_view hex) {
        // Remove optional 0x prefix
        if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
            hex = hex.substr(2);
        }

        // Pad with zeros if needed
        std::string padded;
        if (hex.size() < 64) {
            padded = std::string(64 - hex.size(), '0') + std::string(hex);
            hex = padded;
        } else if (hex.size() > 64) {
            return std::nullopt;  // Too large
        }

        UInt256 result;

        for (size_t i = 0; i < 4; ++i) {
            size_t offset = (3 - i) * 16;  // Big-endian
            std::string_view chunk = hex.substr(offset, 16);

            uint64_t value = 0;
            for (char c : chunk) {
                value <<= 4;
                int digit = hex_char_to_int(c);
                if (digit < 0) return std::nullopt;
                value |= digit;
            }
            result.limbs_[i] = value;
        }

        return result;
    }

    // To hex string
    std::string to_hex() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        bool leading_zero = true;
        for (int i = 3; i >= 0; --i) {
            if (limbs_[i] != 0 || !leading_zero || i == 0) {
                if (leading_zero && limbs_[i] != 0) {
                    oss << limbs_[i];  // No padding for first non-zero
                    leading_zero = false;
                } else if (!leading_zero) {
                    oss << std::setw(16) << limbs_[i];
                } else {
                    oss << "0";
                }
            }
        }

        return oss.str();
    }

    // Comparison
    bool operator==(const UInt256& other) const {
        return limbs_ == other.limbs_;
    }

    bool operator!=(const UInt256& other) const {
        return limbs_ != other.limbs_;
    }

    bool operator<(const UInt256& other) const {
        for (int i = 3; i >= 0; --i) {
            if (limbs_[i] < other.limbs_[i]) return true;
            if (limbs_[i] > other.limbs_[i]) return false;
        }
        return false;
    }

    bool operator<=(const UInt256& other) const {
        return !(other < *this);
    }

    bool operator>(const UInt256& other) const {
        return other < *this;
    }

    bool operator>=(const UInt256& other) const {
        return !(*this < other);
    }

    // Arithmetic
    UInt256 operator+(const UInt256& other) const {
        UInt256 result;
        uint64_t carry = 0;

        for (size_t i = 0; i < 4; ++i) {
            __uint128_t sum = static_cast<__uint128_t>(limbs_[i]) +
                              static_cast<__uint128_t>(other.limbs_[i]) +
                              carry;
            result.limbs_[i] = static_cast<uint64_t>(sum);
            carry = static_cast<uint64_t>(sum >> 64);
        }

        return result;
    }

    UInt256 operator-(const UInt256& other) const {
        UInt256 result;
        uint64_t borrow = 0;

        for (size_t i = 0; i < 4; ++i) {
            uint64_t a = limbs_[i];
            uint64_t b = other.limbs_[i] + borrow;

            if (b < borrow || a < b) {
                borrow = 1;
                result.limbs_[i] = a + (~b + 1);  // a - b with underflow
            } else {
                borrow = 0;
                result.limbs_[i] = a - b;
            }
        }

        return result;
    }

    UInt256& operator+=(const UInt256& other) {
        *this = *this + other;
        return *this;
    }

    UInt256& operator-=(const UInt256& other) {
        *this = *this - other;
        return *this;
    }

    // Increment
    UInt256& operator++() {
        for (size_t i = 0; i < 4; ++i) {
            if (++limbs_[i] != 0) break;
        }
        return *this;
    }

    // Check if zero
    bool is_zero() const {
        return limbs_[0] == 0 && limbs_[1] == 0 &&
               limbs_[2] == 0 && limbs_[3] == 0;
    }

    // Get bit at position (0-255)
    bool get_bit(size_t pos) const {
        if (pos >= 256) return false;
        size_t limb_idx = pos / 64;
        size_t bit_idx = pos % 64;
        return (limbs_[limb_idx] >> bit_idx) & 1;
    }

    // Set bit at position
    void set_bit(size_t pos, bool value = true) {
        if (pos >= 256) return;
        size_t limb_idx = pos / 64;
        size_t bit_idx = pos % 64;

        if (value) {
            limbs_[limb_idx] |= (1ULL << bit_idx);
        } else {
            limbs_[limb_idx] &= ~(1ULL << bit_idx);
        }
    }

    // Get highest set bit position (0-255, or -1 if zero)
    int highest_bit() const {
        for (int i = 3; i >= 0; --i) {
            if (limbs_[i] != 0) {
                return i * 64 + (63 - __builtin_clzll(limbs_[i]));
            }
        }
        return -1;
    }

    // Access limbs directly
    uint64_t& limb(size_t i) { return limbs_[i]; }
    uint64_t limb(size_t i) const { return limbs_[i]; }

    // To bytes (big-endian)
    std::array<uint8_t, 32> to_bytes() const {
        std::array<uint8_t, 32> result;
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                result[31 - (i * 8 + j)] = static_cast<uint8_t>(limbs_[i] >> (j * 8));
            }
        }
        return result;
    }

    // From bytes (big-endian)
    static UInt256 from_bytes(const uint8_t* bytes) {
        UInt256 result;
        for (size_t i = 0; i < 4; ++i) {
            result.limbs_[i] = 0;
            for (size_t j = 0; j < 8; ++j) {
                result.limbs_[i] |= static_cast<uint64_t>(bytes[31 - (i * 8 + j)]) << (j * 8);
            }
        }
        return result;
    }

private:
    static int hex_char_to_int(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    std::array<uint64_t, 4> limbs_;  // Little-endian limbs
};

/**
 * @brief Key range for search operations
 */
struct KeyRange {
    UInt256 start;
    UInt256 end;

    // Create range for a specific bit count
    static KeyRange for_bits(int bit_count) {
        KeyRange range;

        if (bit_count <= 0 || bit_count > 256) {
            throw ValidationException("Bit count must be between 1 and 256");
        }

        // Start: 2^(bit_count-1)
        range.start.set_bit(bit_count - 1);

        // End: 2^bit_count - 1
        for (int i = 0; i < bit_count; ++i) {
            range.end.set_bit(i);
        }

        return range;
    }

    // Get the size of the range
    UInt256 size() const {
        if (start > end) return UInt256(0);
        return end - start + UInt256(1);
    }

    // Check if a key is within this range
    bool contains(const UInt256& key) const {
        return key >= start && key <= end;
    }

    // Split into n equal parts
    std::vector<KeyRange> split(size_t n) const {
        std::vector<KeyRange> parts;
        if (n == 0) return parts;

        UInt256 range_size = size();
        // Simplified split - just divide by n
        // In practice, this would need proper UInt256 division

        parts.reserve(n);
        // TODO: Implement proper division for KeyRange splitting
        parts.push_back(*this);  // Placeholder

        return parts;
    }
};

/**
 * @brief Bitcoin address (Base58Check encoded)
 */
class BitcoinAddress {
public:
    BitcoinAddress() = default;

    explicit BitcoinAddress(const std::string& addr) : address_(addr) {
        if (!validate()) {
            throw ValidationException("Invalid Bitcoin address: " + addr);
        }
    }

    explicit BitcoinAddress(const Hash160& hash, uint8_t version = 0x00) {
        // Would encode hash to Base58Check format
        // Placeholder implementation
        (void)hash;
        (void)version;
    }

    const std::string& to_string() const {
        return address_;
    }

    bool validate() const {
        if (address_.empty()) return false;

        // Basic length check
        if (address_.size() < 26 || address_.size() > 35) return false;

        // Check for valid Base58 characters
        const std::string base58_chars =
            "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

        for (char c : address_) {
            if (base58_chars.find(c) == std::string::npos) {
                return false;
            }
        }

        // Mainnet addresses start with 1 or 3 (P2PKH or P2SH)
        // Testnet addresses start with m, n, or 2
        // Bech32 addresses start with bc1 or tb1
        char prefix = address_[0];
        if (prefix != '1' && prefix != '3' && prefix != 'm' &&
            prefix != 'n' && prefix != '2') {
            // Check for Bech32
            if (address_.size() >= 4 &&
                (address_.substr(0, 3) == "bc1" || address_.substr(0, 3) == "tb1")) {
                return true;  // Bech32 address
            }
            return false;
        }

        return true;
    }

    bool operator==(const BitcoinAddress& other) const {
        return address_ == other.address_;
    }

    bool operator!=(const BitcoinAddress& other) const {
        return address_ != other.address_;
    }

private:
    std::string address_;
};

} // namespace core
} // namespace keyhunt

// Hash function for use in unordered containers
namespace std {
    template<size_t N, typename Tag>
    struct hash<keyhunt::core::ByteArray<N, Tag>> {
        size_t operator()(const keyhunt::core::ByteArray<N, Tag>& arr) const {
            size_t result = 0;
            const size_t* ptr = reinterpret_cast<const size_t*>(arr.data());
            for (size_t i = 0; i < N / sizeof(size_t); ++i) {
                result ^= ptr[i] + 0x9e3779b9 + (result << 6) + (result >> 2);
            }
            return result;
        }
    };
}

#endif // KEYHUNT_CORE_TYPES_H
