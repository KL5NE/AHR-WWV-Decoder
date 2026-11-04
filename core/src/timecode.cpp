#include "ahrwwv/timecode.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ahrwwv {

namespace {

// Bit positions (seconds) of each field, least significant bit first.
const int kYearUnits[] = {4, 5, 6, 7};
const int kMinuteUnits[] = {10, 11, 12, 13};
const int kMinuteTens[] = {15, 16, 17};
const int kHourUnits[] = {20, 21, 22, 23};
const int kHourTens[] = {25, 26};
const int kDoyUnits[] = {30, 31, 32, 33};
const int kDoyTens[] = {35, 36, 37, 38};
const int kDoyHundreds[] = {40, 41};
const int kYearTens[] = {51, 52, 53, 54};
const int kDut1[] = {56, 57, 58};
const int kDst2 = 2;
const int kLsw = 3;
const int kUt1Sign = 50;
const int kDst1 = 55;

bool isMarkerSecond(int s) { return s % 10 == 9; }

// Seconds that always carry a binary 0.
bool isFixedZero(int s)
{
    switch (s) {
    case 1: case 8: case 14: case 18: case 24: case 27: case 28: case 34:
    case 42: case 43: case 44: case 45: case 46: case 47: case 48:
        return true;
    default:
        return false;
    }
}

template <size_t N>
void putBcd(Frame& f, const int (&pos)[N], int value)
{
    for (size_t i = 0; i < N; i++)
        f[pos[i]] = ((value >> i) & 1) ? Symbol::One : Symbol::Zero;
}

template <size_t N>
int getBcd(const Frame& f, const int (&pos)[N], bool& bad)
{
    int v = 0;
    for (size_t i = 0; i < N; i++) {
        Symbol s = f[pos[i]];
        if (s == Symbol::One) v |= (1 << i);
        else if (s != Symbol::Zero) bad = true;
    }
    return v;
}

} // namespace

char symbolChar(Symbol s)
{
    switch (s) {
    case Symbol::None: return '_';
    case Symbol::Zero: return '0';
    case Symbol::One: return '1';
    case Symbol::Marker: return 'M';
    default: return '?';
    }
}

DstState dstState(const TimeCode& tc)
{
    if (tc.dst1 && tc.dst2) return DstState::Daylight;
    if (!tc.dst1 && !tc.dst2) return DstState::Standard;
    if (tc.dst1) return DstState::BeginsToday;
    return DstState::EndsToday;
}

const char* dstStateName(DstState s)
{
    switch (s) {
    case DstState::Standard: return "Standard time";
    case DstState::Daylight: return "Daylight time";
    case DstState::BeginsToday: return "DST begins today";
    case DstState::EndsToday: return "DST ends today";
    }
    return "?";
}

Frame encodeFrame(const TimeCode& tc)
{
    Frame f;
    f.fill(Symbol::Zero);
    f[0] = Symbol::None;
    for (int s = 9; s < 60; s += 10) f[s] = Symbol::Marker;
    int y2 = tc.year % 100;
    putBcd(f, kYearUnits, y2 % 10);
    putBcd(f, kYearTens, y2 / 10);
    putBcd(f, kMinuteUnits, tc.minute % 10);
    putBcd(f, kMinuteTens, tc.minute / 10);
    putBcd(f, kHourUnits, tc.hour % 10);
    putBcd(f, kHourTens, tc.hour / 10);
    putBcd(f, kDoyUnits, tc.dayOfYear % 10);
    putBcd(f, kDoyTens, (tc.dayOfYear / 10) % 10);
    putBcd(f, kDoyHundreds, tc.dayOfYear / 100);
    putBcd(f, kDut1, std::abs(tc.dut1Tenths));
    f[kUt1Sign] = tc.dut1Tenths >= 0 ? Symbol::One : Symbol::Zero;
    f[kLsw] = tc.leapSecondWarning ? Symbol::One : Symbol::Zero;
    f[kDst1] = tc.dst1 ? Symbol::One : Symbol::Zero;
    f[kDst2] = tc.dst2 ? Symbol::One : Symbol::Zero;
    return f;
}

DecodeResult decodeFrame(const Frame& f)
{
    DecodeResult r;
    for (Symbol s : f)
        if (s == Symbol::Unknown) r.unknownSymbols++;
    if (r.unknownSymbols)
        r.problems.push_back(std::to_string(r.unknownSymbols) + " unreadable second(s)");

    if (f[0] != Symbol::None && f[0] != Symbol::Unknown)
        r.problems.push_back("pulse present in second 0");
    for (int s = 1; s < 60; s++) {
        if (isMarkerSecond(s)) {
            if (f[s] != Symbol::Marker && f[s] != Symbol::Unknown)
                r.problems.push_back("missing position marker at second " + std::to_string(s));
        } else if (f[s] == Symbol::Marker) {
            r.problems.push_back("unexpected marker at second " + std::to_string(s));
        } else if (f[s] == Symbol::None) {
            r.problems.push_back("missing pulse at second " + std::to_string(s));
        } else if (isFixedZero(s) && f[s] == Symbol::One) {
            r.problems.push_back("unused bit " + std::to_string(s) + " is 1");
        }
    }

    bool bad = false;
    int yu = getBcd(f, kYearUnits, bad), yt = getBcd(f, kYearTens, bad);
    int mu = getBcd(f, kMinuteUnits, bad), mt = getBcd(f, kMinuteTens, bad);
    int hu = getBcd(f, kHourUnits, bad), ht = getBcd(f, kHourTens, bad);
    int du = getBcd(f, kDoyUnits, bad), dt = getBcd(f, kDoyTens, bad), dh = getBcd(f, kDoyHundreds, bad);
    int dut = getBcd(f, kDut1, bad);
    bool flagBad = false;
    auto flag = [&](int s) {
        if (f[s] == Symbol::One) return true;
        if (f[s] != Symbol::Zero) flagBad = true;
        return false;
    };
    bool sign = flag(kUt1Sign);
    r.tc.leapSecondWarning = flag(kLsw);
    r.tc.dst1 = flag(kDst1);
    r.tc.dst2 = flag(kDst2);
    if (bad || flagBad) r.problems.push_back("data bit not readable as 0/1");

    if (yu > 9 || yt > 9 || mu > 9 || hu > 9 || du > 9 || dt > 9)
        r.problems.push_back("BCD digit greater than 9");
    if (mt > 5) r.problems.push_back("minute tens > 5");
    if (ht > 2) r.problems.push_back("hour tens > 2");
    if (dh > 3) r.problems.push_back("day-of-year hundreds > 3");

    r.tc.year = 2000 + yt * 10 + yu;
    r.tc.minute = mt * 10 + mu;
    r.tc.hour = ht * 10 + hu;
    r.tc.dayOfYear = dh * 100 + dt * 10 + du;
    r.tc.dut1Tenths = sign ? dut : -dut;

    if (r.tc.minute > 59) r.problems.push_back("minute out of range");
    if (r.tc.hour > 23) r.problems.push_back("hour out of range");
    int maxDoy = isLeapYear(r.tc.year) ? 366 : 365;
    if (r.tc.dayOfYear < 1 || r.tc.dayOfYear > maxDoy) r.problems.push_back("day of year out of range");

    r.valid = r.problems.empty();
    return r;
}

DecodeResult decodeFrameSoft(const SoftFrame& c, double minFieldMargin)
{
    DecodeResult r;
    auto argmin = [&](int p) {
        int b = 0;
        for (int h = 1; h < 4; h++) if (c[p][h] < c[p][b]) b = h;
        return b;
    };
    // Only count a clear contradiction as a structure error. Weak skywave
    // pulses can marginally favor a forbidden symbol without being reliable.
    const double kStructMargin = 0.5;
    for (int p = 0; p < 60; p++) {
        bool allowed[4] = {false, false, false, false};
        if (p == 0) allowed[0] = true;
        else if (isMarkerSecond(p)) allowed[3] = true;
        else if (isFixedZero(p)) allowed[1] = true;
        else { allowed[1] = true; allowed[2] = true; }
        double bestAllowed = 1e300, bestForbidden = 1e300;
        for (int h = 0; h < 4; h++) {
            if (allowed[h]) bestAllowed = std::min(bestAllowed, c[p][h]);
            else bestForbidden = std::min(bestForbidden, c[p][h]);
        }
        if (bestForbidden + kStructMargin < bestAllowed) r.structureErrors++;
    }
    auto bitCost = [&](int p, int bit) { return c[p][bit ? 2 : 1]; };
    // cost of writing `value` into the listed bit positions as BCD
    auto bcdCost = [&](const int* pos, int n, int value) {
        double s = 0;
        for (int i = 0; i < n; i++) s += bitCost(pos[i], (value >> i) & 1);
        return s;
    };
    r.minMargin = 1e300;
    auto pick = [&](const char* name, int lo, int hi, auto costFn) {
        double best = 1e300, second = 1e300;
        int bv = lo;
        for (int v = lo; v <= hi; v++) {
            double x = costFn(v);
            if (x < best) { second = best; best = x; bv = v; }
            else if (x < second) second = x;
        }
        double m = second - best;
        if (m < r.minMargin) { r.minMargin = m; r.weakestField = name; }
        return bv;
    };
    int y2 = pick("year", 0, 99, [&](int v) {
        return bcdCost(kYearUnits, 4, v % 10) + bcdCost(kYearTens, 4, v / 10);
    });
    r.tc.year = 2000 + y2;
    r.tc.minute = pick("minute", 0, 59, [&](int v) {
        return bcdCost(kMinuteUnits, 4, v % 10) + bcdCost(kMinuteTens, 3, v / 10);
    });
    r.tc.hour = pick("hour", 0, 23, [&](int v) {
        return bcdCost(kHourUnits, 4, v % 10) + bcdCost(kHourTens, 2, v / 10);
    });
    r.tc.dayOfYear = pick("day of year", 1, isLeapYear(r.tc.year) ? 366 : 365, [&](int v) {
        return bcdCost(kDoyUnits, 4, v % 10) + bcdCost(kDoyTens, 4, (v / 10) % 10) + bcdCost(kDoyHundreds, 2, v / 100);
    });
    r.tc.dut1Tenths = pick("DUT1", -7, 7, [&](int v) {
        double plus = bitCost(kUt1Sign, 1) + bcdCost(kDut1, 3, v);
        double minus = bitCost(kUt1Sign, 0) + bcdCost(kDut1, 3, -v);
        if (v > 0) return plus;
        if (v < 0) return minus;
        return std::min(plus, minus);
    });
    r.tc.leapSecondWarning = pick("leap second warning", 0, 1, [&](int v) { return bitCost(kLsw, v); });
    r.tc.dst1 = pick("DST1", 0, 1, [&](int v) { return bitCost(kDst1, v); });
    r.tc.dst2 = pick("DST2", 0, 1, [&](int v) { return bitCost(kDst2, v); });

    if (r.structureErrors > 3)
        r.problems.push_back(std::to_string(r.structureErrors) + " seconds break the frame structure");
    // Scale the required margin with the noise seen in this frame: estimate
    // sigma from the median best-hypothesis cost (chi-square, 3 dof:
    // median = 2.366 sigma^2).
    double best[60];
    for (int p = 0; p < 60; p++) best[p] = c[p][argmin(p)];
    std::nth_element(best, best + 30, best + 60);
    double sigma = std::sqrt(best[30] / 2.366);
    double need = std::max(minFieldMargin, 2.0 * sigma);
    if (r.minMargin < need)
        r.problems.push_back(r.weakestField + " too weak to read");
    r.valid = r.problems.empty();
    return r;
}

bool isLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

void dayOfYearToDate(int year, int doy, int& month, int& day)
{
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int d = doy;
    for (int m = 0; m < 12; m++) {
        int n = mdays[m] + (m == 1 && isLeapYear(year) ? 1 : 0);
        if (d <= n) { month = m + 1; day = d; return; }
        d -= n;
    }
    month = 12; day = 31;
}

int dateToDayOfYear(int year, int month, int day)
{
    return int(daysFromCivil(year, month, day) - daysFromCivil(year, 1, 1)) + 1;
}

// Howard Hinnant's days_from_civil.
int64_t daysFromCivil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = unsigned(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + int64_t(doe) - 719468;
}

int64_t timeCodeToUnix(const TimeCode& tc)
{
    int64_t days = daysFromCivil(tc.year, 1, 1) + (tc.dayOfYear - 1);
    return days * 86400 + tc.hour * 3600 + tc.minute * 60;
}

TimeCode unixToTimeCode(int64_t t)
{
    int64_t days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
    int64_t sod = t - days * 86400;
    // civil_from_days
    int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = unsigned(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = int(yoe + era * 400);
    const unsigned doyM = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doyM + 2) / 153;
    const unsigned d = doyM - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    y += (m <= 2);
    TimeCode tc;
    tc.year = y;
    tc.dayOfYear = dateToDayOfYear(y, int(m), int(d));
    tc.hour = int(sod / 3600);
    tc.minute = int((sod / 60) % 60);
    return tc;
}

std::string formatUnix(double t, bool withMillis)
{
    double fl = std::floor(t);
    int64_t s = int64_t(fl);
    int ms = int(std::lround((t - fl) * 1000.0));
    if (ms >= 1000) { ms -= 1000; s += 1; }
    TimeCode tc = unixToTimeCode(s);
    int mon, day;
    dayOfYearToDate(tc.year, tc.dayOfYear, mon, day);
    int sec = int(((s % 60) + 60) % 60);
    char buf[64];
    if (withMillis)
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d.%03d", tc.year, mon, day, tc.hour, tc.minute, sec, ms);
    else
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d", tc.year, mon, day, tc.hour, tc.minute, sec);
    return buf;
}

} // namespace ahrwwv
