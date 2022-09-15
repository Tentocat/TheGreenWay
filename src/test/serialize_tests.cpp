// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <serialize.h>
#include <streams.h>
#include <hash.h>
#include <test/test_bitcoin.h>

#include <stdint.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(serialize_tests, BasicTestingSetup)

class CSerializeMethodsTestSingle
{
protected:
    int intval;
    bool boolval;
    std::string stringval;
    const char* charstrval;
    CTransactionRef txval;
public:
    CSerializeMethodsTestSingle() = default;
    CSerializeMethodsTestSingle(int intvalin, bool boolvalin, std::string stringvalin, const char* charstrvalin, CTransaction txvalin) : intval(intvalin), boolval(boolvalin), stringval(std::move(stringvalin)), charstrval(charstrvalin), txval(MakeTransactionRef(txvalin)){}
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(intval);
        READWRITE(boolval);
        READWRITE(stringval);
        READWRITE(FLATDATA(charstrval));
        READWRITE(txval);
    }

    bool operator==(const CSerializeMethodsTestSingle& rhs)
    {
        return  intval == rhs.intval && \
                boolval == rhs.boolval && \
                stringval == rhs.stringval && \
                strcmp(charstrval, rhs.charstrval) == 0 && \
                *txval == *rhs.txval;
    }
};

class CSerializeMethodsTestMany : public CSerializeMethodsTestSingle
{
public:
    using CSerializeMethodsTestSingle::CSerializeMethodsTestSingle;
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEMANY(intval, boolval, stringval, FLATDATA(charstrval), txval);
    }
};

BOOST_AUTO_TEST_CASE(sizes)
{
    BOOST_CHECK_EQUAL(sizeof(char), GetSerializeSize(char(0), 0));
    BOOST_CHECK_EQUAL(sizeof(int8_t), GetSerializeSize(int8_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(uint8_t), GetSerializeSize(uint8_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(int16_t), GetSerializeSize(int16_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(uint16_t), GetSerializeSize(uint16_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(int32_t), GetSerializeSize(int32_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(uint32_t), GetSerializeSize(uint32_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(int64_t), GetSerializeSize(int64_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(uint64_t), GetSerializeSize(uint64_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(float), GetSerializeSize(float(0), 0));
    BOOST_CHECK_EQUAL(sizeof(double), GetSerializeSize(double(0), 0));
    // Bool is serialized as char
    BOOST_CHECK_EQUAL(sizeof(char), GetSerializeSize(bool(0), 0));

    // Sanity-check GetSerializeSize and c++ type matching
    BOOST_CHECK_EQUAL(GetSerializeSize(char(0), 0), 1);
    BOOST_CHECK_EQUAL(GetSerializeSize(int8_t(0), 0), 1);
    BOOST_CHECK_EQUAL(GetSerializeSize(uint8_t(0), 0), 1);
    BOOST_CHECK_EQUAL(GetSerializeSize(int16_t(0), 0), 2);
    BOOST_CHECK_EQUAL(GetSerializeSize(uint16_t(0), 0), 2);
    BOOST_CHECK_EQUAL(GetSerializeSize(int32_t(0), 0), 4);
    BOOST_CHECK_EQUAL(GetSerializeSize(uint32_t(0), 0), 4);
    BOOST_CHECK_EQUAL(GetSerializeSize(int64_t(0), 0), 8);
    BOOST_CHECK_EQUAL(GetSerializeSize(uint64_t(0), 0), 8);
    BOOST_CHECK_EQUAL(GetSerializeSize(float(0), 0), 4);
    BOOST_CHECK_EQUAL(GetSerializeSize(double(0), 0), 8);
    BOOST_CHECK_EQUAL(GetSerializeSize(bool(0), 0), 1);
}

BOOST_AUTO_TEST_CASE(floats_conversion)
{
    // Choose values that map unambiguously to binary floating point to avoid
    // rounding issues at the compiler side.
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x00000000), 0.0F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x3f000000), 0.5F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x3f800000), 1.0F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x40000000), 2.0F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x40800000), 4.0F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x44444444), 785.066650390625F);

    BOOST_CHECK_EQUAL(ser_float_to_uint32(0.0F), 0x00000000);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(0.5F), 0x3f000000);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(1.0F), 0x3f800000);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(2.0F), 0x40000000);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(4.0F), 0x40800000);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(785.066650390625F), 0x44444444);
}

BOOST_AUTO_TEST_CASE(doubles_conversion)
{
    // Choose values that map unambiguously to binary floating point to avoid
    // rounding issues at the compiler side.
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x0000000000000000ULL), 0.0);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x3fe0000000000000ULL), 0.5);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x3ff0000000000000ULL), 1.0);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x4000000000000000ULL), 2.0);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x4010000000000000ULL), 4.0);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x4088888880000000ULL), 785.066650390625);

    BOOST_CHECK_EQUAL(ser_double_to_uint64(0.0), 0x0000000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(0.5), 0x3fe0000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(1.0), 0x3ff0000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(2.0), 0x4000000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(4.0), 0x4010000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(785.066650390625), 0x4088888880000000ULL);
}
/*
Python code to generate the below hashes:

    def reversed_hex(x):
        return binascii.hexlify(''.join(reversed(x)))
    def dsha256(x):
        return hashlib.sha256(hashlib.sha256(x).digest()).digest()

    reversed_hex(dsha256(''.join(struct.pack('<f', x) for x in range(0,1000)))) == '8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c'
    reversed_hex(dsha256(''.join(struct.pack('<d', x) for x in range(0,1000)))) == '43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96'
*/
BOOST_AUTO_TEST_CASE(floats)
{
    CDataStream ss(SER_DISK, 0);
    // encode
    for (int i = 0; i < 1000; i++) {
        ss << float(i);
    }
    BOOST_CHECK(Hash(ss.begin(), ss.end()) == uint256S("8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c"));

    // decode
    for (int i = 0; i < 1000; i++) {
        float j;
        ss >> j;
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

BOOST_AUTO_TEST_CASE(doubles)
{
    CDataStream ss(SER_DISK, 0);
    // encode
    for (int i = 0; i < 1000; i++) {
        ss << double(i);
    }
    BOOST_CHECK(Hash(ss.begin(), ss.end()) == uint256S("43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96"));

    // decode
    for (int i = 0; i < 1000; i++) {
        double j;
        ss >> j;
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

BOOST_AUTO_TEST_CASE(varints)
{
    // encode

    CDataStream ss(SER_DISK, 0);
    CDataStream::size_type size = 0;
    for (int i = 0; i < 100000; i++) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        BOOST_CHECK(size == ss.size());
    }

    for (uint64_t i = 0;  i < 100000000000ULL; i += 999999937) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0, 0);
        BOOST_CHECK(size == ss.size());
    }

    // decode
    for (int i = 0; i < 100000; i++) {
        int j = -1;
        ss >> VARINT(j);
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }

    for (uint64_t i = 0;  i < 100000000000ULL; i += 999999937) {
        uint64_t j = -1;
        ss >> VARINT(j);
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

BOOST_AUTO_TEST_CASE(varints_bitpatterns)
{
    CDataStream ss(SER_DISK, 0);
    ss << VARINT(0); BOOST_CHECK_EQUAL(HexStr(ss), "00"); ss.clear();
    ss << VARINT(0x7f); BOOST_CHECK_EQUAL(HexStr(ss), "7f"); ss.clear();
    ss << VARINT((int8_t)0x7f); BOOST_CHECK_EQUAL(HexStr(ss), "7f"); ss.clear();
    ss << VARINT(0x80); BOOST_CHECK_EQUAL(HexStr(ss), "8000"); ss.clear();
    ss << VARINT((uint8_t)0x80); BOOST_CHECK_EQUAL(HexStr(ss), "8000"); ss.clear();
    ss << VARINT(0x1234); BOOST_CHECK_EQUAL(HexStr(ss), "a334"); ss.clear();
    ss << VARINT((int16_t)0x1234); BOOST_CHECK_EQUAL(HexStr(ss), "a334"); ss.clear();
    ss << VARINT(0xffff); BOOST_CHECK_EQUAL(HexStr(ss), "