#pragma once

#include <algorithm>
#include <iostream>
#include <string>
#include <string.h>
#include <array>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <math.h>
#include <limits>
#include "thirdparty/lodepng.h"

using namespace std;

namespace fs = filesystem;

extern const int NumPalettes;	// = 63;
extern const int c64palettes_size;	// = 1008;
extern unsigned int c64palettes[];

//extern const int oldc64palettes_size;
//extern unsigned char oldc64palettes[];
extern const int paletteconvtab_size;
extern unsigned char paletteconvtab[];

extern int PaletteAssignment[16];


double HungarianAlgorithm(double m[16][16]);// int* assignment[rows]);