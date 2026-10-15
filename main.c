#include "bladegps.h"

// Standalone entry point. The device is opened and closed in bladegps_main().
int main(int argc, char *argv[])
{
	return bladegps_main(NULL, argc, argv);
}
