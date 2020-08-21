

#include "tests.h"

#include <iostream>


int main(int argc, char* argv[])
{
	size_t len = 100;
	for (size_t i = 0; i < len; i++)
	{
		if (!testInterface())
			break;

		if (!testM2OQueue_ManyThreads_Pushes_1thread_Pops())
			break;
		if (!testM2OQueue_1thread_Pops_ManyThreads_Pushes())
			break;
		if (!testM2MQueue_ManyThreads_Pushes_ManyThreads_Pops())
			break;
		if (!testM2MQueue_ManyThreads_Pops_ManyThreads_Pushes())
			break;
		if (!testM2MQueue_ManyThreads_Pushes_1thread_Pops())
			break;

		std::cout << "iteration: " << i << ":" << len << " finished" << std::endl << std::endl;
	}
	return 0;
}