#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>

#include "Rapl.h"

int main(int argc, char *argv[]) {

	Rapl *rapl = new Rapl(0);

	// rapl sample
	rapl->measure_begin();
	
	for(int i=0;i<100000;i++)
	{
		std::cout << "print" << std::endl;
	}

	rapl->measure_end();
			
			std::cout << std::endl
				<< "\tTime:\t" << rapl->total_time() << " sec" << std::endl
				<< "\tTotal Energy:\t" << rapl->total_energy() << " J" << std::endl
				<< "\tAverage Power:\t" << rapl->total_power() << " W" << std::endl;
	
	return 0;
}
