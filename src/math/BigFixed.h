#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fb {

// Signed arbitrary-precision fixed-point number: one 32-bit integer limb plus
// `fracLimbs()` 32-bit fraction limbs (so |value| < 2^32, resolution 2^-32n).
// Used for deep-zoom view centers and reference orbits; values stay small there.
class BigFixed {
public:
    BigFixed() : BigFixed(0.0) {}
    explicit BigFixed(double value, int fracLimbs = 2);

    // Parses "[-]digits[.digits]"; returns nullopt on malformed input.
    static std::optional<BigFixed> parse(std::string_view text, int fracLimbs);

    // Fraction limbs needed to resolve steps of `resolution` with ~64 bits to spare.
    static int limbsFor(double resolution);

    int fracLimbs() const { return int(m_mag.size()) - 1; }
    // Changes precision in place (truncates toward zero when shrinking).
    void setFracLimbs(int n);

    double toDouble() const;
    std::string toString(int fractionDigits) const;
    bool isZero() const;

    BigFixed& operator+=(const BigFixed& o);
    BigFixed& operator-=(const BigFixed& o);
    BigFixed& operator+=(double v) { return *this += BigFixed(v, fracLimbs()); }
    BigFixed& operator-=(double v) { return *this -= BigFixed(v, fracLimbs()); }
    BigFixed& mul2();

    friend BigFixed operator+(BigFixed a, const BigFixed& b) { return a += b; }
    friend BigFixed operator-(BigFixed a, const BigFixed& b) { return a -= b; }
    // Result has the larger precision of the two operands; truncated, not rounded.
    friend BigFixed operator*(const BigFixed& a, const BigFixed& b);

private:
    void addSigned(const BigFixed& o, bool negateOther);
    void normalizeZero();

    std::vector<uint32_t> m_mag; // little-endian; back() is the integer limb
    bool m_neg = false;
};

} // namespace fb
