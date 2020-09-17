/*
 * eForce FSE Accumulator Management System
 *
 * Written by Vojtech Michal
 *
 * Copyright (c) 2020 eForce FEE Prague Formula
 *
 * This header contains signatures of various unrelated functions that 
 * are generic enough not to reside in implementation files.
 */

#include <library/units.hpp>

#include <algorithm>
/* Applies low pass filter characterized by given sampling frequency and time constant
    on the given system. Filtered value is returned. */
template<typename T>
constexpr T lowPassFilter(T state, T input, Frequency sampleRate, Duration tau) {
    float const b1 = 1.0f / (sampleRate.toHertz() * tau.toSeconds());
    float const a1 = 1.0f - b1;

    return state * a1 + input * b1;
}

#define FIELD_IDENTIFIER(message, field, iden) AMS_ ## message ## _ ## field ## _ ## iden

#define CanConversionConstants(message, field) FIELD_IDENTIFIER(message, field, OFFSET), \
FIELD_IDENTIFIER(message, field, FACTOR), FIELD_IDENTIFIER(message, field, MIN), FIELD_IDENTIFIER(message, field, MAX)

template<typename T, typename CoefT, typename ConstT>
constexpr auto ConvertValueToCAN(T value, CoefT coefficient, ConstT offset, ConstT factor, ConstT min, ConstT max) {

    auto const clamped = std::clamp<float>(value, min * coefficient, max * coefficient);
    auto const normalized = clamped - offset * coefficient;
    return normalized / factor / coefficient;
}

