#ifndef OZWU_H
#define OZWU_H

#include <stdio.h>
#include "ozwu_main.h"

int OzwuStartNetwork(Config *c);

int OzwuStopNetwork(Config *c);

int OzwuRunCommand(FILE* log, Config *c);


#endif
