#pragma once 
/*
 * eForce FSE Accumulator Management System
 *
 * Written by Martin Cejp and Vojtech Michal.
 *
 * Copyright (c) 2018, 2019 eForce FEE Prague Formula
 */

#include "library/units.hpp"

#include <cstdint>

void BlockingDelay(Duration time);

struct Timestamp {

	constexpr Timestamp(uint32_t tick) : tick(tick) {}

	bool TimeElapsed(Duration const duration) const {
		return Timestamp::Now() - *this > duration;
	}

	static Timestamp Now();

	constexpr Duration operator-(Timestamp other) const { 
		return Duration{tick - other.tick};
	}

	friend constexpr bool operator< (Timestamp a, Timestamp b) { return a.tick < b.tick; }
	friend constexpr bool operator> (Timestamp a, Timestamp b) { return a.tick > b.tick; }
	friend constexpr bool operator<=(Timestamp a, Timestamp b) { return a.tick <= b.tick; }
	friend constexpr bool operator>=(Timestamp a, Timestamp b) { return a.tick >= b.tick; }
	friend constexpr bool operator==(Timestamp a, Timestamp b) { return a.tick == b.tick; }
	friend constexpr bool operator!=(Timestamp a, Timestamp b) { return a.tick != b.tick; }
private:
	uint32_t tick;
};


class SysTickTimer {
public:

	SysTickTimer() : startTime{Timestamp::Now()} {}

	void Restart() {
		startTime = Timestamp::Now();
	}

	Duration GetTimeElapsed() const {
		return Timestamp::Now() - startTime;
	}

	bool TimeElapsed(Duration const interval) const {
		return startTime.TimeElapsed(interval);
	}

	bool RestartIfTimeElapsed(Duration const interval) {
		if (TimeElapsed(interval)) {
			this->Restart();
			return true;
		}
		else {
			return false;
		}
	}

private:
	Timestamp startTime;
};
