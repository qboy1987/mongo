/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/platform/decimal128.h"

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "mongo/config.h"
#include "mongo/unittest/unittest.h"

namespace mongo {

// Tests for Decimal128 constructors
TEST(Decimal128Test, TestDefaultConstructor) {
    Decimal128 d;
    ASSERT_TRUE(d.isBinaryEqual(Decimal128(0)));
}

TEST(Decimal128Test, TestInt32ConstructorZero) {
    int32_t intZero = 0;
    Decimal128 d(intZero);
    Decimal128::Value val = d.getValue();
    // 0x3040000000000000 0000000000000000 = +0E+0
    uint64_t highBytes = 0x3040000000000000ull;
    uint64_t lowBytes = 0x0000000000000000ull;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestInt32ConstructorMax) {
    int32_t intMax = std::numeric_limits<int32_t>::max();
    Decimal128 d(intMax);
    Decimal128::Value val = d.getValue();
    // 0x3040000000000000 000000007fffffff = +2147483647E+0
    uint64_t highBytes = 0x3040000000000000ull;
    uint64_t lowBytes = 0x000000007fffffffull;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestInt32ConstructorMin) {
    int32_t intMin = std::numeric_limits<int32_t>::lowest();
    Decimal128 d(intMin);
    Decimal128::Value val = d.getValue();
    // 0xb040000000000000 000000007fffffff = -2147483648E+0
    uint64_t highBytes = 0xb040000000000000ull;
    uint64_t lowBytes = 0x0000000080000000ull;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestInt64ConstructorZero) {
    int64_t longZero = 0;
    Decimal128 d(longZero);
    Decimal128::Value val = d.getValue();
    // 0x3040000000000000 0000000000000000 = +0E+0
    uint64_t highBytes = 0x3040000000000000ull;
    uint64_t lowBytes = 0x0000000000000000ull;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestInt64ConstructorMax) {
    int64_t longMax = std::numeric_limits<long long>::max();
    Decimal128 d(longMax);
    Decimal128::Value val = d.getValue();
    // 0x3040000000000000 7fffffffffffffff = +9223372036854775807E+0
    uint64_t highBytes = 0x3040000000000000ull;
    uint64_t lowBytes = 0x7fffffffffffffffull;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestInt64ConstructorMin) {
    int64_t longMin = std::numeric_limits<long long>::lowest();
    Decimal128 d(longMin);
    Decimal128::Value val = d.getValue();
    // 0xb040000000000000 8000000000000000 = -9223372036854775808E+0
    uint64_t highBytes = 0xb040000000000000;
    uint64_t lowBytes = 0x8000000000000000;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestPartsConstructor) {
    Decimal128 expected(10);
    Decimal128 val(0LL, Decimal128::kExponentBias, 0LL, 10LL);
    ASSERT_EQUALS(val.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(val.getValue().low64, expected.getValue().low64);
}

TEST(Decimal128Test, TestDoubleConstructorQuant1) {
    double dbl = 0.1 / 10;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "0.0100000000000000");
}

TEST(Decimal128Test, TestDoubleConstructorQuant2) {
    double dbl = 0.1 / 10000;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "0.0000100000000000000");
}

TEST(Decimal128Test, TestDoubleConstructorQuant3) {
    double dbl = 0.1 / 1000 / 1000 / 1000 / 1000 / 1000 / 1000;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "1.00000000000000E-19");
}

TEST(Decimal128Test, TestDoubleConstructorQuant4) {
    double dbl = 0.01 * 1000 * 1000 * 1000 * 1000 * 1000 * 1000;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "1.00000000000000E+16");
}

TEST(Decimal128Test, TestDoubleConstructorQuant5) {
    double dbl = 0.0127;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "0.0127000000000000");
}

TEST(Decimal128Test, TestDoubleConstructorQuant6) {
    double dbl = 1234567890.12709;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "1234567890.12709");
}

TEST(Decimal128Test, TestDoubleConstructorQuant7) {
    double dbl = 0.1129857 / 1000 / 1000 / 1000 / 1000 / 1000 / 1000;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "1.12985700000000E-19");
}

TEST(Decimal128Test, TestDoubleConstructorQuant8) {
    double dbl = 724.8799725651578000906738452613353729248046875;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "724.879972565158");
}

TEST(Decimal128Test, TestDoubleConstructorQuant9) {
    double dbl = -0.09645061728395000478;
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "-0.0964506172839500");
}

TEST(Decimal128Test, TestDoubleConstructorQuantFailPoorLog10Of2Estimate) {
    double dbl = exp2(1000);
    Decimal128 d(dbl);
    ASSERT_EQUALS(d.toString(), "1.07150860718627E+301");
}

TEST(Decimal128Test, TestDoubleConstructorZero) {
    double doubleZero = 0;
    Decimal128 d(doubleZero);
    ASSERT_TRUE(d.isEqual(Decimal128(0)));
}

TEST(Decimal128Test, TestDoubleConstructorPos) {
    double doubleNeg = 1.0;
    Decimal128 d(doubleNeg);
    ASSERT_EQUALS(d.toString(), "1.00000000000000");
}

TEST(Decimal128Test, TestDoubleConstructorNeg) {
    double doubleNeg = -1.0;
    Decimal128 d(doubleNeg);
    ASSERT_EQUALS(d.toString(), "-1.00000000000000");
}

TEST(Decimal128Test, TestDoubleConstructorMaxRoundDown) {
    double doubleMax = DBL_MAX;
    Decimal128 d(
        doubleMax, Decimal128::kRoundTo15Digits, Decimal128::RoundingMode::kRoundTowardNegative);
    ASSERT_EQUALS(d.toString(), "1.79769313486231E+308");
}

TEST(Decimal128Test, TestDoubleConstructorMaxRoundUp) {
    double doubleMax = DBL_MAX;
    Decimal128 d(
        doubleMax, Decimal128::kRoundTo15Digits, Decimal128::RoundingMode::kRoundTowardPositive);
    ASSERT_EQUALS(d.toString(), "1.79769313486232E+308");
}

TEST(Decimal128Test, TestDoubleConstructorRoundAllNines) {
    double allNines = 0.999999999999999;  // 15 nines
    Decimal128 d(
        allNines, Decimal128::kRoundTo15Digits, Decimal128::RoundingMode::kRoundTiesToAway);
    ASSERT_EQUALS(d.toString(), "0.999999999999999");  // 15 nines
}

TEST(Decimal128Test, TestDoubleConstructorMaxNeg) {
    double doubleMax = -1 * DBL_MAX;
    Decimal128 d(doubleMax);
    ASSERT_EQUALS(d.toString(), "-1.79769313486232E+308");
}

TEST(Decimal128Test, TestDoubleConstructorMin) {
    double min = DBL_MIN;
    Decimal128 d(min);
    ASSERT_EQUALS(d.toString(), "2.22507385850720E-308");
}

TEST(Decimal128Test, TestDoubleConstructorMinNeg) {
    double min = -DBL_MIN;
    Decimal128 d(min);
    ASSERT_EQUALS(d.toString(), "-2.22507385850720E-308");
}

TEST(Decimal128Test, TestDoubleConstructorInfinity) {
    double dbl = std::numeric_limits<double>::infinity();
    Decimal128 d(dbl);
    ASSERT_TRUE(d.isInfinite());
}

TEST(Decimal128Test, TestDoubleConstructorNaN) {
    double dbl = std::numeric_limits<double>::quiet_NaN();
    Decimal128 d(dbl);
    ASSERT_TRUE(d.isNaN());
}

TEST(Decimal128Test, TestStringConstructorInRange) {
    std::string s = "+2.010";
    Decimal128 d(s);
    Decimal128::Value val = d.getValue();
    // 0x303a000000000000 00000000000007da = +2.010
    uint64_t highBytes = 0x303a000000000000;
    uint64_t lowBytes = 0x00000000000007da;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestStringConstructorPosInfinity) {
    std::string s = "+INFINITY";
    Decimal128 d(s);
    Decimal128::Value val = d.getValue();
    // 0x7800000000000000 0000000000000000 = +Inf
    uint64_t highBytes = 0x7800000000000000;
    uint64_t lowBytes = 0x0000000000000000;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestStringConstructorNegInfinity) {
    std::string s = "-INFINITY";
    Decimal128 d(s);
    Decimal128::Value val = d.getValue();
    // 0xf800000000000000 0000000000000000 = -Inf
    uint64_t highBytes = 0xf800000000000000;
    uint64_t lowBytes = 0x0000000000000000;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestStringConstructorNaN) {
    std::string s = "I am not a number!";
    Decimal128 d(s);
    Decimal128::Value val = d.getValue();
    // 0x7c00000000000000 0000000000000000 = NaN
    uint64_t highBytes = 0x7c00000000000000;
    uint64_t lowBytes = 0x0000000000000000;
    ASSERT_EQUALS(val.high64, highBytes);
    ASSERT_EQUALS(val.low64, lowBytes);
}

TEST(Decimal128Test, TestNonCanonicalDecimal) {
    // It is possible to encode a significand with more than 34 decimal digits.
    // Conforming implementations should not generate these, but they must be treated as zero
    // when encountered. However, the exponent and sign still matter.

    // 0x6c10000000000000 0000000000000000 = non-canonical 0, all ignored bits clear
    Decimal128 nonCanonical0E0(Decimal128::Value{0, 0x6c10000000000000ull});
    std::string zeroE0 = nonCanonical0E0.toString();
    ASSERT_EQUALS(zeroE0, "0");

    // 0xec100000deadbeef 0123456789abcdef = non-canonical -0, random stuff in ignored bits
    Decimal128 nonCanonicalM0E0(Decimal128::Value{0x0123456789abcdefull, 0xec100000deadbeefull});
    std::string minusZeroE0 = nonCanonicalM0E0.toString();
    ASSERT_EQUALS(minusZeroE0, "-0");

    // 0x6c11fffffffffffff ffffffffffffffff = non-canonical 0.000, all ignored bits set
    Decimal128 nonCanonical0E3(Decimal128::Value{0xffffffffffffffffull, 0x6c11ffffffffffffull});
    std::string zeroE3 = nonCanonical0E3.toString();
    ASSERT_EQUALS(zeroE3, "0E+3");

    // Check extraction functions, they should treat this as the corresponding zero as well.
    ASSERT_EQUALS(nonCanonical0E3.getBiasedExponent(), Decimal128("0E+3").getBiasedExponent());
    ASSERT_EQUALS(nonCanonical0E3.getCoefficientHigh(), 0u);
    ASSERT_EQUALS(nonCanonical0E3.getCoefficientLow(), 0u);

    // Check doing some arithmetic opations and number conversions
    const double minusZeroDouble = nonCanonicalM0E0.toDouble();
    ASSERT_EQUALS(minusZeroDouble, 0.0);
    ASSERT_EQUALS(-1.0, std::copysign(1.0, minusZeroDouble));
    ASSERT_TRUE(nonCanonical0E3.add(Decimal128(1)).isEqual(Decimal128(1)));
    ASSERT_TRUE(Decimal128(1).divide(nonCanonicalM0E0).isEqual(Decimal128::kNegativeInfinity));
}

// Tests for absolute value function
TEST(Decimal128Test, TestAbsValuePos) {
    Decimal128 d(25);
    Decimal128 dAbs = d.toAbs();
    ASSERT_TRUE(dAbs.isEqual(d));
}

TEST(Decimal128Test, TestAbsValueNeg) {
    Decimal128 d(-25);
    Decimal128 dAbs = d.toAbs();
    ASSERT_TRUE(dAbs.isEqual(Decimal128(25)));
}


// Tests for Decimal128 conversions
TEST(Decimal128Test, TestDecimal128ToInt32Even) {
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-3, -2, -2, 2, 2, 3};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toInt(), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32Neg) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardNegative;
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-3, -3, -3, 2, 2, 2};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toInt(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32Pos) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardPositive;
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-2, -2, -2, 3, 3, 3};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toInt(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32Zero) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardZero;
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-2, -2, -2, 2, 2, 2};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toInt(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32Away) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTiesToAway;
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-3, -3, -2, 2, 3, 3};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toInt(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64Even) {
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967297, -4294967296, -4294967296, 4294967296, 4294967296, 4294967297};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLong(), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64Neg) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardNegative;
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967297, -4294967297, -4294967297, 4294967296, 4294967296, 4294967296};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLong(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64Pos) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardPositive;
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967296, -4294967296, -4294967296, 4294967297, 4294967297, 4294967297};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLong(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64Zero) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardZero;
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967296, -4294967296, -4294967296, 4294967296, 4294967296, 4294967296};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLong(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64Away) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTiesToAway;
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967297, -4294967297, -4294967296, 4294967296, 4294967297, 4294967297};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLong(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32ExactEven) {
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-3, -2, -2, 2, 2, 3};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toIntExact(), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32ExactNeg) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardNegative;
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-3, -3, -3, 2, 2, 2};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toIntExact(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32ExactPos) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardPositive;
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-2, -2, -2, 3, 3, 3};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toIntExact(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32ExactZero) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardZero;
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-2, -2, -2, 2, 2, 2};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toIntExact(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt32ExactAway) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTiesToAway;
    std::string in[6] = {"-2.7", "-2.5", "-2.2", "2.2", "2.5", "2.7"};
    int32_t out[6] = {-3, -3, -2, 2, 3, 3};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toIntExact(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64ExactEven) {
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967297, -4294967296, -4294967296, 4294967296, 4294967296, 4294967297};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLongExact(), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64ExactNeg) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardNegative;
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967297, -4294967297, -4294967297, 4294967296, 4294967296, 4294967296};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLongExact(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64ExactPos) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardPositive;
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967296, -4294967296, -4294967296, 4294967297, 4294967297, 4294967297};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLongExact(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64ExactZero) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTowardZero;
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967296, -4294967296, -4294967296, 4294967296, 4294967296, 4294967296};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLongExact(roundMode), out[testNo]);
    }
}

TEST(Decimal128Test, TestDecimal128ToInt64ExactAway) {
    Decimal128::RoundingMode roundMode = Decimal128::RoundingMode::kRoundTiesToAway;
    std::string in[6] = {"-4294967296.7",
                         "-4294967296.5",
                         "-4294967296.2",
                         "4294967296.2",
                         "4294967296.5",
                         "4294967296.7"};
    int64_t out[6] = {-4294967297, -4294967297, -4294967296, 4294967296, 4294967297, 4294967297};
    std::unique_ptr<Decimal128> decPtr;
    for (int testNo = 0; testNo < 6; ++testNo) {
        decPtr = std::make_unique<Decimal128>(in[testNo]);
        ASSERT_EQUALS(decPtr->toLongExact(roundMode), out[testNo]);
    }
}


TEST(Decimal128Test, TestDecimal128ToDoubleNormal) {
    std::string s = "+2.015";
    Decimal128 d(s);
    double result = d.toDouble();
    ASSERT_EQUALS(result, 2.015);
}

TEST(Decimal128Test, TestDecimal128ToDoubleZero) {
    std::string s = "+0.000";
    Decimal128 d(s);
    double result = d.toDouble();
    ASSERT_EQUALS(result, 0.0);
}

TEST(Decimal128Test, TestDecimal128ToDoubleLargerThanInfinity) {
    std::string s = "300E2000";
    Decimal128 d(s);
    double result = d.toDouble();
    ASSERT_EQUALS(result, std::numeric_limits<double>::infinity());
}

TEST(Decimal128Test, TestDecimal128ToDoubleLargerThanNegInfinity) {
    std::string s = "-300E2000";
    Decimal128 d(s);
    double result = d.toDouble();
    ASSERT_EQUALS(result, -1 * std::numeric_limits<double>::infinity());
}

TEST(Decimal128Test, TestDecimal128ToDoubleSmallerThanSmallestDouble) {
    std::string s = "1E-5900";
    Decimal128 d(s);
    double result = d.toDouble();
    ASSERT_EQUALS(result, 0);
}

TEST(Decimal128Test, TestDecimal128ToDoubleSmallerThanNegSmallestDouble) {
    std::string s = "-1E-5900";
    Decimal128 d(s);
    double result = d.toDouble();
    ASSERT_EQUALS(result, 0);
}

TEST(Decimal128Test, TestDecimal128ToStringPos) {
    std::string s = "2087.015E+281";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "2.087015E+284");
}

TEST(Decimal128Test, TestDecimal128ToStringPos2) {
    std::string s = "10.50E3";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "1.050E+4");
}

TEST(Decimal128Test, TestDecimal128ToStringPos3) {
    std::string s = "10.51E3";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "1.051E+4");
}

TEST(Decimal128Test, TestDecimal128ToStringNeg) {
    std::string s = "-2087.015E-281";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "-2.087015E-278");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeZero1) {
    std::string s = "0";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeZero2) {
    std::string s = "0.0";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0.0");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeZero3) {
    std::string s = "0.00";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0.00");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeZero4) {
    std::string s = "000.0";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0.0");
}


TEST(Decimal128Test, TestDecimal128ToStringInRangeZero5) {
    std::string s = "0.000000000000";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0E-12");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangePos1) {
    std::string s = "1234567890.1234567890";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "1234567890.1234567890");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangePos2) {
    std::string s = "5.00";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "5.00");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangePos3) {
    std::string s = "50.0";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "50.0");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangePos4) {
    std::string s = "5";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "5");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangePos5) {
    std::string s = "50";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "50");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangePos5Minus) {
    std::string s = "-50";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "-50");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeNeg1) {
    std::string s = ".05";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0.05");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeNeg2) {
    std::string s = ".5";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0.5");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeNeg3) {
    std::string s = ".0052";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0.0052");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeNeg4) {
    std::string s = ".005";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "0.005");
}

TEST(Decimal128Test, TestDecimal128ToStringInRangeNeg4Minus) {
    std::string s = "-.005";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "-0.005");
}

TEST(Decimal128Test, TestDecimal128ToStringOutRangeNeg3) {
    std::string s = ".012587E-200";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "1.2587E-202");
}

TEST(Decimal128Test, TestDecimal128ToStringOutRangePos2) {
    std::string s = "10201.01E14";
    Decimal128 d(s);
    std::string result = d.toString();
    ASSERT_EQUALS(result, "1.020101E+18");
}

TEST(Decimal128Test, TestDecimal128ToStringFinite) {
    // General test cases taken from http://speleotrove.com/decimal/daconvs.html#reftostr
    std::string s[15] = {"123",
                         "-123",
                         "123E1",
                         "123E3",
                         "123E-1",
                         "123E-5",
                         "123E-10",
                         "-123E-12",
                         "0E0",
                         "0E-2",
                         "0E2",
                         "-0",
                         "5E-6",
                         "50E-7",
                         "5E-7"};
    std::string expected[15] = {"123",
                                "-123",
                                "1.23E+3",
                                "1.23E+5",
                                "12.3",
                                "0.00123",
                                "1.23E-8",
                                "-1.23E-10",
                                "0",
                                "0.00",
                                "0E+2",
                                "-0",
                                "0.000005",
                                "0.0000050",
                                "5E-7"};
    for (int i = 0; i < 15; i++) {
        Decimal128 d(s[i]);
        std::string result = d.toString();
        ASSERT_EQUALS(result, expected[i]);
    }
}

TEST(Decimal128Test, TestDecimal128ToStringInvalidToNaN) {
    std::string s = "Some garbage string";
    Decimal128 d(s);
    ASSERT_EQUALS(d.toString(), "NaN");
}

TEST(Decimal128Test, TestDecimal128ToStringNaN) {
    std::string s[3] = {"-NaN", "+NaN", "NaN"};
    for (auto& item : s) {
        Decimal128 d(item);
        ASSERT_EQUALS(d.toString(), "NaN");
    }

    // Testing a NaN with a payload
    Decimal128 payloadNaN(Decimal128::Value({/*payload*/ 0x1, 0x7cull << 56}));
    ASSERT_EQUALS(payloadNaN.toString(), "NaN");
}

TEST(Decimal128Test, TestDecimal128ToStringPosInf) {
    std::string s[3] = {"Inf", "Infinity", "+Inf"};
    for (auto& item : s) {
        Decimal128 d(item);
        ASSERT_EQUALS(d.toString(), "Infinity");
    }
}

TEST(Decimal128Test, TestDecimal128ToStringNegInf) {
    std::string s[2] = {"-Infinity", "-Inf"};
    for (auto& item : s) {
        Decimal128 d(item);
        ASSERT_EQUALS(d.toString(), "-Infinity");
    }
}

// Tests for Decimal128 operations that use a signaling flag
TEST(Decimal128Test, TestDecimal128ToIntSignaling) {
    Decimal128 d("NaN");
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    int32_t intVal = d.toInt(&sigFlags);
    ASSERT_EQUALS(intVal, std::numeric_limits<int32_t>::min());
    ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kInvalid));
}

TEST(Decimal128Test, TestDecimal128ToLongSignaling) {
    Decimal128 d("Infinity");
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    int64_t longVal = d.toLong(&sigFlags);
    ASSERT_EQUALS(longVal, std::numeric_limits<int64_t>::lowest());
    ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kInvalid));
}

TEST(Decimal128Test, TestDecimal128ToIntExactSignaling) {
    Decimal128 d("10000000000000000");
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    int32_t intVal = d.toIntExact(&sigFlags);
    ASSERT_EQUALS(intVal, std::numeric_limits<int32_t>::lowest());
    // TODO: The supported library does not set the kInexact flag even though
    // the documentation claims to for exact integer conversions.
    // ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kInexact));
    ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kInvalid));
}

TEST(Decimal128Test, TestDecimal128ToLongExactSignaling) {
    Decimal128 d("100000000000000000000000000");
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    int64_t longVal = d.toLongExact(&sigFlags);
    ASSERT_EQUALS(longVal, std::numeric_limits<int64_t>::lowest());
    // TODO: The supported library does not set the kInexact flag even though
    // the documentation claims to for exact integer conversions.
    // ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kInexact));
    ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kInvalid));
}

TEST(Decimal128Test, TestDecimal128ToDoubleSignaling) {
    Decimal128 d("0.1");
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    double doubleVal = d.toDouble(&sigFlags);
    ASSERT_EQUALS(doubleVal, 0.1);
    ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kInexact));
}

TEST(Decimal128Test, TestDecimal128AddSignaling) {
    Decimal128 d1("0.1");
    Decimal128 d2("0.1");
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    d1.add(d2, &sigFlags);
    ASSERT_EQUALS(sigFlags, Decimal128::SignalingFlag::kNoFlag);
}

TEST(Decimal128Test, TestDecimal128SubtractSignaling) {
    Decimal128 d = Decimal128::kLargestNegative;
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    Decimal128 res = d.subtract(Decimal128(1), &sigFlags);
    ASSERT_TRUE(res.isEqual(Decimal128::kLargestNegative));
    ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kInexact));
}

TEST(Decimal128Test, TestDecimal128MultiplySignaling) {
    Decimal128 d("2");
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    Decimal128 res = d.multiply(Decimal128::kLargestPositive, &sigFlags);
    ASSERT_TRUE(res.isEqual(Decimal128::kPositiveInfinity));
    ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kOverflow));
}

TEST(Decimal128Test, TestDecimal128DivideSignaling) {
    Decimal128 d("2");
    uint32_t sigFlags = Decimal128::SignalingFlag::kNoFlag;
    Decimal128 res = d.divide(Decimal128(0), &sigFlags);
    ASSERT_TRUE(res.isEqual(Decimal128::kPositiveInfinity));
    ASSERT_TRUE(Decimal128::hasFlag(sigFlags, Decimal128::SignalingFlag::kDivideByZero));
}

// Test Decimal128 special comparisons
TEST(Decimal128Test, TestDecimal128IsZero) {
    Decimal128 d1(0);
    Decimal128 d2(500);
    ASSERT_TRUE(d1.isZero());
    ASSERT_FALSE(d2.isZero());
}

TEST(Decimal128Test, TestDecimal128IsNaN) {
    Decimal128 d1("NaN");
    Decimal128 d2("10.5");
    Decimal128 d3("Inf");
    ASSERT_TRUE(d1.isNaN());
    ASSERT_FALSE(d2.isNaN());
    ASSERT_FALSE(d3.isNaN());
}

TEST(Decimal128Test, TestDecimal128IsInfinite) {
    Decimal128 d1("NaN");
    Decimal128 d2("10.5");
    Decimal128 d3("Inf");
    Decimal128 d4("-Inf");
    ASSERT_FALSE(d1.isInfinite());
    ASSERT_FALSE(d2.isInfinite());
    ASSERT_TRUE(d3.isInfinite());
    ASSERT_TRUE(d4.isInfinite());
}

TEST(Decimal128Test, TestDecimal128IsNegative) {
    Decimal128 d1("NaN");
    Decimal128 d2("-NaN");
    Decimal128 d3("10.5");
    Decimal128 d4("-10.5");
    Decimal128 d5("Inf");
    Decimal128 d6("-Inf");
    ASSERT_FALSE(d1.isNegative());
    ASSERT_FALSE(d3.isNegative());
    ASSERT_FALSE(d5.isNegative());
    ASSERT_TRUE(d2.isNegative());
    ASSERT_TRUE(d4.isNegative());
    ASSERT_TRUE(d6.isNegative());
}

// Tests for Decimal128 math operations
TEST(Decimal128Test, TestDecimal128AdditionCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("-50.5218E19");
    Decimal128 result = d1.add(d2);
    Decimal128 expected("1.999782E21");
    ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128AdditionCase2) {
    Decimal128 d1("1.00");
    Decimal128 d2("2.000");
    Decimal128 result = d1.add(d2);
    Decimal128 expected("3.000");
    ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128SubtractionCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("-50.5218E19");
    Decimal128 result = d1.subtract(d2);
    Decimal128 expected("3.010218E21");
    ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128SubtractionCase2) {
    Decimal128 d1("1.00");
    Decimal128 d2("2.000");
    Decimal128 result = d1.subtract(d2);
    Decimal128 expected("-1.000");
    ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128MultiplicationCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("-50.5218E19");
    Decimal128 result = d1.multiply(d2);
    Decimal128 expected("-1.265571090E42");
    ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128MultiplicationCase2) {
    Decimal128 d1("1.00");
    Decimal128 d2("2.000");
    Decimal128 result = d1.multiply(d2);
    Decimal128 expected("2.00000");
    ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128DivisionCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("-50.5218E19");
    Decimal128 result = d1.divide(d2);
    Decimal128 expected("-4.958255644098191275845278671781290");
    ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128DivisionCase2) {
    Decimal128 d1("1.00");
    Decimal128 d2("2.000");
    Decimal128 result = d1.divide(d2);
    Decimal128 expected("0.5");
    ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
    ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128Quantize) {
    {
        Decimal128 expected("1.00001");
        Decimal128 val("1.000008");
        Decimal128 ref("0.00001");
        Decimal128 result = val.quantize(ref);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3.1");
        Decimal128 val("3.14159");
        Decimal128 ref("0.1");
        Decimal128 result = val.quantize(ref);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3.14");
        Decimal128 val("3.14159");
        Decimal128 ref("0.01");
        Decimal128 result = val.quantize(ref);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3.142");
        Decimal128 val("3.14159");
        Decimal128 ref("0.001");
        Decimal128 result = val.quantize(ref);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3.1416");
        Decimal128 val("3.14159");
        Decimal128 ref("0.0001");
        Decimal128 result = val.quantize(ref);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3.141");
        Decimal128 val("3.14159");
        Decimal128 ref("0.001");
        Decimal128 result = val.quantize(ref, Decimal128::kRoundTowardZero);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3.1415");
        Decimal128 val("3.14159");
        Decimal128 ref("0.0001");
        Decimal128 result = val.quantize(ref, Decimal128::kRoundTowardZero);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("30.1415");
        Decimal128 val("30.14159");
        Decimal128 ref("0.0001");
        Decimal128 result = val.quantize(ref, Decimal128::kRoundTowardZero);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("30.14159");
        Decimal128 val("30.14159");
        Decimal128 ref("0.00001");
        Decimal128 result = val.quantize(ref, Decimal128::kRoundTowardZero);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3000000000000000000000.141590000000");
        Decimal128 val("3000000000000000000000.14159000000");
        Decimal128 ref("0.000000000001");
        Decimal128 result = val.quantize(ref, Decimal128::kRoundTowardZero);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("30000000000000000000000.141590000000");
        Decimal128 val("30000000000000000000000.14159000000");
        Decimal128 ref("0.000000000001");
        Decimal128 result = val.quantize(ref, Decimal128::kRoundTowardZero);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3000000000000000000000000.141590000000");
        Decimal128 val("3000000000000000000000000.141590000");
        Decimal128 ref("0.000000000001");
        Decimal128 result = val.quantize(ref, Decimal128::kRoundTowardZero);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
    {
        Decimal128 expected("3000000000000000000000000");
        Decimal128 val("3000000000000000000000000.141590000");
        Decimal128 result = val.quantize(Decimal128::kNormalizedZero, Decimal128::kRoundTowardZero);
        ASSERT_EQUALS(result.getValue().low64, expected.getValue().low64);
        ASSERT_EQUALS(result.getValue().high64, expected.getValue().high64);
    }
}

TEST(Decimal128Test, TestDecimal128NormalizeSmallVals) {
    Decimal128 d1("500E-2");
    Decimal128 d2("5");
    Decimal128 d1Norm = d1.normalize();
    Decimal128 d2Norm = d2.normalize();
    ASSERT_EQUALS(d1Norm.getValue().low64, d2Norm.getValue().low64);
    ASSERT_EQUALS(d1Norm.getValue().high64, d2Norm.getValue().high64);
}

TEST(Decimal128Test, TestDecimal128NormalizeLargeVals) {
    Decimal128 d1("5E-6174");
    Decimal128 d2("500E-6176");
    Decimal128 d1Norm = d1.normalize();
    Decimal128 d2Norm = d2.normalize();
    ASSERT_EQUALS(d1Norm.getValue().low64, d2Norm.getValue().low64);
    ASSERT_EQUALS(d1Norm.getValue().high64, d2Norm.getValue().high64);
}

// Tests for Decimal128 comparison operations
TEST(Decimal128Test, TestDecimal128EqualCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("25.05E20");
    bool result = d1.isEqual(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128EqualCase2) {
    Decimal128 d1("1.00");
    Decimal128 d2("1.000000000");
    bool result = d1.isEqual(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128EqualCase3) {
    Decimal128 d1("0.1");
    Decimal128 d2("0.100000000000000005");
    bool result = d1.isEqual(d2);
    ASSERT_FALSE(result);
}

TEST(Decimal128Test, TestDecimal128EqualCase4) {
    Decimal128 d1("inf");
    Decimal128 d2("inf");
    bool result = d1.isEqual(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128NotEqualCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("25.06E20");
    bool result = d1.isNotEqual(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128NotEqualCase2) {
    Decimal128 d1("-25.0001E20");
    Decimal128 d2("-25.00010E20");
    bool result = d1.isNotEqual(d2);
    ASSERT_FALSE(result);
}

TEST(Decimal128Test, TestDecimal128GreaterCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("-25.05E20");
    bool result = d1.isGreater(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128GreaterCase2) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("25.05E20");
    bool result = d1.isGreater(d2);
    ASSERT_FALSE(result);
}

TEST(Decimal128Test, TestDecimal128GreaterCase3) {
    Decimal128 d1("-INFINITY");
    Decimal128 d2("+INFINITY");
    bool result = d1.isGreater(d2);
    ASSERT_FALSE(result);
}

TEST(Decimal128Test, TestDecimal128GreaterEqualCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("-25.05E20");
    bool result = d1.isGreaterEqual(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128GreaterEqualCase2) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("25.05E20");
    bool result = d1.isGreaterEqual(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128GreaterEqualCase3) {
    Decimal128 d1("-INFINITY");
    Decimal128 d2("+INFINITY");
    bool result = d1.isGreaterEqual(d2);
    ASSERT_FALSE(result);
}

TEST(Decimal128Test, TestDecimal128LessCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("-25.05E20");
    bool result = d1.isLess(d2);
    ASSERT_FALSE(result);
}

TEST(Decimal128Test, TestDecimal128LessCase2) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("25.05E20");
    bool result = d1.isLess(d2);
    ASSERT_FALSE(result);
}

TEST(Decimal128Test, TestDecimal128LessCase3) {
    Decimal128 d1("-INFINITY");
    Decimal128 d2("+INFINITY");
    bool result = d1.isLess(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128LessEqualCase1) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("-25.05E20");
    bool result = d1.isLessEqual(d2);
    ASSERT_FALSE(result);
}

TEST(Decimal128Test, TestDecimal128LessEqualCase2) {
    Decimal128 d1("25.05E20");
    Decimal128 d2("25.05E20");
    bool result = d1.isLessEqual(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128LessEqualCase3) {
    Decimal128 d1("-INFINITY");
    Decimal128 d2("+INFINITY");
    bool result = d1.isLessEqual(d2);
    ASSERT_TRUE(result);
}

TEST(Decimal128Test, TestDecimal128GetLargestPositive) {
    Decimal128 d = Decimal128::kLargestPositive;
    uint64_t largestPositiveDecimalHigh64 = 6917508178773903296ull;
    uint64_t largestPositveDecimalLow64 = 4003012203950112767ull;
    ASSERT_EQUALS(d.getValue().high64, largestPositiveDecimalHigh64);
    ASSERT_EQUALS(d.getValue().low64, largestPositveDecimalLow64);
}

TEST(Decimal128Test, TestDecimal128GetSmallestPositive) {
    Decimal128 d = Decimal128::kSmallestPositive;
    uint64_t smallestPositiveDecimalHigh64 = 0ull;
    uint64_t smallestPositiveDecimalLow64 = 1ull;
    ASSERT_EQUALS(d.getValue().high64, smallestPositiveDecimalHigh64);
    ASSERT_EQUALS(d.getValue().low64, smallestPositiveDecimalLow64);
}

TEST(Decimal128Test, TestDecimal128GetLargestNegative) {
    Decimal128 d = Decimal128::kLargestNegative;
    uint64_t largestNegativeDecimalHigh64 = 16140880215628679104ull;
    uint64_t largestNegativeDecimalLow64 = 4003012203950112767ull;
    ASSERT_EQUALS(d.getValue().high64, largestNegativeDecimalHigh64);
    ASSERT_EQUALS(d.getValue().low64, largestNegativeDecimalLow64);
}

TEST(Decimal128Test, TestDecimal128GetSmallestNegative) {
    Decimal128 d = Decimal128::kSmallestNegative;
    uint64_t smallestNegativeDecimalHigh64 = 9223372036854775808ull;
    uint64_t smallestNegativeDecimalLow64 = 1ull;
    ASSERT_EQUALS(d.getValue().high64, smallestNegativeDecimalHigh64);
    ASSERT_EQUALS(d.getValue().low64, smallestNegativeDecimalLow64);
}

TEST(Decimal128Test, TestDecimal128GetPosInfinity) {
    Decimal128 d = Decimal128::kPositiveInfinity;
    uint64_t decimalPositiveInfinityHigh64 = 8646911284551352320ull;
    uint64_t decimalPositiveInfinityLow64 = 0ull;
    ASSERT_EQUALS(d.getValue().high64, decimalPositiveInfinityHigh64);
    ASSERT_EQUALS(d.getValue().low64, decimalPositiveInfinityLow64);
}

TEST(Decimal128Test, TestDecimal128GetNegInfinity) {
    Decimal128 d = Decimal128::kNegativeInfinity;
    uint64_t decimalNegativeInfinityHigh64 = 17870283321406128128ull;
    uint64_t decimalNegativeInfinityLow64 = 0ull;
    ASSERT_EQUALS(d.getValue().high64, decimalNegativeInfinityHigh64);
    ASSERT_EQUALS(d.getValue().low64, decimalNegativeInfinityLow64);
}

TEST(Decimal128Test, TestDecimal128GetPosNaN) {
    Decimal128 d = Decimal128::kPositiveNaN;
    uint64_t decimalPositiveNaNHigh64 = 8935141660703064064ull;
    uint64_t decimalPositiveNaNLow64 = 0ull;
    ASSERT_EQUALS(d.getValue().high64, decimalPositiveNaNHigh64);
    ASSERT_EQUALS(d.getValue().low64, decimalPositiveNaNLow64);
}

TEST(Decimal128Test, TestDecimal128GetNegNaN) {
    Decimal128 d = Decimal128::kNegativeNaN;
    uint64_t decimalNegativeNaNHigh64 = 18158513697557839872ull;
    uint64_t decimalNegativeNaNLow64 = 0ull;
    ASSERT_EQUALS(d.getValue().high64, decimalNegativeNaNHigh64);
    ASSERT_EQUALS(d.getValue().low64, decimalNegativeNaNLow64);
}

TEST(Decimal128Test, TestDecimal128GetLargestNegativeExponentZero) {
    Decimal128 d = Decimal128::kLargestNegativeExponentZero;
    uint64_t largestNegativeExponentZeroHigh64 = 0ull;
    uint64_t largestNegativeExponentZeroLow64 = 0ull;
    ASSERT_EQUALS(d.getValue().high64, largestNegativeExponentZeroHigh64);
    ASSERT_EQUALS(d.getValue().low64, largestNegativeExponentZeroLow64);
}

/**
* Test data was generated using 64 bit versions of these functions, so we must test
* approximate results.
*/

void assertDecimal128ApproxEqual(Decimal128 x, Decimal128 y) {
    ASSERT_TRUE(x.subtract(y).toAbs().isLess(Decimal128("0.00000005")));
}

/**
 * A few tests need exact comparisons to test boundary conditions
 */

void assertDecimal128ExactlyEqual(Decimal128 x, Decimal128 y) {
    ASSERT_EQUALS(x.getValue().high64, y.getValue().high64);
    ASSERT_EQUALS(x.getValue().low64, y.getValue().low64);
}

TEST(Decimal128Test, TestExp) {
    assertDecimal128ApproxEqual(Decimal128("-1").exponential(),
                                Decimal128("0.3678794411714423215955237701614609"));
    assertDecimal128ApproxEqual(Decimal128("0").exponential(), Decimal128("1"));
    assertDecimal128ApproxEqual(Decimal128("1").exponential(),
                                Decimal128("2.718281828459045235360287471352662"));
    assertDecimal128ApproxEqual(Decimal128("1.5").exponential(),
                                Decimal128("4.481689070338064822602055460119276"));
    assertDecimal128ApproxEqual(Decimal128("1.79769313486231E+308")
                                    .exponential(Decimal128::RoundingMode::kRoundTowardNegative),
                                Decimal128("9.999999999999999999999999999999999E+6144"));
}

TEST(Decimal128Test, TestSqrt) {
    assertDecimal128ApproxEqual(Decimal128("0").squareRoot(), Decimal128("0"));
    assertDecimal128ApproxEqual(Decimal128("1").squareRoot(), Decimal128("1"));
    assertDecimal128ApproxEqual(Decimal128("25").squareRoot(), Decimal128("5"));
    assertDecimal128ApproxEqual(Decimal128("25.5").squareRoot(),
                                Decimal128("5.049752469181038976681692958534800"));
    assertDecimal128ApproxEqual(Decimal128("1.79769313486231E+308")
                                    .squareRoot(Decimal128::RoundingMode::kRoundTowardNegative),
                                Decimal128("1.340780792994257506864497209340836E+154"));
}

TEST(Decimal128Test, TestAsin) {
    assertDecimal128ApproxEqual(Decimal128("-1.0").asin(), Decimal128("-1.57079632679"));
    assertDecimal128ApproxEqual(Decimal128("-0.9").asin(), Decimal128("-1.119769515"));
    assertDecimal128ApproxEqual(Decimal128("-0.8").asin(), Decimal128("-0.927295218002"));
    assertDecimal128ApproxEqual(Decimal128("-0.7").asin(), Decimal128("-0.775397496611"));
    assertDecimal128ApproxEqual(Decimal128("-0.6").asin(), Decimal128("-0.643501108793"));
    assertDecimal128ApproxEqual(Decimal128("-0.5").asin(), Decimal128("-0.523598775598"));
    assertDecimal128ApproxEqual(Decimal128("-0.4").asin(), Decimal128("-0.411516846067"));
    assertDecimal128ApproxEqual(Decimal128("-0.3").asin(), Decimal128("-0.304692654015"));
    assertDecimal128ApproxEqual(Decimal128("-0.2").asin(), Decimal128("-0.20135792079"));
    assertDecimal128ApproxEqual(Decimal128("-0.1").asin(), Decimal128("-0.100167421162"));
    assertDecimal128ApproxEqual(Decimal128("0.0").asin(), Decimal128("0.0"));
    assertDecimal128ApproxEqual(Decimal128("0.1").asin(), Decimal128("0.100167421162"));
    assertDecimal128ApproxEqual(Decimal128("0.2").asin(), Decimal128("0.20135792079"));
    assertDecimal128ApproxEqual(Decimal128("0.3").asin(), Decimal128("0.304692654015"));
    assertDecimal128ApproxEqual(Decimal128("0.4").asin(), Decimal128("0.411516846067"));
    assertDecimal128ApproxEqual(Decimal128("0.5").asin(), Decimal128("0.523598775598"));
    assertDecimal128ApproxEqual(Decimal128("0.6").asin(), Decimal128("0.643501108793"));
    assertDecimal128ApproxEqual(Decimal128("0.7").asin(), Decimal128("0.775397496611"));
    assertDecimal128ApproxEqual(Decimal128("0.8").asin(), Decimal128("0.927295218002"));
    assertDecimal128ApproxEqual(Decimal128("0.9").asin(), Decimal128("1.119769515"));
    assertDecimal128ApproxEqual(Decimal128("1.0").asin(), Decimal128("1.57079632679"));
}

TEST(Decimal128Test, TestAcos) {
    // The intel decimal library has a bug at -1 where it returns 0.
    //
    // uncomment this test when we update to the new intel decimal library.
    // assertDecimal128ExactlyEqual(Decimal128("-1").acos(), Decimal128::kPi);
    assertDecimal128ExactlyEqual(Decimal128("-0.9999999999999999999999999999999997").acos(),
                                 Decimal128("3.141592653589793213967745955447722"));
    // Back to normal tests.
    assertDecimal128ApproxEqual(Decimal128("-0.9").acos(), Decimal128("2.69056584179"));
    assertDecimal128ApproxEqual(Decimal128("-0.8").acos(), Decimal128("2.4980915448"));
    assertDecimal128ApproxEqual(Decimal128("-0.7").acos(), Decimal128("2.34619382341"));
    assertDecimal128ApproxEqual(Decimal128("-0.6").acos(), Decimal128("2.21429743559"));
    assertDecimal128ApproxEqual(Decimal128("-0.5").acos(), Decimal128("2.09439510239"));
    assertDecimal128ApproxEqual(Decimal128("-0.4").acos(), Decimal128("1.98231317286"));
    assertDecimal128ApproxEqual(Decimal128("-0.3").acos(), Decimal128("1.87548898081"));
    assertDecimal128ApproxEqual(Decimal128("-0.2").acos(), Decimal128("1.77215424759"));
    assertDecimal128ApproxEqual(Decimal128("-0.1").acos(), Decimal128("1.67096374796"));
    assertDecimal128ApproxEqual(Decimal128("0.0").acos(), Decimal128("1.57079632679"));
    assertDecimal128ApproxEqual(Decimal128("0.1").acos(), Decimal128("1.47062890563"));
    assertDecimal128ApproxEqual(Decimal128("0.2").acos(), Decimal128("1.369438406"));
    assertDecimal128ApproxEqual(Decimal128("0.3").acos(), Decimal128("1.26610367278"));
    assertDecimal128ApproxEqual(Decimal128("0.4").acos(), Decimal128("1.15927948073"));
    assertDecimal128ApproxEqual(Decimal128("0.5").acos(), Decimal128("1.0471975512"));
    assertDecimal128ApproxEqual(Decimal128("0.6").acos(), Decimal128("0.927295218002"));
    assertDecimal128ApproxEqual(Decimal128("0.7").acos(), Decimal128("0.795398830184"));
    assertDecimal128ApproxEqual(Decimal128("0.8").acos(), Decimal128("0.643501108793"));
    assertDecimal128ApproxEqual(Decimal128("0.9").acos(), Decimal128("0.451026811796"));
    assertDecimal128ApproxEqual(Decimal128("1.0").acos(), Decimal128("0.0"));
}

TEST(Decimal128Test, TestAcosh) {
    assertDecimal128ApproxEqual(Decimal128("1.0").acosh(), Decimal128("0.0"));
    assertDecimal128ApproxEqual(Decimal128("1.1").acosh(), Decimal128("0.443568254385"));
    assertDecimal128ApproxEqual(Decimal128("1.5").acosh(), Decimal128("0.962423650119"));
    assertDecimal128ApproxEqual(Decimal128("2").acosh(), Decimal128("1.31695789692"));
    assertDecimal128ApproxEqual(Decimal128("2.5").acosh(), Decimal128("1.56679923697"));
    assertDecimal128ApproxEqual(Decimal128("3").acosh(), Decimal128("1.76274717404"));
}


TEST(Decimal128Test, TestAtanh) {
    assertDecimal128ApproxEqual(Decimal128("-0.9").atanh(), Decimal128("-1.47221948958"));
    assertDecimal128ApproxEqual(Decimal128("-0.8").atanh(), Decimal128("-1.09861228867"));
    assertDecimal128ApproxEqual(Decimal128("-0.7").atanh(), Decimal128("-0.867300527694"));
    assertDecimal128ApproxEqual(Decimal128("-0.6").atanh(), Decimal128("-0.69314718056"));
    assertDecimal128ApproxEqual(Decimal128("-0.5").atanh(), Decimal128("-0.549306144334"));
    assertDecimal128ApproxEqual(Decimal128("-0.4").atanh(), Decimal128("-0.423648930194"));
    assertDecimal128ApproxEqual(Decimal128("-0.3").atanh(), Decimal128("-0.309519604203"));
    assertDecimal128ApproxEqual(Decimal128("-0.2").atanh(), Decimal128("-0.202732554054"));
    assertDecimal128ApproxEqual(Decimal128("-0.1").atanh(), Decimal128("-0.100335347731"));
    assertDecimal128ApproxEqual(Decimal128("0.0").atanh(), Decimal128("0.0"));
    assertDecimal128ApproxEqual(Decimal128("0.1").atanh(), Decimal128("0.100335347731"));
    assertDecimal128ApproxEqual(Decimal128("0.2").atanh(), Decimal128("0.202732554054"));
    assertDecimal128ApproxEqual(Decimal128("0.3").atanh(), Decimal128("0.309519604203"));
    assertDecimal128ApproxEqual(Decimal128("0.4").atanh(), Decimal128("0.423648930194"));
    assertDecimal128ApproxEqual(Decimal128("0.5").atanh(), Decimal128("0.549306144334"));
    assertDecimal128ApproxEqual(Decimal128("0.6").atanh(), Decimal128("0.69314718056"));
    assertDecimal128ApproxEqual(Decimal128("0.7").atanh(), Decimal128("0.867300527694"));
    assertDecimal128ApproxEqual(Decimal128("0.8").atanh(), Decimal128("1.09861228867"));
    assertDecimal128ApproxEqual(Decimal128("0.9").atanh(), Decimal128("1.47221948958"));
}

TEST(Decimal128Test, TestAtan) {
    assertDecimal128ApproxEqual(Decimal128("-1.5").atan(), Decimal128("-0.982793723247"));
    assertDecimal128ApproxEqual(Decimal128("-1.0471975512").atan(), Decimal128("-0.80844879263"));
    assertDecimal128ApproxEqual(Decimal128("-0.785398163397").atan(),
                                Decimal128("-0.665773750028"));
    assertDecimal128ApproxEqual(Decimal128("0").atan(), Decimal128("0.0"));
    assertDecimal128ApproxEqual(Decimal128("0.785398163397").atan(), Decimal128("0.665773750028"));
    assertDecimal128ApproxEqual(Decimal128("1.0471975512").atan(), Decimal128("0.80844879263"));
    assertDecimal128ApproxEqual(Decimal128("1.5").atan(), Decimal128("0.982793723247"));
}

TEST(Decimal128Test, TestAtan2) {
    assertDecimal128ApproxEqual(Decimal128("1.0").atan2(Decimal128("0.0")),
                                Decimal128("1.57079632679"));
    assertDecimal128ApproxEqual(Decimal128("0.866025403784").atan2(Decimal128("0.5")),
                                Decimal128("1.0471975512"));
    assertDecimal128ApproxEqual(Decimal128("0.707106781187").atan2(Decimal128("0.707106781187")),
                                Decimal128("0.785398163397"));
    assertDecimal128ApproxEqual(Decimal128("0.5").atan2(Decimal128("0.866025403784")),
                                Decimal128("0.523598775598"));
    assertDecimal128ApproxEqual(Decimal128("6.12323399574e-17").atan2(Decimal128("1.0")),
                                Decimal128("6.12323399574e-17"));
    assertDecimal128ApproxEqual(Decimal128("-0.5").atan2(Decimal128("0.866025403784")),
                                Decimal128("-0.523598775598"));
    assertDecimal128ApproxEqual(Decimal128("-0.707106781187").atan2(Decimal128("0.707106781187")),
                                Decimal128("-0.785398163397"));
    assertDecimal128ApproxEqual(Decimal128("-0.866025403784").atan2(Decimal128("0.5")),
                                Decimal128("-1.0471975512"));
    assertDecimal128ApproxEqual(Decimal128("-1.0").atan2(Decimal128("1.22464679915e-16")),
                                Decimal128("-1.57079632679"));
    assertDecimal128ApproxEqual(Decimal128("-0.866025403784").atan2(Decimal128("-0.5")),
                                Decimal128("-2.09439510239"));
    assertDecimal128ApproxEqual(Decimal128("-0.707106781187").atan2(Decimal128("-0.707106781187")),
                                Decimal128("-2.35619449019"));
    assertDecimal128ApproxEqual(Decimal128("-0.5").atan2(Decimal128("-0.866025403784")),
                                Decimal128("-2.61799387799"));
    assertDecimal128ApproxEqual(Decimal128("-1.83697019872e-16").atan2(Decimal128("-1.0")),
                                Decimal128("-3.14159265359"));
    assertDecimal128ApproxEqual(Decimal128("0.5").atan2(Decimal128("-0.866025403784")),
                                Decimal128("2.61799387799"));
    assertDecimal128ApproxEqual(Decimal128("0.707106781187").atan2(Decimal128("-0.707106781187")),
                                Decimal128("2.35619449019"));
    assertDecimal128ApproxEqual(Decimal128("0.866025403784").atan2(Decimal128("-0.5")),
                                Decimal128("2.09439510239"));
    assertDecimal128ApproxEqual(Decimal128("1.0").atan2(Decimal128("-2.44929359829e-16")),
                                Decimal128("1.57079632679"));
}

TEST(Decimal128Test, TestCos) {
    assertDecimal128ApproxEqual(Decimal128("0.0").cos(), Decimal128("1.0"));
    assertDecimal128ApproxEqual(Decimal128("0.523598775598").cos(), Decimal128("0.866025403784"));
    assertDecimal128ApproxEqual(Decimal128("0.785398163397").cos(), Decimal128("0.707106781187"));
    assertDecimal128ApproxEqual(Decimal128("1.0471975512").cos(), Decimal128("0.5"));
    assertDecimal128ApproxEqual(Decimal128("1.57079632679").cos(), Decimal128("6.12323399574e-17"));
    assertDecimal128ApproxEqual(Decimal128("2.09439510239").cos(), Decimal128("-0.5"));
    assertDecimal128ApproxEqual(Decimal128("2.35619449019").cos(), Decimal128("-0.707106781187"));
    assertDecimal128ApproxEqual(Decimal128("2.61799387799").cos(), Decimal128("-0.866025403784"));
    assertDecimal128ApproxEqual(Decimal128("3.14159265359").cos(), Decimal128("-1.0"));
    assertDecimal128ApproxEqual(Decimal128("3.66519142919").cos(), Decimal128("-0.866025403784"));
    assertDecimal128ApproxEqual(Decimal128("3.92699081699").cos(), Decimal128("-0.707106781187"));
    assertDecimal128ApproxEqual(Decimal128("4.18879020479").cos(), Decimal128("-0.5"));
    assertDecimal128ApproxEqual(Decimal128("4.71238898038").cos(),
                                Decimal128("-1.83697019872e-16"));
    assertDecimal128ApproxEqual(Decimal128("5.23598775598").cos(), Decimal128("0.5"));
    assertDecimal128ApproxEqual(Decimal128("5.49778714378").cos(), Decimal128("0.707106781187"));
    assertDecimal128ApproxEqual(Decimal128("5.75958653158").cos(), Decimal128("0.866025403784"));
    assertDecimal128ApproxEqual(Decimal128("6.28318530718").cos(), Decimal128("1.0"));
}

TEST(Decimal128Test, TestCosh) {
    assertDecimal128ApproxEqual(Decimal128("0.0").cosh(), Decimal128("1.0"));
    assertDecimal128ApproxEqual(Decimal128("0.523598775598").cosh(), Decimal128("1.14023832108"));
    assertDecimal128ApproxEqual(Decimal128("0.785398163397").cosh(), Decimal128("1.32460908925"));
    assertDecimal128ApproxEqual(Decimal128("1.0471975512").cosh(), Decimal128("1.6002868577"));
    assertDecimal128ApproxEqual(Decimal128("1.57079632679").cosh(), Decimal128("2.50917847866"));
    assertDecimal128ApproxEqual(Decimal128("2.09439510239").cosh(), Decimal128("4.12183605387"));
    assertDecimal128ApproxEqual(Decimal128("2.35619449019").cosh(), Decimal128("5.32275214952"));
    assertDecimal128ApproxEqual(Decimal128("2.61799387799").cosh(), Decimal128("6.89057236498"));
    assertDecimal128ApproxEqual(Decimal128("3.14159265359").cosh(), Decimal128("11.5919532755"));
    assertDecimal128ApproxEqual(Decimal128("3.66519142919").cosh(), Decimal128("19.5446063168"));
    assertDecimal128ApproxEqual(Decimal128("3.92699081699").cosh(), Decimal128("25.3868611924"));
    assertDecimal128ApproxEqual(Decimal128("4.18879020479").cosh(), Decimal128("32.97906491"));
    assertDecimal128ApproxEqual(Decimal128("4.71238898038").cosh(), Decimal128("55.6633808904"));
    assertDecimal128ApproxEqual(Decimal128("5.23598775598").cosh(), Decimal128("93.9599750339"));
    assertDecimal128ApproxEqual(Decimal128("5.49778714378").cosh(), Decimal128("122.07757934"));
    assertDecimal128ApproxEqual(Decimal128("5.75958653158").cosh(), Decimal128("158.610147472"));
    assertDecimal128ApproxEqual(Decimal128("6.28318530718").cosh(), Decimal128("267.746761484"));
}

TEST(Decimal128Test, TestSin) {
    assertDecimal128ApproxEqual(Decimal128("0.0").sin(), Decimal128("0.0"));
    assertDecimal128ApproxEqual(Decimal128("0.523598775598").sin(), Decimal128("0.5"));
    assertDecimal128ApproxEqual(Decimal128("0.785398163397").sin(), Decimal128("0.707106781187"));
    assertDecimal128ApproxEqual(Decimal128("1.0471975512").sin(), Decimal128("0.866025403784"));
    assertDecimal128ApproxEqual(Decimal128("1.57079632679").sin(), Decimal128("1.0"));
    assertDecimal128ApproxEqual(Decimal128("2.09439510239").sin(), Decimal128("0.866025403784"));
    assertDecimal128ApproxEqual(Decimal128("2.35619449019").sin(), Decimal128("0.707106781187"));
    assertDecimal128ApproxEqual(Decimal128("2.61799387799").sin(), Decimal128("0.5"));
    assertDecimal128ApproxEqual(Decimal128("3.14159265359").sin(), Decimal128("1.22464679915e-16"));
    assertDecimal128ApproxEqual(Decimal128("3.66519142919").sin(), Decimal128("-0.5"));
    assertDecimal128ApproxEqual(Decimal128("3.92699081699").sin(), Decimal128("-0.707106781187"));
    assertDecimal128ApproxEqual(Decimal128("4.18879020479").sin(), Decimal128("-0.866025403784"));
    assertDecimal128ApproxEqual(Decimal128("4.71238898038").sin(), Decimal128("-1.0"));
    assertDecimal128ApproxEqual(Decimal128("5.23598775598").sin(), Decimal128("-0.866025403784"));
    assertDecimal128ApproxEqual(Decimal128("5.49778714378").sin(), Decimal128("-0.707106781187"));
    assertDecimal128ApproxEqual(Decimal128("5.75958653158").sin(), Decimal128("-0.5"));
    assertDecimal128ApproxEqual(Decimal128("6.28318530718").sin(),
                                Decimal128("-2.44929359829e-16"));
}

TEST(Decimal128Test, TestSinh) {
    assertDecimal128ApproxEqual(Decimal128("0.0").sinh(), Decimal128("0.0"));
    assertDecimal128ApproxEqual(Decimal128("0.523598775598").sinh(), Decimal128("0.547853473888"));
    assertDecimal128ApproxEqual(Decimal128("0.785398163397").sinh(), Decimal128("0.868670961486"));
    assertDecimal128ApproxEqual(Decimal128("1.0471975512").sinh(), Decimal128("1.24936705052"));
    assertDecimal128ApproxEqual(Decimal128("1.57079632679").sinh(), Decimal128("2.30129890231"));
    assertDecimal128ApproxEqual(Decimal128("2.09439510239").sinh(), Decimal128("3.9986913428"));
    assertDecimal128ApproxEqual(Decimal128("2.35619449019").sinh(), Decimal128("5.22797192468"));
    assertDecimal128ApproxEqual(Decimal128("2.61799387799").sinh(), Decimal128("6.81762330413"));
    assertDecimal128ApproxEqual(Decimal128("3.14159265359").sinh(), Decimal128("11.5487393573"));
    assertDecimal128ApproxEqual(Decimal128("3.66519142919").sinh(), Decimal128("19.5190070464"));
    assertDecimal128ApproxEqual(Decimal128("3.92699081699").sinh(), Decimal128("25.3671583194"));
    assertDecimal128ApproxEqual(Decimal128("4.18879020479").sinh(), Decimal128("32.9639002901"));
    assertDecimal128ApproxEqual(Decimal128("4.71238898038").sinh(), Decimal128("55.6543975994"));
    assertDecimal128ApproxEqual(Decimal128("5.23598775598").sinh(), Decimal128("93.9546534685"));
    assertDecimal128ApproxEqual(Decimal128("5.49778714378").sinh(), Decimal128("122.073483515"));
    assertDecimal128ApproxEqual(Decimal128("5.75958653158").sinh(), Decimal128("158.606995057"));
    assertDecimal128ApproxEqual(Decimal128("6.28318530718").sinh(), Decimal128("267.744894041"));
}

TEST(Decimal128Test, TestTan) {
    assertDecimal128ApproxEqual(Decimal128("-1.5").tan(), Decimal128("-14.1014199472"));
    assertDecimal128ApproxEqual(Decimal128("-1.0471975512").tan(), Decimal128("-1.73205080757"));
    assertDecimal128ApproxEqual(Decimal128("-0.785398163397").tan(), Decimal128("-1.0"));
    assertDecimal128ApproxEqual(Decimal128("0").tan(), Decimal128("0.0"));
    assertDecimal128ApproxEqual(Decimal128("0.785398163397").tan(), Decimal128("1.0"));
    assertDecimal128ApproxEqual(Decimal128("1.0471975512").tan(), Decimal128("1.73205080757"));
    assertDecimal128ApproxEqual(Decimal128("1.5").tan(), Decimal128("14.1014199472"));
}

TEST(Decimal128Test, TestTanh) {
    assertDecimal128ApproxEqual(Decimal128("0.0").tanh(), Decimal128("0.0"));
    assertDecimal128ApproxEqual(Decimal128("0.523598775598").tanh(), Decimal128("0.480472778156"));
    assertDecimal128ApproxEqual(Decimal128("0.785398163397").tanh(), Decimal128("0.655794202633"));
    assertDecimal128ApproxEqual(Decimal128("1.0471975512").tanh(), Decimal128("0.780714435359"));
    assertDecimal128ApproxEqual(Decimal128("1.57079632679").tanh(), Decimal128("0.917152335667"));
    assertDecimal128ApproxEqual(Decimal128("2.09439510239").tanh(), Decimal128("0.970123821166"));
    assertDecimal128ApproxEqual(Decimal128("2.35619449019").tanh(), Decimal128("0.982193380007"));
    assertDecimal128ApproxEqual(Decimal128("2.61799387799").tanh(), Decimal128("0.989413207353"));
    assertDecimal128ApproxEqual(Decimal128("3.14159265359").tanh(), Decimal128("0.996272076221"));
    assertDecimal128ApproxEqual(Decimal128("3.66519142919").tanh(), Decimal128("0.998690213046"));
    assertDecimal128ApproxEqual(Decimal128("3.92699081699").tanh(), Decimal128("0.999223894879"));
    assertDecimal128ApproxEqual(Decimal128("4.18879020479").tanh(), Decimal128("0.999540174353"));
    assertDecimal128ApproxEqual(Decimal128("4.71238898038").tanh(), Decimal128("0.999838613989"));
    assertDecimal128ApproxEqual(Decimal128("5.23598775598").tanh(), Decimal128("0.999943363486"));
    assertDecimal128ApproxEqual(Decimal128("5.49778714378").tanh(), Decimal128("0.999966449"));
    assertDecimal128ApproxEqual(Decimal128("5.75958653158").tanh(), Decimal128("0.99998012476"));
    assertDecimal128ApproxEqual(Decimal128("6.28318530718").tanh(), Decimal128("0.99999302534"));
}
}  // namespace mongo
