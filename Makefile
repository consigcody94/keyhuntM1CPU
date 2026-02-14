CC = clang
CXX = clang++
CFLAGS = -Ofast -target arm64-apple-macos -arch arm64 -ftree-vectorize -flto=thin -march=armv8.2-a+crypto -fomit-frame-pointer -D__APPLE__
CXXFLAGS = -Ofast -target arm64-apple-macos -arch arm64 -ftree-vectorize -flto=thin -march=armv8.2-a+crypto -fomit-frame-pointer -D__APPLE__
LDFLAGS = -lm -lpthread -lcrypto -lgmp
RM = rm -f

# Specify the include path for OpenSSL
OPENSSL_INCLUDE = -I/opt/homebrew/opt/openssl@3/include
OPENSSL_LIB = -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto

# Specify the include and library paths for GMP
GMP_INCLUDE := -I/opt/homebrew/include
GMP_LIB := -L/opt/homebrew/lib -lgmp

CFLAGS += $(OPENSSL_INCLUDE) $(GMP_INCLUDE)
CXXFLAGS += $(OPENSSL_INCLUDE) $(GMP_INCLUDE)
LDFLAGS += $(OPENSSL_LIB) $(GMP_LIB)

# Define the separator
SEPARATOR = ;

# List of GMP256K1 source files
GMP256K1_SRCS = gmp256k1/GMP256K1.cpp gmp256k1/Int.cpp gmp256k1/IntGroup.cpp gmp256k1/IntMod.cpp gmp256k1/Point.cpp gmp256k1/Random.cpp

legacy: ; \
    $(CXX) $(CXXFLAGS) -c oldbloom/bloom.cpp -o oldbloom.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c bloom/bloom.cpp -o bloom.o $(SEPARATOR) \
    $(CC) $(CFLAGS) -c base58/base58.c -o base58.o $(SEPARATOR) \
    $(CC) $(CFLAGS) -c xxhash/xxhash.c -o xxhash.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c util.c -o util.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c sha3/sha3.c -o sha3.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c sha3/keccak.c -o keccak.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c hashing.c -o hashing.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c gmp256k1/Int.cpp -o Int.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c gmp256k1/Point.cpp -o Point.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c gmp256k1/GMP256K1.cpp -o GMP256K1.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c gmp256k1/IntMod.cpp -o IntMod.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c gmp256k1/Random.cpp -o Random.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c gmp256k1/IntGroup.cpp -o IntGroup.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -o keyhunt keyhunt_legacy.cpp base58.o bloom.o oldbloom.o xxhash.o util.o Int.o Point.o GMP256K1.o IntMod.o IntGroup.o Random.o hashing.o sha3.o keccak.o $(LDFLAGS) $(SEPARATOR) \
    $(RM) *.o

bsgsd: ; \
    $(CXX) $(CXXFLAGS) -c oldbloom/bloom.cpp -o oldbloom.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c bloom/bloom.cpp -o bloom.o $(SEPARATOR) \
    $(CC) $(CFLAGS) -c base58/base58.c -o base58.o $(SEPARATOR) \
    $(CC) $(CFLAGS) -c rmd160/rmd160.c -o rmd160.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c sha3/sha3.c -o sha3.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c sha3/keccak.c -o keccak.o $(SEPARATOR) \
    $(CC) $(CFLAGS) -c xxhash/xxhash.c -o xxhash.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c util.c -o util.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c secp256k1/Int.cpp -o Int.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c secp256k1/Point.cpp -o Point.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c secp256k1/SECP256K1.cpp -o SECP256K1.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c secp256k1/IntMod.cpp -o IntMod.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c secp256k1/Random.cpp -o Random.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -c secp256k1/IntGroup.cpp -o IntGroup.o $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -o hash/ripemd160.o -c hash/ripemd160.cpp $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -o hash/sha256.o -c hash/sha256.cpp $(SEPARATOR) \
    $(CXX) $(CXXFLAGS) -o bsgsd bsgsd.cpp base58.o rmd160.o hash/ripemd160.o hash/sha256.o bloom.o oldbloom.o xxhash.o util.o Int.o Point.o SECP256K1.o IntMod.o Random.o IntGroup.o sha3.o keccak.o $(LDFLAGS) $(SEPARATOR) \
    $(RM) *.o    
