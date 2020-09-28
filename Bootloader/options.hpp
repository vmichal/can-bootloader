/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once
#include <cstdio>

#ifdef ENABLE_STDOUT
#define debug_printf(args) printf args
#else
#define debug_printf(args)
#endif

constexpr bool enableAssert = true;


