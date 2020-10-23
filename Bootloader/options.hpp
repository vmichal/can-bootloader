/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

#include <ufsel/bit_operations.hpp>
#include <library/units.hpp>


constexpr bool enableAssert = true;

constexpr auto otherBLdetectionTime = 1_s;

constexpr int isrVectorAlignmentBits = 9; //TODO make customization point
constexpr std::uint32_t isrVectorAlignmentMask = ufsel::bit::bitmask_of_width(isrVectorAlignmentBits);

