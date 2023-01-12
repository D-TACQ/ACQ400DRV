/* SITE=site acq404_reg REG [M=mask] [value]
 * all values 8 bit, hex
 */
#include <stdio.h>
#include <stdlib.h>

namespace G {
	int site = 1;
	unsigned mask = 0xff;
	int verbose;
	const char* root = "/sys/bus/spi/devices/spi1";
};



struct File {
	FILE *fp;
	File(unsigned reg, const char* mode) {
		char fname[128];
		sprintf(fname, "%s.%d/tdc_gpx2_cfg%02d", G::root, G::site-1, reg);
		fp = fopen(fname, mode);
		if (!fp){
			printf("FAILED to open %s mode %s\n", fname, mode);
			exit(1);
		}
	}
	~File() {
		fclose(fp);
	}
	unsigned getKnob() {
		char aline[80];
		return strtoul(fgets(aline, 80, fp), 0, 16);
	}
	unsigned setKnob(unsigned value) {
		fprintf(fp, "0x%02x\n", value);
		return value;
	}	
};

unsigned first_bit(unsigned mask) 
{
	int ib = 0;
	while ((1<<ib & mask) == 0){
		++ib;
	}
	return ib;
}
unsigned set_reg(unsigned reg, unsigned value)
{
	if (G::mask == 0xff){
		File wreg(reg, "w");
		return wreg.setKnob(value);
	}else{
		File rreg(reg, "r");
		unsigned v0 = rreg.getKnob();
		v0 &= ~G::mask;
		v0 |= value << first_bit(G::mask);
		File wreg(reg, "w");
		return wreg.setKnob(v0);
	}
}
unsigned get_reg(unsigned reg) {
	File rreg(reg, "r");
	return (rreg.getKnob()&G::mask) >> first_bit(G::mask);
}

void set_init(void) {
	char fname[128];
	sprintf(fname, "%s.%d/tdc_gpx2_init", G::root, G::site-1);
	FILE *fp = fopen(fname, "w");
	if (!fp){
		printf("FAILED to open %s mode %s\n", fname, "w");
		exit(1);
	}
	fprintf(fp, "1\n");
	fclose(fp);
}
/*
template <int WS>
class RegRW {
	unsigned reg;

	static unsigned first_bit(unsigned mask) {
		int ib = 0;
		while ((1<<ib & mask) == 0){
			++ib;
		}
		return ib;
	}
	const char* fmtr
public:
	RegRW(unsigned _reg, unsigned mask): reg(_reg) {

	}
	unsigned get_reg(void) {

	}
	unsigned set_reg(unsigned value) {

	}
	unsigned get_reg_print(void) {

	}
	unsigned set_reg_print(unsigned value) {

	}
};
*/
int main(int argc, char* argv[])
{
	unsigned reg, value;
	bool value_set = false;

	if (getenv("SITE")){
		G::site = atoi(getenv("SITE"));
	}
	if (argc <= 1){
		fprintf(stderr, "USAGE: reg_rw reg [M=mask] [value]");
		return 1;
	}
	if (argc > 1){
		reg = strtoul(argv[1], 0, 16);
	}
	if (argc > 2){
		if (sscanf(argv[2], "M=%x", &G::mask) == 1){
			G::verbose && printf("mask set 0x%02x\n", G::mask);
		}else{
			value = strtoul(argv[2], 0, 16);
			value_set = true;
		}
	}
	if (argc > 3){
		value = strtoul(argv[3], 0, 16);
		value_set = true;
	}

	if (value_set){
		G::verbose &&
		printf("set: SITE:%d reg:%d mask:0x%02x value:0x%02x\n", 
			G::site, reg, G::mask, value);
		set_reg(reg, value);
		set_init();
	}else{
		G::verbose &&
		printf("get: SITE:%d reg:%d mask:0x%02x\n",
			G::site, reg, G::mask);
		printf("0x%02x\n", get_reg(reg));
	}
	return 0;
}
