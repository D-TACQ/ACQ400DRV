/*
 * wrtd_TS.h
 *
 *  Created on: 5 Nov 2021
 *      Author: pgm
 */

#ifndef WRTD_TS_H_
#define WRTD_TS_H_

#define SECONDS_SHL	28
#define SECONDS_MASK	0x7
#define MIN_TAI		59				// > this value -> absolute time
#define TICKS_MASK	0x0fffffff
#define TS_EN		(1<<31)

#define TS_QUICK	0xffffffffU			// trigger right away, no timing ..
#define TS_QUICK_TICKS	0x0fffffffU			// trigger right away, no timing ..

#define M1 1000000
#define NSPS	(1000*M1)		// nanoseconds per second

#define MAX_TX_INF	0xFFFFFFFF

typedef std::vector<std::string> VS;


namespace G {
	unsigned ticks_per_sec = 80000000;
	int delta_ticks;
	double ns_per_tick = 50.0;			// ticks per nsec
}

struct TS {

private:
	void make_raw(unsigned _secs, unsigned _ticks, bool en=true) {
		tai_s = _secs>MIN_TAI? _secs: 0;
		mask = 0;
		raw = (_secs&SECONDS_MASK) << SECONDS_SHL | (_ticks&TICKS_MASK) | (en?TS_EN:0);
	}
	unsigned inc(unsigned s){
		return ++s&SECONDS_MASK;
	}
	unsigned dec(unsigned s){
		return --s&SECONDS_MASK;
	}
public:
	unsigned tai_s;			/* if non-zero, we have an absolute TS */
	unsigned raw;
	char repr[32];
	unsigned char mask;		/* possible, multiple receivers */

	TS(const TS& ts) : tai_s(ts.tai_s), raw(ts.raw), mask(ts.mask)
	{}

	TS(unsigned _raw = 0): tai_s(0), raw(_raw), mask(0)
	{}
	TS(unsigned _secs, unsigned _ticks, bool en=true) {
		make_raw(_secs, _ticks, en);
	}
	TS(const char* def){
		 VS tsx;
		 split2<VS>(def, tsx, ':');
		 assert(tsx.size() == 2);
		 make_raw(strtoul(tsx[0].c_str(), 0, 10), strtoul(tsx[1].c_str(), 0, 10));
	}

	unsigned secs() const { return tai_s? tai_s: (raw& ~TS_EN) >> SECONDS_SHL; }
	unsigned ticks() const { return raw&TICKS_MASK; }
	unsigned nsec() const { return ticks() * G::ns_per_tick; }

	TS add (unsigned dsecs, unsigned dticks = 0) {
		unsigned ss = secs();
		unsigned tt = ticks();
		tt += dticks;
		if (tt > G::ticks_per_sec){
			tt -= G::ticks_per_sec;
			ss += 1;
		}
		ss += dsecs;

		return TS(ss, tt);
	}
	TS operator+ (unsigned dticks) {
		return add(0, dticks);
	}
	bool operator== (TS& ts2) const {
		return ticks() == ts2.ticks() && secs() == ts2.secs();
	}
	bool operator!= (TS& ts2) const {
		return ticks() != ts2.ticks() || secs() != ts2.secs();
	}
	void strip() {			/* convert to relative (HW) timestamp */
		tai_s = 0;
	}
	bool is_abs_tai() {
		return tai_s != 0;
	}
	long diff(TS& ts2){
		unsigned _ticks = ticks();
		unsigned _secs = secs();
		if (ts2.ticks() > ticks()){
			_ticks += G::ticks_per_sec;
			_ticks -= ts2.ticks();
			_secs = dec(_secs);
		}else{
			_ticks -= ts2.ticks();
		}
		return (_secs - ts2.secs())*NSPS + _ticks*G::ns_per_tick;
	}

	const char* toStr(void) {
		snprintf(repr, 32, "%d:%07d m:%02x", secs(), ticks(), mask);
		return repr;
	}
	static int do_ts_diff(const char* _ts1, const char* _ts2){
		TS ts1(_ts1);
		TS ts2(_ts2);
		fprintf(stderr, "ts1:%s ts2:%s diff:%ld\n", ts1.toStr(), ts2.toStr(), ts1.diff(ts2));
		return 0;
	}
	TS next_second() {
		int add_sec = 1;
		if (ticks() + G::delta_ticks >= TICKS_MASK){
			add_sec += 1;
		}
		return TS(secs() + add_sec, 0);
	}

	static TS ts_quick;
};

TS TS::ts_quick = TS_QUICK;

#endif /* WRTD_TS_H_ */
