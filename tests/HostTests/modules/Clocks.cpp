/*
 * Tests optimised time conversion code use for hardware timer.
 * We do this by evaluating various time values and comparing against floating-point
 * calculation.
 */

#include <HostTests.h>
#include <Platform/Timers.h>
#include <HardwareTimer.h>

template <class Clock, typename TimeType> class ClockTestTemplate : public TestGroup
{
public:
	using Micros = NanoTime::TimeSource<Clock, NanoTime::Microseconds, TimeType>;

	ClockTestTemplate()
		: TestGroup(F("TimeSource: ") + Micros::toString()), refCycles("Reference cycles"),
		  calcCycles("Calculation cycles")
	{
#if DEBUG_VERBOSE_LEVEL == DBG
		verbose = true;
#endif
	}

	void execute() override
	{
		printLimits();

		for(unsigned i = 0; i < 2000; ++i) {
			auto value = os_random();
			check<NanoTime::Milliseconds>(value);
			check<NanoTime::Microseconds>(value);
			check<NanoTime::Nanoseconds>(value);
		}

		printStats();

		TEST_CASE("vs. system time")
		{
			constexpr uint32_t duration{2000000};
			auto startTime = system_get_time();
			auto startTicks = Clock::ticks();
			uint32_t time;
			while((time = system_get_time()) < startTime + duration) {
				//
			}
			auto endTicks = Clock::ticks();
			// Handle both up and down counters
			auto elapsedTicks = (endTicks >= startTicks) ? endTicks - startTicks : startTicks - endTicks;

			debug_w("System time elapsed: %u", time - startTime);
			debug_w("%s ticks: %u", Clock::typeName(), elapsedTicks);
			debug_w("Ratio: x %f", float(elapsedTicks) / (time - startTime));
			uint32_t us = Micros::ticksToTime(elapsedTicks);
			debug_w("Apparent time: %u", us);
			// Up-timers may report 0 if inactive
			if(endTicks != 0 || startTicks != 0) {
				REQUIRE(abs(int(us - duration)) < 500); // Allow some latitude
			}
		}
	}

	template <NanoTime::Unit unit> void check(TimeType value)
	{
		using TimeSource = NanoTime::TimeSource<Clock, unit, TimeType>;

		//		Serial.print("HwTimer::maxTime = ");
		//		Serial.println(HwTimer::maxTime());

		this->timeunit = unit;

		//
		valueIsTime = true;
		this->value = value % TimeSource::maxCalcTime();

		noInterrupts();
		refCycles.start();
		ref = timeToTicksRef();
		refCycles.update();

		calcCycles.start();
		calc = TimeSource::timeToTicks(this->value);
		calcCycles.update();
		interrupts();

		if(calc != ref) {
			calc = TimeSource::timeToTicks(this->value);
		}

		compare();

		//
		valueIsTime = false;
		this->value = value % TimeSource::maxCalcTicks();

		noInterrupts();
		refCycles.start();
		ref = ticksToTimeRef();
		refCycles.update();

		calcCycles.start();
		calc = TimeSource::ticksToTime(this->value);
		calcCycles.update();
		interrupts();
		compare();
	}

	void printStats()
	{
		Serial.println(refCycles);
		Serial.println(calcCycles);
	}

	template <NanoTime::Unit unit> void printMaxTicks()
	{
		NanoTime::TimeSource<Clock, unit, TimeType> src;
		auto time = src.maxClockTime();

		Serial.print("    ");
		Serial.print(src.maxTicks());
		Serial.print(" ticks = ");
		Serial.print(time.toString());
		Serial.print(" = ");
		Serial.println(time.value());
	};

	template <NanoTime::Unit unit> void printMaxCalcTicks()
	{
		NanoTime::TimeSource<Clock, unit, TimeType> src;
		auto time = src.maxCalcTicks().template as<unit>();

		Serial.print("    ");
		Serial.print(src.maxCalcTicks());
		Serial.print(" ticks = ");
		Serial.print(time.toString());
		Serial.print(" = ");
		Serial.println(time.value());
	};

	template <NanoTime::Unit unit> void printMaxTime()
	{
		NanoTime::TimeSource<Clock, unit, TimeType> source;
		auto time = source.maxCalcTime();
		auto ticks = source.timeToTicks(time);

		Serial.print("    ");
		Serial.print(time.toString());
		Serial.print(" = ");
		Serial.print(time.value());
		Serial.print(" = ");
		Serial.print(ticks);
		Serial.println(" ticks");
	};

	void printLimits()
	{
		m_puts("Limits:\r\n");

		m_puts("  clock ticks:\r\n");
		printMaxTicks<NanoTime::Milliseconds>();
		printMaxTicks<NanoTime::Microseconds>();
		printMaxTicks<NanoTime::Nanoseconds>();

		m_puts("  ticks -> time:\r\n");
		printMaxCalcTicks<NanoTime::Milliseconds>();
		printMaxCalcTicks<NanoTime::Microseconds>();
		printMaxCalcTicks<NanoTime::Nanoseconds>();

		m_puts("  time -> ticks:\r\n");
		printMaxTime<NanoTime::Milliseconds>();
		printMaxTime<NanoTime::Microseconds>();
		printMaxTime<NanoTime::Nanoseconds>();
	}

private:
	const char* get_tag(bool is_time)
	{
		return is_time ? "time" : "ticks";
	}

	void print()
	{
		String value_tag = get_tag(valueIsTime);
		String result_tag = get_tag(!valueIsTime);
		auto diff = calc - ref;

		Serial.print("  ");
		Serial.print(value_tag);
		Serial.print(": ");
		Serial.print(value);
		Serial.print(" (");
		Serial.print(unitToString(timeunit));
		Serial.print("), ref ");
		Serial.print(result_tag);
		Serial.print(": ");
		Serial.print(ref);
		Serial.print(", calc ");
		Serial.print(result_tag);
		Serial.print(": ");
		Serial.print(calc);
		Serial.print(", diff: ");
		Serial.println(diff);
	}

	void compare()
	{
		// Tolerate a +/- 1 in result to account for FP rounding
		int64_t diff = calc - ref;
		bool ok = abs(diff) == 0; //1;
		if(verbose || calc != ref) {
			print();
		}
		TEST_ASSERT(ok);
	};

#define USE_FP_CALC

	uint64_t timeToTicksRef()
	{
		auto unitTicks = NanoTime::unitTicks[timeunit];
#ifdef USE_FP_CALC
		return round(double(value) * Clock::frequency() * unitTicks.den / unitTicks.num);
#else
		return value / Ratio64 r(unitTicks.num, Clock::frequency() * unitTicks.den);
#endif
	}

	uint64_t ticksToTimeRef()
	{
		auto unitTicks = NanoTime::unitTicks[timeunit];
#ifdef USE_FP_CALC
		return round(double(value) * unitTicks.num / (Clock::frequency() * unitTicks.den));
#else
		return value * Ratio64 r(unitTicks.num, Clock::frequency() * unitTicks.den);
#endif
	}

private:
	NanoTime::Unit timeunit = NanoTime::Seconds;
	TimeType value = 0; // ticks or time as input to calculation
	bool valueIsTime = false;
	uint64_t ref = 0;  // Reference result
	TimeType calc = 0; // Calculated result
	bool verbose = false;
	CpuCycleTimes refCycles;
	CpuCycleTimes calcCycles;
};

template <hw_timer_clkdiv_t clkdiv>
class Timer1ClockTestTemplate : public ClockTestTemplate<Timer1Clock<clkdiv>, uint32_t>
{
public:
	void execute() override
	{
		// Configure the hardware to match selected clock divider
		Timer1Api<clkdiv, eHWT_Maskable> timer;
		timer.arm(false);

		ClockTestTemplate<Timer1Clock<clkdiv>, uint32_t>::execute();
	}
};

template <class Clock, typename TimeType> class CpuClockTestTemplate : public ClockTestTemplate<Clock, TimeType>
{
public:
	using ClockTestTemplate<Clock, TimeType>::ClockTestTemplate;

	void execute() override
	{
		uint32_t curFreq = system_get_cpu_freq();
		if(!System.setCpuFrequency(Clock::cpuFrequency())) {
			debug_e("Failed to set CPU frequency, skipping test");
			return;
		}

		debug_i("CPU freq: %u -> %u MHz", curFreq, system_get_cpu_freq());
		ClockTestTemplate<Clock, TimeType>::execute();
		System.setCpuFrequency(CpuCycleClockNormal::cpuFrequency());
	}
};

/*
 * Why use a Polled timer? Comparison versus hand-coded loops.
 */
class BenchmarkPolledTimer : public TestGroup
{
public:
	BenchmarkPolledTimer() : TestGroup(_F("Benchmark Polled Timer"))
	{
	}

	static constexpr unsigned TIMEOUT_MS = 100;

	unsigned __noinline millis_loop()
	{
		unsigned loopCount = 0;
		unsigned start = millis();
		while(millis() - start < TIMEOUT_MS) {
			++loopCount;
		}
		return loopCount;
	}

	unsigned __noinline micros_loop()
	{
		unsigned loopCount = 0;
		unsigned start = micros();
		while(micros() - start < (TIMEOUT_MS * 1000)) {
			++loopCount;
		}
		return loopCount;
	}

	unsigned __noinline polledTimer_loop()
	{
		unsigned loopCount = 0;
		OneShotFastMs timer(TIMEOUT_MS);
		while(!timer.expired()) {
			++loopCount;
		}
		return loopCount;
	}

	void execute() override
	{
		Serial.print("How many loop iterations can we achieve in ");
		Serial.print(TIMEOUT_MS);
		Serial.println(" ms ?");

		auto print = [](const char* type, uint32_t loopCount) {
			Serial.print("Using ");
			Serial.print(type);
			Serial.print(", managed ");
			Serial.print(loopCount);
			Serial.print(" iterations, average loop time = ");
			using namespace NanoTime;
			constexpr auto nsTotal = convert<TIMEOUT_MS, Milliseconds, Nanoseconds>();
			auto nsPerLoop = time(Nanoseconds, muldiv(nsTotal, uint32_t(1), loopCount));
			Serial.print(nsPerLoop.toString());
			Serial.print(" (");
			auto cycles = CpuCycleClockNormal::template timeToTicks<Nanoseconds>(uint32_t(nsPerLoop));
			Serial.print(cycles.toString());
			Serial.println(" CPU cycles)");
		};

		print("millis()", millis_loop());
		print("micros()", micros_loop());
		print("PolledTimer", polledTimer_loop());
	}
};

constexpr unsigned BenchmarkPolledTimer::TIMEOUT_MS;

template <hw_timer_clkdiv_t clkdiv, NanoTime::Unit unit, typename TimeType>
struct Timer1TestSource : public Timer1Clock<clkdiv> {
	static TimeType timeToTicks_test1(const TimeType& time)
	{
		/*
		 * Refactorise to eliminate overflow when scaling down and avoid division by
		 * using pre-defined constant values.
		 *
		 * Original code:
		 *
		 * 		if(us > 0x35A)
		 * 			return (us / 4) * (frequency / 250000) + (us % 4) * (frequency / 1000000);
		 *
		 * This only works for /16 prescale. It's also un-necessary since the ratio reduces to 5:1,
		 * i.e. it's just a x5 multiplication.
		 *
		 * However, with /256 prescale it's a little tricker. This code is used in the
		 * `ets_timer_arm_new` function for converting milliseconds into ticks:
		 *
		 * 		if(ms > 13743)
		 * 			return (ms / 4) * (frequency / 250) + (ms % 4) * (frequency / 1000)
		 *
		 * In this case the ratio is 625:2 which limits the range to 1'54"31.947, but this
		 * calculation offers an improvement to 3'49"25.92. It is probably slightly faster
		 * as well.
		 *
		 * Converting from microseconds the ratio is 16:5.
		 *
		 * The advantage of muldiv is that it is generic and will work with any ratio.
		 * In most cases it's just as fast, offers overflow detection and a greater range by
		 * using 64-bit calculations when necessary.
		 *
		 * Ideally all time conversions should be pre-calculated so that time-critical code
		 * (e.g. within ISRs) operates using the timer tick values.
		 *
		 */
		constexpr uint32_t prediv = 4;
		constexpr auto unitTicks = NanoTime::unitTicks[unit];
		constexpr auto frequency = Timer1TestSource::frequency();
		constexpr uint32_t mul = frequency * unitTicks.den / (unitTicks.num / prediv);
		constexpr uint32_t div = frequency * unitTicks.den / unitTicks.num;

		if(clkdiv == TIMER_CLKDIV_16) {
			//		debug_i("prediv = %u, frequency = %u, mul = %u, div = %u", prediv, frequency, mul, div);
			return (time / prediv) * mul + (time % prediv) * div;
		} else {
			using R = std::ratio<frequency * unitTicks.den, unitTicks.num>;
			return muldiv<R::num, R::den>(time);
		}
	}

	TimeType timeToTicks_test2(const TimeType& time)
	{
		/*
		 * Evaluate performance using full muldiv64 all the time.
		 * In practice, this is un-necessary and the implemented solution is to
		 * only use it when an overflow would occur.
		 */
		return uint64_t(time) * Timer1TestSource::ticksPerUnit(unit); // 116
		//		return muldiv(time, uint32_t(TPU::num), uint32_t(TPU::den)); // 47
	}
};

template <hw_timer_clkdiv_t clkdiv, typename TimeType> void testTimer1()
{
	using Clock = Timer1TestSource<clkdiv, NanoTime::Microseconds, uint32_t>;
	using TimeSource = NanoTime::TimeSource<Clock, NanoTime::Microseconds, TimeType>;
	Clock timer1;

	CpuCycleTimes m1("ticks"), m2("ticks1"), m3("ticks2");

	for(unsigned i = 0; i < 5000; ++i) {
		TimeType time = os_random(); //0x7fffffff;
		if(sizeof(time) == sizeof(uint64_t)) {
			//			time = (time << 32) | os_random();
		}

		time %= TimeSource::maxCalcTime();

		m1.start();
		volatile TimeType ticks = TimeSource::timeToTicks(time);
		m1.update();

		m2.start();
		volatile TimeType ticks1 = timer1.timeToTicks_test1(time);
		m2.update();

		m3.start();
		volatile TimeType ticks2 = timer1.timeToTicks_test2(time);
		m3.update();

		TimeType refticks = round(double(time) * TimeSource::TicksPerUnit::num / TimeSource::TicksPerUnit::den);

		//		uint64_t refticks = timer1.timeToTicksRef(time);

		auto check = [time, refticks](const char* tag, TimeType ticks) {
			int64_t diff = int64_t(ticks) - int64_t(refticks);
			if(abs(diff) > 2) {
				Serial.print("time = ");
				Serial.print(time);
				Serial.print(", refticks = ");
				Serial.print(refticks);
				Serial.print(", ");
				Serial.print(tag);
				Serial.print(" = ");
				Serial.print(ticks);
				Serial.print(", diff = ");
				Serial.println(diff);
			}
		};

		check("ticks", ticks);
		check("ticks1", ticks1);
		check("ticks2", ticks2);

		//		if(refticks != ticks || refticks != ticks1 || refticks != ticks2) {
		//			m_printf("time = %u (0x%08x), ticks = %u, ticks1 = %u, ticks2 = %u\r\n", time, time, ticks, ticks1, ticks2);
		//		}
	}

	Serial.println(m1);
	Serial.println(m2);
	Serial.println(m3);
}

class TimerCalcTest : public TestGroup
{
public:
	TimerCalcTest() : TestGroup(_F("Clock calculations"))
	{
	}

	void execute() override
	{
		testTimer1<TIMER_CLKDIV_16, uint32_t>();
		testTimer1<TIMER_CLKDIV_256, uint32_t>();
	}
};

void REGISTER_TEST(Clocks)
{
	registerGroup<BenchmarkPolledTimer>();

	registerGroup<TimerCalcTest>();

	registerGroup<Timer1ClockTestTemplate<TIMER_CLKDIV_16>>();
	registerGroup<Timer1ClockTestTemplate<TIMER_CLKDIV_256>>();

	registerGroup<ClockTestTemplate<Timer2Clock, uint32_t>>();

	if(CpuCycleClockSlow::cpuFrequency() != CpuCycleClockNormal::cpuFrequency()) {
		registerGroup<CpuClockTestTemplate<CpuCycleClockSlow, uint32_t>>();
	}
	registerGroup<CpuClockTestTemplate<CpuCycleClockNormal, uint32_t>>();
	registerGroup<CpuClockTestTemplate<CpuCycleClockFast, uint64_t>>();
}
