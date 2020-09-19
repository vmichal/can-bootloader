/*
 * FSE.07 Accumulator Management System
 *
 * Written by Martin Cejp, Vojtech Michal
 *
 * Copyright (c) 2018 eForce FEE Prague Formula
 */

#ifndef ecua_types_hpp
#define ecua_types_hpp

#include <cstdint>
#include <limits>

#include <type_traits>
#include <cstdint>

/* Base type for all quantities. Defines arithmetic and relational operators. */
template<typename Storage, typename Derived>
struct Quantity {

    using underlying = Storage;
    underlying raw_value;

    constexpr bool operator< (Derived rhs) const { return raw_value <  rhs.raw_value; }
    constexpr bool operator> (Derived rhs) const { return raw_value >  rhs.raw_value; }
    constexpr bool operator==(Derived rhs) const { return raw_value == rhs.raw_value; }
    constexpr bool operator!=(Derived rhs) const { return raw_value != rhs.raw_value; }
    constexpr bool operator>=(Derived rhs) const { return raw_value >= rhs.raw_value; }
    constexpr bool operator<=(Derived rhs) const { return raw_value <= rhs.raw_value; }

    constexpr Derived operator- () const { 
        static_assert(std::is_signed_v<underlying>, "Underlying type must be signed to apply unary minus operator.");
        return { static_cast<underlying>(-raw_value) };
    }

    constexpr Derived  operator+ (Derived rhs) const { return { static_cast<underlying>(raw_value + rhs.raw_value) }; }
    constexpr Derived  operator- (Derived rhs) const { return { static_cast<underlying>(raw_value - rhs.raw_value) }; }
    constexpr Derived& operator+=(Derived rhs)       { raw_value += rhs.raw_value; return static_cast<Derived&>(*this); }
    constexpr Derived& operator-=(Derived rhs)       { raw_value -= rhs.raw_value; return static_cast<Derived&>(*this); }

    template<typename Scalar> constexpr friend Derived operator* (Scalar s, Quantity q) { return { static_cast<underlying>(q.raw_value * s) }; }
    template<typename Scalar> constexpr friend Derived operator* (Quantity q, Scalar s) { return { static_cast<underlying>(q.raw_value * s) }; }

    template<typename Scalar> constexpr std::enable_if_t<std::is_arithmetic_v<Scalar>, Derived>
                                                 operator/ (Scalar s) const { return { static_cast<underlying>(raw_value / s) }; }
                              constexpr float    operator/ (Quantity q) const { return raw_value / static_cast<float>(q.raw_value); }

    template<typename Scalar> constexpr Derived& operator*=(Scalar s)       { raw_value *= s; return static_cast<Derived&>(*this); }
    template<typename Scalar> constexpr Derived& operator/=(Scalar s)       { raw_value /= s; return static_cast<Derived&>(*this); }

    constexpr static Derived copy(Derived const volatile& rhs) { return Derived{ rhs.raw_value };}
};

struct InformationSize : public Quantity<std::uintptr_t, InformationSize> {
    //Internally stores storage size in bytes

	constexpr static InformationSize fromBytes(underlying bytes) { return { bytes }; }
	constexpr static InformationSize fromKibiBytes(underlying KiB) { return { KiB << 10 }; }
	constexpr static InformationSize fromMebiBytes(underlying MiB) { return { MiB << 20 }; }

    constexpr underlying toBytes()     const { return raw_value; }
    constexpr underlying toKibiBytes() const { return raw_value >> 10; }
    constexpr float      toMebiBytes() const { return raw_value / float(1 << 20); }
};

struct Voltage : public Quantity<int, Voltage> {
    //Internally stores voltage in microvolts

    template<typename T>
    constexpr static Voltage fromVolts     (T volts)      { return { static_cast<underlying>(volts * 1'000'000) }; }
    constexpr static Voltage fromMillivolts(underlying millivolts) { return { millivolts * 1000 }; }
    constexpr static Voltage fromBMSFormat (underlying ADC_value) { 
        /* Please, refer to section 7.3.3.4 VSENSE Input Channels in https://www.ti.com/lit/ds/slusc51c/slusc51c.pdf

        According to datasheet: raw values convert to volts using the formula V_cell = 2 * V_ref / 65535 * READ_ADC_VALUE,
        where V_ref is apparently 2,5V. BMS therefore feed data in format 1000 mV ~ 13 107 raw. */

        int const raw = static_cast<std::int64_t>(ADC_value) * 5'000'000 / 65535;
        return { raw };
    }


    constexpr underlying toMicrovolts() const { return raw_value; }
	constexpr underlying toMillivolts() const { return raw_value / 1000; }
	constexpr underlying toTenthsMillivolt() const { return raw_value / 100; }
	constexpr float toVolts() const { return raw_value / 1'000'000.0f; }

};

struct Current : public Quantity<int, Current> {
    //Internally stores current in microamperes

    template<typename T>
    constexpr static Current fromAmps     (T amps)      { return { static_cast<underlying>(amps * 1'000'000) };}
    constexpr static Current fromMilliamps(int milliamps) { return { milliamps * 1'000 }; }
    constexpr static Current fromMicroamps(int micro) { return { micro }; }

    constexpr int   toMicroamps() const { return raw_value; }
    constexpr int   toMilliamps() const { return raw_value / 1000; }
    constexpr float toAmps() const      { return raw_value / 1'000'000.0f; }
};


/* Stores duration information within the codebase. Allows arithmetic operations like plain ints, but contains
    information about the unit and thus shall be prefered. */
template<typename T>
struct GenericDuration : Quantity<T, GenericDuration<T>> {
    //Internally stores duration in microseconds

    constexpr static GenericDuration fromMicroseconds(unsigned micros) { return {micros}; }
    constexpr static GenericDuration fromMilliseconds(unsigned millis) { return { millis * 1'000 }; }
    constexpr static GenericDuration fromSeconds     (unsigned seconds) { return { seconds * 1'000'000 }; }
    constexpr static GenericDuration fromMinutes     (unsigned minutes) { return { minutes * (1'000 * 60 * 1'000) }; }

    constexpr T     toMicroseconds() const { return this->raw_value ;}
    constexpr T     toMilliseconds() const { return this->raw_value / 1'000;}
    constexpr float      toSeconds() const { return this->raw_value / 1'000'000.0f;}
    constexpr float      toMinutes() const { return this->raw_value / (1000.0f * 60 * 1'000);}

    template<typename Rhs> constexpr bool operator==(GenericDuration<Rhs> rhs) const { return this->raw_value == rhs.raw_value; }
    template<typename Rhs> constexpr bool operator!=(GenericDuration<Rhs> rhs) const { return this->raw_value != rhs.raw_value; }
    template<typename Rhs> constexpr bool operator<=(GenericDuration<Rhs> rhs) const { return this->raw_value <= rhs.raw_value; }
    template<typename Rhs> constexpr bool operator>=(GenericDuration<Rhs> rhs) const { return this->raw_value >= rhs.raw_value; }
    template<typename Rhs> constexpr bool operator <(GenericDuration<Rhs> rhs) const { return this->raw_value  < rhs.raw_value; }
    template<typename Rhs> constexpr bool operator >(GenericDuration<Rhs> rhs) const { return this->raw_value  > rhs.raw_value; }

    template<typename Dest> //Conversion operator for converting between GenericDurations
    operator GenericDuration<Dest>() const { return { static_cast<Dest>(this->raw_value) }; } //TODO make explicit if sizeof(Dest) < sizeof(T)
};
static_assert(sizeof(unsigned) == sizeof(std::uint32_t), "For backwards compatibility");

using Duration = GenericDuration<unsigned>;
using LongDuration = GenericDuration<std::uint64_t>;

/* Wraps raw frequency in hertz and offers conversion to period. */
struct Frequency : Quantity<unsigned, Frequency> {
    //internally stores frequency in hertz

    template<typename T>
    constexpr static Frequency fromHertz(T hz) { return { static_cast<underlying>(hz) }; }

    constexpr underlying toHertz() const { return raw_value; }

    constexpr Duration period() const { return Duration::fromMilliseconds(1'000 / raw_value); }
};

struct Power : Quantity<unsigned, Power> {
    //internally stores power in watts

    static constexpr Power fromWatts(unsigned power) { return { power }; }
    static constexpr Power fromKilowatts(unsigned power) { return { power * 1'000 }; }

    constexpr unsigned toWatts() const { return raw_value; }
    constexpr unsigned toKilowatts() const { return raw_value / 1'000; }

};

struct Resistance : Quantity<unsigned, Resistance> {
    //internally stores power in ohms

    static constexpr Resistance fromOhms(unsigned power) { return { power }; }
    static constexpr Resistance fromKiloohms(unsigned power) { return { power * 1'000 }; }
    static constexpr Resistance fromMegaohms(unsigned power) { return { power * 1'000'000 }; }

    constexpr unsigned toOhms() const { return raw_value; }
    constexpr unsigned toKiloohms() const { return raw_value / 1'000; }
    constexpr unsigned toMegaohms() const { return raw_value / 1'000'000; }

};

constexpr Resistance operator""_Ohm(unsigned long long resistance) {return Resistance::fromOhms(resistance); }
constexpr Resistance operator""_kOhm(unsigned long long resistance) {return Resistance::fromKiloohms(resistance); }
constexpr Resistance operator""_MOhm(unsigned long long resistance) {return Resistance::fromMegaohms(resistance); }

constexpr Power operator""_W(unsigned long long power)          { return Power::fromWatts(power); }
constexpr Power operator""_kW(unsigned long long power)         { return Power::fromKilowatts(power); }

constexpr Frequency operator""_Hz(unsigned long long freq)      { return Frequency::fromHertz(freq); }

constexpr Duration operator""_us (unsigned long long dur)     { return Duration::fromMicroseconds(dur); }
constexpr Duration operator""_ms (unsigned long long dur)     { return Duration::fromMilliseconds(dur); }
constexpr Duration operator""_s  (unsigned long long dur)     { return Duration::fromSeconds(dur); }
constexpr Duration operator""_min(unsigned long long dur)     { return Duration::fromMinutes(dur); }

constexpr Voltage operator""_V     (unsigned long long voltage) { return Voltage::fromVolts(voltage); }
constexpr Voltage operator""_mV    (unsigned long long voltage) { return Voltage::fromMillivolts(voltage); }

constexpr Current operator""_A      (unsigned long long current) { return Current::fromAmps(current); }
constexpr Current operator""_mA     (unsigned long long current) { return Current::fromMilliamps(current); }

constexpr InformationSize operator""_B   (unsigned long long size) {return InformationSize::fromBytes    (size); }
constexpr InformationSize operator""_KiB (unsigned long long size) {return InformationSize::fromKibiBytes(size); }
constexpr InformationSize operator""_MiB (unsigned long long size) {return InformationSize::fromMebiBytes(size); }

static_assert(393_V - 394_V < 0_V, "Sanity check of unit arithmetic.");
#endif
