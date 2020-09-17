/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

#include "ringstream.hpp"

namespace boot {

	void main();

	inline RingStream ser0;


}
