#include "math/BigFixed.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace fb {

namespace {

// Compares magnitudes of equal-length limb vectors.
int compareMag(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    for (size_t i = a.size(); i-- > 0;) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

// a += b (equal length); overflow past the integer limb is dropped.
void addMag(std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    uint64_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        const uint64_t t = uint64_t(a[i]) + b[i] + carry;
        a[i] = uint32_t(t);
        carry = t >> 32;
    }
}

// a = big - small (equal length, |big| >= |small|).
void subMag(std::vector<uint32_t>& out, const std::vector<uint32_t>& big, const std::vector<uint32_t>& small) {
    int64_t borrow = 0;
    for (size_t i = 0; i < big.size(); ++i) {
        int64_t t = int64_t(big[i]) - small[i] - borrow;
        borrow = t < 0;
        if (t < 0) t += int64_t(1) << 32;
        out[i] = uint32_t(t);
    }
}

} // namespace

BigFixed::BigFixed(double value, int fracLimbs) : m_mag(size_t(std::max(fracLimbs, 1)) + 1, 0) {
    m_neg = value < 0.0;
    double v = std::fabs(value);
    assert(v < 4294967296.0);
    const double ip = std::floor(v);
    m_mag.back() = uint32_t(ip);
    v -= ip;
    for (size_t i = m_mag.size() - 1; i-- > 0 && v > 0.0;) {
        v *= 4294967296.0;
        const double limb = std::floor(v);
        m_mag[i] = uint32_t(limb);
        v -= limb;
    }
    normalizeZero();
}

std::optional<BigFixed> BigFixed::parse(std::string_view text, int fracLimbs) {
    while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
    bool neg = false;
    if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
        neg = text.front() == '-';
        text.remove_prefix(1);
    }
    const size_t dot = text.find('.');
    const std::string_view intPart = text.substr(0, dot);
    const std::string_view fracPart = dot == std::string_view::npos ? std::string_view{} : text.substr(dot + 1);
    if (intPart.empty() && fracPart.empty()) return std::nullopt;

    uint64_t integer = 0;
    for (char ch : intPart) {
        if (ch < '0' || ch > '9') return std::nullopt;
        integer = integer * 10 + uint64_t(ch - '0');
        if (integer > 0xffffffffull) return std::nullopt;
    }

    // One guard limb absorbs the truncation of the divisions below; rounded off at the end.
    BigFixed r(0.0, fracLimbs + 1);
    // Horner from the last digit: x = (digit + x) / 10, accumulating 0.d1d2d3...
    for (size_t k = fracPart.size(); k-- > 0;) {
        const char ch = fracPart[k];
        if (ch < '0' || ch > '9') return std::nullopt;
        r.m_mag.back() = uint32_t(ch - '0');
        uint64_t rem = 0;
        for (size_t i = r.m_mag.size(); i-- > 0;) {
            const uint64_t cur = (rem << 32) | r.m_mag[i];
            r.m_mag[i] = uint32_t(cur / 10);
            rem = cur % 10;
        }
    }
    r.m_mag.back() = uint32_t(integer);
    BigFixed half(0.0, fracLimbs + 1);
    half.m_mag[0] = 0x80000000u;
    addMag(r.m_mag, half.m_mag);
    r.setFracLimbs(fracLimbs);
    r.m_neg = neg;
    r.normalizeZero();
    return r;
}

int BigFixed::limbsFor(double resolution) {
    const double bits = resolution > 0.0 ? -std::log2(resolution) : 64.0;
    return std::max(2, int(std::ceil((std::max(bits, 0.0) + 64.0) / 32.0)));
}

void BigFixed::setFracLimbs(int n) {
    n = std::max(n, 1);
    const int cur = fracLimbs();
    if (n > cur) {
        m_mag.insert(m_mag.begin(), size_t(n - cur), 0u);
    } else if (n < cur) {
        m_mag.erase(m_mag.begin(), m_mag.begin() + (cur - n));
        normalizeZero();
    }
}

double BigFixed::toDouble() const {
    // Only the top few non-zero limbs matter for a 53-bit mantissa.
    const int frac = fracLimbs();
    double r = 0.0;
    int used = 0;
    for (size_t i = m_mag.size(); i-- > 0 && used < 3;) {
        if (m_mag[i] == 0 && used == 0) continue;
        r += std::ldexp(double(m_mag[i]), 32 * (int(i) - frac));
        ++used;
    }
    return m_neg ? -r : r;
}

std::string BigFixed::toString(int fractionDigits) const {
    fractionDigits = std::max(fractionDigits, 0);
    // Round the magnitude to the last printed digit by adding half a unit of it.
    std::vector<uint32_t> f(m_mag.begin(), m_mag.end());
    if (auto half = parse("0." + std::string(size_t(fractionDigits), '0') + "5", fracLimbs())) {
        addMag(f, half->m_mag);
    }
    std::string s = m_neg && !isZero() ? "-" : "";
    s += std::to_string(f.back());
    if (fractionDigits == 0) return s;
    s += '.';
    for (int d = 0; d < fractionDigits; ++d) {
        f.back() = 0;
        uint64_t carry = 0;
        for (size_t i = 0; i < f.size(); ++i) {
            const uint64_t t = uint64_t(f[i]) * 10 + carry;
            f[i] = uint32_t(t);
            carry = t >> 32;
        }
        s += char('0' + f.back());
    }
    return s;
}

bool BigFixed::isZero() const {
    return std::all_of(m_mag.begin(), m_mag.end(), [](uint32_t l) { return l == 0; });
}

void BigFixed::normalizeZero() {
    if (isZero()) m_neg = false;
}

void BigFixed::addSigned(const BigFixed& o, bool negateOther) {
    const BigFixed* other = &o;
    BigFixed widened;
    if (o.fracLimbs() != fracLimbs()) {
        if (o.fracLimbs() > fracLimbs()) setFracLimbs(o.fracLimbs());
        if (o.fracLimbs() < fracLimbs()) {
            widened = o;
            widened.setFracLimbs(fracLimbs());
            other = &widened;
        }
    }
    const bool otherNeg = other->m_neg != negateOther;
    if (m_neg == otherNeg) {
        addMag(m_mag, other->m_mag);
    } else if (compareMag(m_mag, other->m_mag) >= 0) {
        subMag(m_mag, m_mag, other->m_mag);
    } else {
        subMag(m_mag, other->m_mag, m_mag);
        m_neg = otherNeg;
    }
    normalizeZero();
}

BigFixed& BigFixed::operator+=(const BigFixed& o) {
    addSigned(o, false);
    return *this;
}

BigFixed& BigFixed::operator-=(const BigFixed& o) {
    addSigned(o, true);
    return *this;
}

BigFixed& BigFixed::mul2() {
    uint32_t carry = 0;
    for (uint32_t& limb : m_mag) {
        const uint32_t next = limb >> 31;
        limb = (limb << 1) | carry;
        carry = next;
    }
    return *this;
}

BigFixed operator*(const BigFixed& a, const BigFixed& b) {
    if (a.fracLimbs() != b.fracLimbs()) {
        BigFixed wa = a, wb = b;
        const int n = std::max(a.fracLimbs(), b.fracLimbs());
        wa.setFracLimbs(n);
        wb.setFracLimbs(n);
        return wa * wb;
    }
    const size_t n = a.m_mag.size();
    const size_t frac = n - 1;
    thread_local std::vector<uint32_t> prod;
    prod.assign(2 * n, 0u);
    for (size_t i = 0; i < n; ++i) {
        const uint64_t ai = a.m_mag[i];
        if (ai == 0) continue;
        uint64_t carry = 0;
        for (size_t j = 0; j < n; ++j) {
            const uint64_t t = ai * b.m_mag[j] + prod[i + j] + carry;
            prod[i + j] = uint32_t(t);
            carry = t >> 32;
        }
        prod[i + n] = uint32_t(carry);
    }
    BigFixed r(0.0, int(frac));
    std::copy(prod.begin() + ptrdiff_t(frac), prod.begin() + ptrdiff_t(frac + n), r.m_mag.begin());
    r.m_neg = a.m_neg != b.m_neg;
    r.normalizeZero();
    return r;
}

} // namespace fb
