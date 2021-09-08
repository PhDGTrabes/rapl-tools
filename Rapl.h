/* Read the RAPL registers on recent (>sandybridge) Intel processors	*/
/*									*/
/* There are currently three ways to do this:				*/
/*	1. Read the MSRs directly with /dev/cpu/??/msr			*/
/*	2. Use the perf_event_open() interface				*/
/*	3. Read the values from the sysfs powercap interface		*/
/*									*/
/* MSR Code originally based on a (never made it upstream) linux-kernel	*/
/*	RAPL driver by Zhang Rui <rui.zhang@intel.com>			*/
/*	https://lkml.org/lkml/2011/5/26/93				*/
/* Additional contributions by:						*/
/*	Romain Dolbeau -- romain @ dolbeau.org				*/
/*									*/
/* For raw MSR access the /dev/cpu/??/msr driver must be enabled and	*/
/*	permissions set to allow read access.				*/
/*	You might need to "modprobe msr" before it will work.		*/
/*									*/
/* perf_event_open() support requires at least Linux 3.14 and to have	*/
/*	/proc/sys/kernel/perf_event_paranoid < 1			*/
/*									*/
/* the sysfs powercap interface got into the kernel in 			*/
/*	2d281d8196e38dd (3.13)						*/
/*									*/
/* Compile with:   gcc -O2 -Wall -o rapl-read rapl-read.c -lm		*/
/*									*/
/* Vince Weaver -- vincent.weaver @ maine.edu -- 11 September 2015	*/
/*									*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <sys/syscall.h>
#include <linux/perf_event.h>


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <sys/syscall.h>
#include <linux/perf_event.h>


#include <cstdio>
#include <string>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include<iostream>




/* AMD Support */
#define MSR_AMD_RAPL_POWER_UNIT			0xc0010299

#define MSR_AMD_PKG_ENERGY_STATUS		0xc001029B
#define MSR_AMD_PP0_ENERGY_STATUS		0xc001029A



/* Intel support */

#define MSR_INTEL_RAPL_POWER_UNIT		0x606
/*
 * Platform specific RAPL Domains.
 * Note that PP1 RAPL Domain is supported on 062A only
 * And DRAM RAPL Domain is supported on 062D only
 */
/* Package RAPL Domain */
#define MSR_PKG_RAPL_POWER_LIMIT	0x610
#define MSR_INTEL_PKG_ENERGY_STATUS	0x611
#define MSR_PKG_PERF_STATUS		0x613
#define MSR_PKG_POWER_INFO		0x614

/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT		0x638
#define MSR_INTEL_PP0_ENERGY_STATUS	0x639
#define MSR_PP0_POLICY			0x63A
#define MSR_PP0_PERF_STATUS		0x63B

/* PP1 RAPL Domain, may reflect to uncore devices */
#define MSR_PP1_POWER_LIMIT		0x640
#define MSR_PP1_ENERGY_STATUS		0x641
#define MSR_PP1_POLICY			0x642

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT		0x618
#define MSR_DRAM_ENERGY_STATUS		0x619
#define MSR_DRAM_PERF_STATUS		0x61B
#define MSR_DRAM_POWER_INFO		0x61C

/* PSYS RAPL Domain */
#define MSR_PLATFORM_ENERGY_STATUS	0x64d

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET	0
#define POWER_UNIT_MASK		0x0F

#define ENERGY_UNIT_OFFSET	0x08
#define ENERGY_UNIT_MASK	0x1F00

#define TIME_UNIT_OFFSET	0x10
#define TIME_UNIT_MASK		0xF000

#define CPU_VENDOR_INTEL	1
#define CPU_VENDOR_AMD		2

#define CPU_SANDYBRIDGE		42
#define CPU_SANDYBRIDGE_EP	45
#define CPU_IVYBRIDGE		58
#define CPU_IVYBRIDGE_EP	62
#define CPU_HASWELL		60
#define CPU_HASWELL_ULT		69
#define CPU_HASWELL_GT3E	70
#define CPU_HASWELL_EP		63
#define CPU_BROADWELL		61
#define CPU_BROADWELL_GT3E	71
#define CPU_BROADWELL_EP	79
#define CPU_BROADWELL_DE	86
#define CPU_SKYLAKE		78
#define CPU_SKYLAKE_HS		94
#define CPU_SKYLAKE_X		85
#define CPU_KNIGHTS_LANDING	87
#define CPU_KNIGHTS_MILL	133
#define CPU_KABYLAKE_MOBILE	142
#define CPU_KABYLAKE		158
#define CPU_ATOM_SILVERMONT	55
#define CPU_ATOM_AIRMONT	76
#define CPU_ATOM_MERRIFIELD	74
#define CPU_ATOM_MOOREFIELD	90
#define CPU_ATOM_GOLDMONT	92
#define CPU_ATOM_GEMINI_LAKE	122
#define CPU_ATOM_DENVERTON	95

#define CPU_AMD_FAM17H		0xc000




#include <unistd.h>
#include <cstdint>


//static unsigned int msr_rapl_units,msr_pkg_energy_status,msr_pp0_energy_status;


#ifndef RAPL_H_
#define RAPL_H_

struct rapl_state_t {
	uint64_t pkg;
	uint64_t pp0;
	uint64_t pp1;
	uint64_t dram;
	uint64_t psys;
	struct timeval tsc;
};

#define MAX_CPUS	1024
#define MAX_PACKAGES	16

#define MSR_RAPL_POWER_UNIT		0x606

static int total_cores=0,total_packages=0,cpu_model=0;
static int package_map[MAX_PACKAGES];

class Rapl {

private:
	// Rapl configuration
	int fd;
	long long result;
	double power_units,time_units;
	double cpu_energy_units[MAX_PACKAGES],dram_energy_units[MAX_PACKAGES];
	double package_before[MAX_PACKAGES],package_after[MAX_PACKAGES];
	double pp0_before[MAX_PACKAGES],pp0_after[MAX_PACKAGES];
	double pp1_before[MAX_PACKAGES],pp1_after[MAX_PACKAGES];
	double dram_before[MAX_PACKAGES],dram_after[MAX_PACKAGES];
	double psys_before[MAX_PACKAGES],psys_after[MAX_PACKAGES];
	double thermal_spec_power,minimum_power,maximum_power,time_window;
	int j;

	//static unsigned int msr_rapl_units,msr_pkg_energy_status,msr_pp0_energy_status;

	int dram_avail=0,pp0_avail=0,pp1_avail=0,psys_avail=0;
	int different_units=0;


	// Rapl state
	rapl_state_t *current_state[MAX_PACKAGES];
	rapl_state_t *prev_state[MAX_PACKAGES];
	rapl_state_t *next_state[MAX_PACKAGES];
	rapl_state_t running_total[MAX_PACKAGES];
	rapl_state_t state1[MAX_PACKAGES], state2[MAX_PACKAGES], state3[MAX_PACKAGES];

	int detect_cpu();
	int detect_packages();
	void detect_rapl_domains(int cpu_model);
	int open_msr(int core);
	long long read_msr(int fd, unsigned int msr_offset);
	double time_delta(struct timeval *begin, struct timeval *after);
	uint64_t energy_delta(uint64_t before, uint64_t after);
	double power(uint64_t before, uint64_t after, double time_delta, int package);

public:

	unsigned int msr_rapl_units,msr_pkg_energy_status,msr_pp0_energy_status;

	void hardware_info();
	Rapl(int core);
	void reset();
	void measure();
	void measure_begin();
	void measure_end();

	double pkg_current_power(int package);
	double pp0_current_power(int package);
	double pp1_current_power(int package);
	double dram_current_power(int package);

	double pkg_average_power(int package);
	double pp0_average_power(int package);
	double pp1_average_power(int package);
	double dram_average_power(int package);

	double pkg_total_energy(int package);
	double pp0_total_energy(int package);
	double pp1_total_energy(int package);
	double dram_total_energy(int package);
	
	double total_energy_per_package(int package);
	double total_power_per_package(int package);

	double total_energy();
        double total_power();

	double total_time();
	double current_time();
};

#endif /* RAPL_H_ */
