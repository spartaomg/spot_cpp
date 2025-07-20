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
#include <sstream>
#include <filesystem>
#include <cmath>
#include <math.h>
#include <limits>
#include "thirdparty/lodepng.h"
#include <unordered_map>
#include <climits>
#include <bitset>
#include <bit>

using namespace std;

namespace fs = filesystem;

extern const int NumPalettes;		// = 64;
extern const int c64palettes_size;	// = 1024;
extern unsigned int c64palettes[];

extern const int paletteconvtab_size;
extern unsigned char paletteconvtab[];

extern int PaletteAssignment[16];

extern bool VerboseMode;
extern bool OnePassMode;

extern bool IsHires;

double HungarianAlgorithm(double m[16][16]);
int CalculateCompressedSize(const vector<unsigned char>& data);