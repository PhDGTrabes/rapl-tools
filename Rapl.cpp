#include "Rapl.h"

using namespace std;

/* TODO: on Skylake, also may support  PSys "platform" domain,	*/
/* the whole SoC not just the package.				*/
/* see dcee75b3b7f025cc6765e6c92ba0a4e59a4d25f4			*/

int Rapl::detect_cpu(void) {

	FILE *fff;

	int vendor=-1,family,model=-1;
	char buffer[BUFSIZ],*result;
	char vendor_string[BUFSIZ];

	fff=fopen("/proc/cpuinfo","r");
	if (fff==NULL) return -1;

	while(1) {
		result=fgets(buffer,BUFSIZ,fff);
		if (result==NULL) break;

		if (!strncmp(result,"vendor_id",8)) {
			sscanf(result,"%*s%*s%s",vendor_string);

			if (!strncmp(vendor_string,"GenuineIntel",12)) {
				vendor=CPU_VENDOR_INTEL;
			}
			if (!strncmp(vendor_string,"AuthenticAMD",12)) {
				vendor=CPU_VENDOR_AMD;
			}
		}

		if (!strncmp(result,"cpu family",10)) {
			sscanf(result,"%*s%*s%*s%d",&family);
		}

		if (!strncmp(result,"model",5)) {
			sscanf(result,"%*s%*s%d",&model);
		}

	}

	if (vendor==CPU_VENDOR_INTEL) {
		if (family!=6) {
			printf("Wrong CPU family %d\n",family);
			return -1;
		}

		msr_rapl_units=MSR_INTEL_RAPL_POWER_UNIT;
		msr_pkg_energy_status=MSR_INTEL_PKG_ENERGY_STATUS;
		msr_pp0_energy_status=MSR_INTEL_PP0_ENERGY_STATUS;
/*
		printf("Found ");

		switch(model) {
			case CPU_SANDYBRIDGE:
				printf("Sandybridge");
				break;
			case CPU_SANDYBRIDGE_EP:
				printf("Sandybridge-EP");
				break;
			case CPU_IVYBRIDGE:
				printf("Ivybridge");
				break;
			case CPU_IVYBRIDGE_EP:
				printf("Ivybridge-EP");
				break;
			case CPU_HASWELL:
			case CPU_HASWELL_ULT:
			case CPU_HASWELL_GT3E:
				printf("Haswell");
				break;
			case CPU_HASWELL_EP:
				printf("Haswell-EP");
				break;
			case CPU_BROADWELL:
			case CPU_BROADWELL_GT3E:
				printf("Broadwell");
				break;
			case CPU_BROADWELL_EP:
				printf("Broadwell-EP");
				break;
			case CPU_SKYLAKE:
			case CPU_SKYLAKE_HS:
				printf("Skylake");
				break;
			case CPU_SKYLAKE_X:
				printf("Skylake-X");
				break;
			case CPU_KABYLAKE:
			case CPU_KABYLAKE_MOBILE:
				printf("Kaby Lake");
				break;
			case CPU_KNIGHTS_LANDING:
				printf("Knight's Landing");
				break;
			case CPU_KNIGHTS_MILL:
				printf("Knight's Mill");
				break;
			case CPU_ATOM_GOLDMONT:
			case CPU_ATOM_GEMINI_LAKE:
			case CPU_ATOM_DENVERTON:
				printf("Atom");
				break;
			default:
				printf("Unsupported model %d\n",model);
				model=-1;
				break;
		}
*/
	}

	if (vendor==CPU_VENDOR_AMD) {

		msr_rapl_units=MSR_AMD_RAPL_POWER_UNIT;
		msr_pkg_energy_status=MSR_AMD_PKG_ENERGY_STATUS;
		msr_pp0_energy_status=MSR_AMD_PP0_ENERGY_STATUS;

		if (family!=23) {
			printf("Wrong CPU family %d\n",family);
			return -1;
		}
		model=CPU_AMD_FAM17H;
	}

	fclose(fff);

	//printf(" Processor type\n");

	return model;
}




int Rapl::detect_packages(void) {

	char filename[BUFSIZ];
	FILE *fff;
	int package;
	int i;

	for(i=0;i<MAX_PACKAGES;i++) package_map[i]=-1;

	//printf("\t");
	for(i=0;i<MAX_CPUS;i++) {
		sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id",i);
		fff=fopen(filename,"r");
		if (fff==NULL) break;
		fscanf(fff,"%d",&package);
		//printf("%d (%d)",i,package);
		//if (i%8==7) printf("\n\t"); else printf(", ");
		fclose(fff);

		if (package_map[package]==-1) {
			total_packages++;
			package_map[package]=i;
		}

	}

//	printf("\n");

	total_cores=i;

//	printf("\tDetected %d cores in %d packages\n\n",
//		total_cores,total_packages);

	return 0;
}

void Rapl::detect_rapl_domains(int cpu_model){
	switch(cpu_model) {

			case CPU_SANDYBRIDGE_EP:
			case CPU_IVYBRIDGE_EP:
				pp0_avail=1;
				pp1_avail=0;
				dram_avail=1;
				different_units=0;
				psys_avail=0;
				break;

			case CPU_HASWELL_EP:
			case CPU_BROADWELL_EP:
			case CPU_SKYLAKE_X:
				pp0_avail=1;
				pp1_avail=0;
				dram_avail=1;
				different_units=1;
				psys_avail=0;
				break;

			case CPU_KNIGHTS_LANDING:
			case CPU_KNIGHTS_MILL:
				pp0_avail=0;
				pp1_avail=0;
				dram_avail=1;
				different_units=1;
				psys_avail=0;
				break;

			case CPU_SANDYBRIDGE:
			case CPU_IVYBRIDGE:
				pp0_avail=1;
				pp1_avail=1;
				dram_avail=0;
				different_units=0;
				psys_avail=0;
				break;

			case CPU_HASWELL:
			case CPU_HASWELL_ULT:
			case CPU_HASWELL_GT3E:
			case CPU_BROADWELL:
			case CPU_BROADWELL_GT3E:
			case CPU_ATOM_GOLDMONT:
			case CPU_ATOM_GEMINI_LAKE:
			case CPU_ATOM_DENVERTON:
				pp0_avail=1;
				pp1_avail=1;
				dram_avail=1;
				different_units=0;
				psys_avail=0;
				break;

			case CPU_SKYLAKE:
			case CPU_SKYLAKE_HS:
			case CPU_KABYLAKE:
			case CPU_KABYLAKE_MOBILE:
				pp0_avail=1;
				pp1_avail=1;
				dram_avail=1;
				different_units=0;
				psys_avail=1;
				break;

		}
}



void Rapl::hardware_info(){
	/* Read MSR_RAPL_POWER_UNIT Register */
        //MSR_AMD_RAPL_POWER_UNIT
        for(j=0;j<total_packages;j++){
                //printf("\tListing paramaters for package #%d\n",j);

                fd=open_msr(package_map[j]);

                /* Calculate the units used */
                result=read_msr(fd,msr_rapl_units);

                power_units=pow(0.5,(double)(result&0xf));
                cpu_energy_units[j]=pow(0.5,(double)((result>>8)&0x1f));
                time_units=pow(0.5,(double)((result>>16)&0xf));

                /* On Haswell EP and Knights Landing */
                /* The DRAM units differ from the CPU ones */
                if (different_units) {
                        dram_energy_units[j]=pow(0.5,(double)16);
                       // printf("DRAM: Using %lf instead of %lf\n",
                       //                 dram_energy_units[j],cpu_energy_units[j]);
                }
                else {
                        dram_energy_units[j]=cpu_energy_units[j];
                }

                //printf("\t\tPower units = %.3fW\n",power_units);
                //printf("\t\tCPU Energy units = %.8fJ\n",cpu_energy_units[j]);
                //printf("\t\tDRAM Energy units = %.8fJ\n",dram_energy_units[j]);
                //printf("\t\tTime units = %.8fs\n",time_units);
                //printf("\n");

		if (cpu_model!=CPU_AMD_FAM17H) {
                        /* Show package power info */
                        result=read_msr(fd,MSR_PKG_POWER_INFO);
                        thermal_spec_power=power_units*(double)(result&0x7fff);
                        //printf("\t\tPackage thermal spec: %.3fW\n",thermal_spec_power);
                        minimum_power=power_units*(double)((result>>16)&0x7fff);
                        //printf("\t\tPackage minimum power: %.3fW\n",minimum_power);
                        maximum_power=power_units*(double)((result>>32)&0x7fff);
                        //printf("\t\tPackage maximum power: %.3fW\n",maximum_power);
                        time_window=time_units*(double)((result>>48)&0x7fff);
                        //printf("\t\tPackage maximum time window: %.6fs\n",time_window);

                        /* Show package power limit */
                        result=read_msr(fd,MSR_PKG_RAPL_POWER_LIMIT);
                        //printf("\t\tPackage power limits are %s\n", (result >> 63) ? "locked" : "unlocked");
                        double pkg_power_limit_1 = power_units*(double)((result>>0)&0x7FFF);
                        double pkg_time_window_1 = time_units*(double)((result>>17)&0x007F);
                        //printf("\t\tPackage power limit #1: %.3fW for %.6fs (%s, %s)\n",
                        //pkg_power_limit_1, pkg_time_window_1,
                        //(result & (1LL<<15)) ? "enabled" : "disabled",
                        //(result & (1LL<<16)) ? "clamped" : "not_clamped");
                        double pkg_power_limit_2 = power_units*(double)((result>>32)&0x7FFF);
                        double pkg_time_window_2 = time_units*(double)((result>>49)&0x007F);
                        //printf("\t\tPackage power limit #2: %.3fW for %.6fs (%s, %s)\n",
                        //pkg_power_limit_2, pkg_time_window_2,
                        //(result & (1LL<<47)) ? "enabled" : "disabled",
                        //(result & (1LL<<48)) ? "clamped" : "not_clamped");
                }
 /* only available on *Bridge-EP */
                if ((cpu_model==CPU_SANDYBRIDGE_EP) || (cpu_model==CPU_IVYBRIDGE_EP)) {
                        result=read_msr(fd,MSR_PKG_PERF_STATUS);
                        double acc_pkg_throttled_time=(double)result*time_units;
                        //printf("\tAccumulated Package Throttled Time : %.6fs\n",
                        //                acc_pkg_throttled_time);
                }

                /* only available on *Bridge-EP */
                if ((cpu_model==CPU_SANDYBRIDGE_EP) || (cpu_model==CPU_IVYBRIDGE_EP)) {
                        result=read_msr(fd,MSR_PP0_PERF_STATUS);
                        double acc_pp0_throttled_time=(double)result*time_units;
                        //printf("\tPowerPlane0 (core) Accumulated Throttled Time "
                        //        ": %.6fs\n",acc_pp0_throttled_time);

                        result=read_msr(fd,MSR_PP0_POLICY);
                        int pp0_policy=(int)result&0x001f;
                        //printf("\tPowerPlane0 (core) for core %d policy: %d\n",core,pp0_policy);

                }

                if (pp1_avail) {
                        result=read_msr(fd,MSR_PP1_POLICY);
                        int pp1_policy=(int)result&0x001f;
                        //printf("\tPowerPlane1 (on-core GPU if avail) %d policy: %d\n",
                        //        core,pp1_policy);
                        }

                close(fd);
	}
}





Rapl::Rapl(int core) {

	cpu_model = detect_cpu();
	detect_packages();

	detect_rapl_domains(cpu_model);

	if (cpu_model<0) {
		printf("\tUnsupported CPU model %d\n",cpu_model);
		//return -1;
	}
	hardware_info();
	reset();
}

void Rapl::reset() {

	for(int j=0;j<total_packages;j++){
		prev_state[j] = &state1[j];
		current_state[j] = &state2[j];
		next_state[j] = &state3[j];

		// Initialize running_total
		running_total[j].pkg = 0.0;
		running_total[j].pp0 = 0.0;
		running_total[j].pp1 = 0.0;
		running_total[j].dram = 0.0;
		running_total[j].psys = 0.0;

		gettimeofday(&(running_total[j].tsc), NULL);

		current_state[j]->pkg = 0.0;
		current_state[j]->pp0 = 0.0;
		current_state[j]->pp1 = 0.0;
		current_state[j]->dram = 0.0;
		current_state[j]->psys = 0.0;

		next_state[j]->pkg = 0.0;
		next_state[j]->pp0 = 0.0;
		next_state[j]->pp1 = 0.0;
		next_state[j]->dram = 0.0;
		next_state[j]->psys = 0.0;

	}
	// sample twice to fill current and previous
	measure();
	measure();

}


int Rapl::open_msr(int core) {

	char msr_filename[BUFSIZ];
	int fd;

	sprintf(msr_filename, "/dev/cpu/%d/msr", core);
	fd = open(msr_filename, O_RDONLY);
	if ( fd < 0 ) {
		if ( errno == ENXIO ) {
			fprintf(stderr, "rdmsr: No CPU %d\n", core);
			exit(2);
		} else if ( errno == EIO ) {
			fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
					core);
			exit(3);
		} else {
			perror("rdmsr:open");
			fprintf(stderr,"Trying to open %s\n",msr_filename);
			exit(127);
		}
	}

	return fd;
}


long long Rapl::read_msr(int fd, unsigned int which) {

	uint64_t data;

	if ( pread(fd, &data, sizeof data, which) != sizeof data ) {
		perror("rdmsr:pread");
		fprintf(stderr,"Error reading MSR %x\n",which);
		exit(127);
	}

	return (long long)data;
}


void Rapl::measure() {
	uint32_t max_int = ~((uint32_t) 0);
	long long result,i;

	for(j=0;j<total_packages;j++) {

		fd=open_msr(package_map[j]);

		/* Package Energy */
		result=read_msr(fd,msr_pkg_energy_status) & max_int;
		//package_before[j]=(double)result*cpu_energy_units[j];
		package_before[j]=(double)result; 


		/* PP0 energy */
		/* Not available on Knights* */
		/* Always returns zero on Haswell-EP? */
		if (pp0_avail) {
			result=read_msr(fd,msr_pp0_energy_status);
			//pp0_before[j]=(double)result*cpu_energy_units[j];
			pp0_before[j]=(double)result;
		}

		/* PP1 energy */
		/* not available on *Bridge-EP */
		if (pp1_avail) {
		 	result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
			//pp1_before[j]=(double)result*cpu_energy_units[j];
			pp1_before[j]=(double)result;
		}

		/* Updated documentation (but not the Vol3B) says Haswell and	*/
		/* Broadwell have DRAM support too				*/
		if (dram_avail) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			//dram_before[j]=(double)result*dram_energy_units[j];
			dram_before[j]=(double)result;
		}

		/* Skylake and newer for Psys				*/
		if (psys_avail) {
			result=read_msr(fd,MSR_PLATFORM_ENERGY_STATUS);
			//psys_before[j]=(double)result*cpu_energy_units[j];
			psys_before[j]=(double)result;
		}

		close(fd);

		next_state[j]->pkg = package_before[j];
		next_state[j]->pp0 = pp0_before[j];
		next_state[j]->pp1 = pp1_before[j];
		next_state[j]->dram = dram_before[j];
		next_state[j]->psys = psys_before[j];

/*
		cout << "pkg" << next_state[j]->pkg << endl;
		cout << "pp0" << next_state[j]->pp0 << endl;
		cout << "pp1" << next_state[j]->pp1 << endl;
		cout << "dram" << next_state[j]->dram << endl;
*/

		gettimeofday(&(next_state[j]->tsc), NULL);

/*
		// Update running total
		running_total[j].pkg += energy_delta(current_state[j]->pkg, next_state[j]->pkg);
		running_total[j].pp0 += energy_delta(current_state[j]->pp0, next_state[j]->pp0);
		running_total[j].pp1 += energy_delta(current_state[j]->pp1, next_state[j]->pp1);
		running_total[j].dram += energy_delta(current_state[j]->dram, next_state[j]->dram);
		running_total[j].psys += energy_delta(current_state[j]->psys, next_state[j]->psys);

		cout << "running total pkg" << running_total[j].pkg << endl;
                cout << "running total pp0" << running_total[j].pp0 << endl;
                cout << "running total pp1" << running_total[j].pp1 << endl;
                cout << "running total dram" << running_total[j].dram << endl;
*/
		// Rotate states
		rapl_state_t *pprev_state = prev_state[j];
		prev_state[j] = current_state[j];
		current_state[j] = next_state[j];
		next_state[j] = pprev_state;

	}
}


void Rapl::measure_begin() {
        uint32_t max_int = ~((uint32_t) 0);
        long long result,i;

        for(j=0;j<total_packages;j++) {

		fd=open_msr(package_map[j]);

		/* Package Energy */
		result=read_msr(fd,msr_pkg_energy_status);
		package_before[j]=(double)result*cpu_energy_units[j];

		/* PP0 energy */
		/* Not available on Knights* */
		/* Always returns zero on Haswell-EP? */
		if (pp0_avail) {
			result=read_msr(fd,msr_pp0_energy_status);
			pp0_before[j]=(double)result*cpu_energy_units[j];
		}

		/* PP1 energy */
		/* not available on *Bridge-EP */
		if (pp1_avail) {
	 		result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
			pp1_before[j]=(double)result*cpu_energy_units[j];
		}


		/* Updated documentation (but not the Vol3B) says Haswell and	*/
		/* Broadwell have DRAM support too				*/
		if (dram_avail) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			dram_before[j]=(double)result*dram_energy_units[j];
		}


		/* Skylake and newer for Psys				*/
		if (psys_avail) {

			result=read_msr(fd,MSR_PLATFORM_ENERGY_STATUS);
			psys_before[j]=(double)result*cpu_energy_units[j];
		}

		close(fd);
	}

	gettimeofday(&(current_state[0]->tsc), NULL);

}



void Rapl::measure_end() {
        uint32_t max_int = ~((uint32_t) 0);
        long long result,i;
	
	gettimeofday(&(next_state[0]->tsc), NULL);


        for(j=0;j<total_packages;j++) {

		fd=open_msr(package_map[j]);

		printf("\tPackage %d:\n",j);

		result=read_msr(fd,msr_pkg_energy_status);
		package_after[j]=(double)result*cpu_energy_units[j];
		printf("\t\tPackage energy: %.6fJ\n",
			package_after[j]-package_before[j]);

		result=read_msr(fd,msr_pp0_energy_status);
		pp0_after[j]=(double)result*cpu_energy_units[j];
		printf("\t\tPowerPlane0 (cores): %.6fJ\n",
			pp0_after[j]-pp0_before[j]);

		/* not available on SandyBridge-EP */
		if (pp1_avail) {
			result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
			pp1_after[j]=(double)result*cpu_energy_units[j];
			printf("\t\tPowerPlane1 (on-core GPU if avail): %.6f J\n",
				pp1_after[j]-pp1_before[j]);
		}

		if (dram_avail) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			dram_after[j]=(double)result*dram_energy_units[j];
			printf("\t\tDRAM: %.6fJ\n",
				dram_after[j]-dram_before[j]);
		}

		if (psys_avail) {
			result=read_msr(fd,MSR_PLATFORM_ENERGY_STATUS);
			psys_after[j]=(double)result*cpu_energy_units[j];
			printf("\t\tPSYS: %.6fJ\n",
				psys_after[j]-psys_before[j]);
		}

		close(fd);

		running_total[j].pkg = package_after[j]-package_before[j];
                running_total[j].pp0 = pp0_after[j]-pp0_before[j];
                running_total[j].pp1 = pp1_after[j]-pp1_before[j];
                running_total[j].dram = dram_after[j]-dram_before[j];
                running_total[j].psys = psys_after[j]-psys_before[j];


	std::cout << "\t\tTime: "<< time_delta(&(current_state[0]->tsc), &(next_state[0]->tsc)) << std::endl;
	std:cout << "\t\tAverage Power: " << (package_after[j]-package_before[j])/(time_delta(&(current_state[0]->tsc), &(next_state[0]->tsc))) << std::endl;
	
	}

}





double Rapl::time_delta(struct timeval *begin, struct timeval *end) {
        return (end->tv_sec - begin->tv_sec)
                + ((end->tv_usec - begin->tv_usec)/1000000.0);
}

double Rapl::power(uint64_t before, uint64_t after, double time_delta, int package) {
	if (time_delta == 0.0f || time_delta == -0.0f) { return 0.0; }
	double energy = cpu_energy_units[package] * ((double) energy_delta(before,after));
	//double energy = ((double) energy_delta(before,after));
	return energy / time_delta;
}

uint64_t Rapl::energy_delta(uint64_t before, uint64_t after) {
	uint64_t max_int = ~((uint32_t) 0);
	uint64_t eng_delta = after - before;

	// Check for rollovers
	if (before > after) {
		eng_delta = after + (max_int - before);
	}

	return eng_delta;
}

double Rapl::pkg_current_power(int package) {
	double t;
	t = time_delta(&(prev_state[package]->tsc), &(current_state[package]->tsc));
	return power(prev_state[package]->pkg, current_state[package]->pkg, t, package);
}

double Rapl::pp0_current_power(int package) {
	double t;
	t = time_delta(&(prev_state[package]->tsc), &(current_state[package]->tsc));
	return power(prev_state[package]->pp0, current_state[package]->pp0, t, package);
}

double Rapl::pp1_current_power(int package) {
	double t;
	t = time_delta(&(prev_state[package]->tsc), &(current_state[package]->tsc));
	return power(prev_state[package]->pp1, current_state[package]->pp1, t, package);
}

double Rapl::dram_current_power(int package) {
	double t;
	t = time_delta(&(prev_state[package]->tsc), &(current_state[package]->tsc));
	return power(prev_state[package]->dram, current_state[package]->dram, t, package);
}

double Rapl::pkg_average_power(int package) {
	return pkg_total_energy(package)/total_time();
}

double Rapl::pp0_average_power(int package) {
	return pp0_total_energy(package)/total_time();
}

double Rapl::pp1_average_power(int package) {
	return pp1_total_energy(package)/total_time();
}

double Rapl::dram_average_power(int package) {
	return dram_total_energy(package) / total_time();
}

double Rapl::pkg_total_energy(int package) {
	//return cpu_energy_units[package] * ((double) running_total[package].pkg);
	//return ((double) running_total[package].pkg);
	//return running_total[package].pkg;
	//printf("%.6fJ", running_total[package].pkg);
	//printf("\t\tPackage energy: %.6fJ\n",
        //                package_after[package]-package_before[package]);
	return package_after[package]-package_before[package];
}

double Rapl::pp0_total_energy(int package) {
	//return cpu_energy_units[package] * ((double) running_total[package].pp0);
	return ((double) running_total[package].pp0);
}

double Rapl::pp1_total_energy(int package) {
	//return cpu_energy_units[package] * ((double) running_total[package].pp1);
	return ((double) running_total[package].pp1);
}

double Rapl::dram_total_energy(int package) {
	//return	dram_energy_units[package] * ((double) running_total[package].dram);
	//return	((double) running_total[package].dram);
	return dram_after[package]-dram_before[package];
}

double Rapl::total_energy_per_package(int package) {
        return (package_after[package]-package_before[package])+(dram_after[package]-dram_before[package]);
}

double Rapl::total_power_per_package(int package) {
        return total_energy_per_package(package)/total_time();
}

double Rapl::total_energy() {
	double energy=0.0;

	for(j=0;j<total_packages;j++) {
		energy+=total_energy_per_package(j);
	}

        return energy;
}

double Rapl::total_power() {
	double energy=0.0;
        return total_energy()/total_time();
}


double Rapl::total_time() {
	//return time_delta(&(running_total[0].tsc), &(current_state[0]->tsc));
	return time_delta(&(current_state[0]->tsc), &(next_state[0]->tsc));
}

double Rapl::current_time() {
	return time_delta(&(prev_state[0]->tsc), &(current_state[0]->tsc));
}
