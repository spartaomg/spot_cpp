
//new in v1.3
//option g for switch -f to save background color as a 1-byte *.bgc file 
//option x for switch -b to only create files using the first detected background color
//Hungarian algorithm to identify best palette match if no direct match found
//improved optimization (saves 54 bytes on the test corpus with Dali compared with v1.2)

//new in v1.4
//new optimization algorithm based on average color fragment length instead of color frequency to predict optimal color placement
//improved overlap correction
//SPOT now checks 8192 permutations and selects candidates for best compression based on the number of color fragments and color combinations,
//then passes them through a simple compression cost calculator to select the one that compresses best as final output
//total gain is 759 bytes vs. SPOT 1.3 and 284 bytes vs. P2P 1.8's overall best result in Burglar's benchmark
//new switch -v for verbose mode
//new switch -s for simple/speedy mode - bypasses multiple output candidate creation and analysis - helpful in case of huge, non-standard images
//accept VICE 384x272px PNG screenshots as input
//KoalaX (klx) file format for large, non-standard bitmaps
//klx layout is similar to Koala: bitmap width (double pixels) lo + hi, bitmap height lo + hi, bitmap pixel info, screen RAM, color RAM, background color

//new since v1.4
//bugfix to count color fragments correctly in case the last slot of a color space is used by the color - would result in a buggy output if a color was present in every block
//making sure each color order is only processed once
//adding hi-res graphics support

#include "common.h"

//#define DEBUG

const int VICE_PicW = 384;
const int VICE_PicH = 272;

vector <int> Predictors;

struct colorspace
{
    int Frequency;
    double SeqLen;          //Average sequence length
    double UnusedLen;       //Average length between sequences
    double Compactness;
    unsigned char Color;
    int OverlapWithSH;
    int OverlapWithSL;
    int OverlapWithCR;
    bool Used;
};

vector <colorspace> ColorSpace(16);

vector<unsigned char> arrBMP;

vector<vector <unsigned char>> vecBMP{ arrBMP };

bool VerboseMode = false;
bool OnePassMode = false;

bool IsHires = false;

int BitmapFrag[256]{};
int BmpFrag = 0;

int BestNumFrag{};
int BestNumFragCol{};

unsigned char PreferredFirst[16]{}, PreferredSecond[16]{}, PreferredThird[16]{};

int ThisCompress = 0;

int PrgLen = 0;
string InFile{}, OutFile{};
string FPath{}, FName{}, FExt{};

string CmdOptions = "k";                    //Default output file type = kla
string CmdColors = "0123456789abcdef";      //Default output background color: all possible colors
string CmdMode = "m";

bool C64Formats = false;

unsigned int PicW = 0;
unsigned int PicH = 0;
int CharCol = 0;
int CharRow = 0;

bool OutputKla = false;
bool OutputMap = false;
bool OutputCol = false;
bool OutputScr = false;
bool OutputCcr = false;
bool OutputObm = false;
bool OutputPng = false;
bool OutputBmp = false;
bool OutputBgc = false;
bool OutputKlx = false;

int NumBGCols = -1;
unsigned char BGCol, BGCols[16]{};  //Background color
const unsigned char UnusedColor = 0x10;

vector <unsigned char> ImgRaw;      //raw PNG/BMP/Koala/KoalaX
vector <unsigned char> Image;       //pixels in RGBA format (4 bytes per pixel)
vector <unsigned char> C64Bitmap;

unsigned char MUCSeq[16]{};
unsigned char MUCCol[16]{};
unsigned char MUCMed[16]{};
unsigned char MUCDen[16]{};

int ColTabSize = 0;

vector <unsigned char> ColTab0, ColTab1, ColTab2, ColTab3;
vector <unsigned char> Pic, PicMsk;       //Original picture
vector <unsigned char> BMP;                //C64 bitmap
vector <unsigned char> ColR, ColRAM, ScrHi, ScrLo, ScrRAM, ColMap;

typedef struct tagBITMAPFILEHEADER {
    int16_t bfType;
    int32_t bfSize;
    int16_t bfReserved1;
    int16_t bfReserved2;
    int32_t bfOffBits;
} BITMAPFILEHEADER, * LPBITMAPFILEHEADER, * PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    int32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    int16_t biPlanes;
    int16_t biBitCount;
    int32_t biCompression;
    int32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    int32_t biClrUsed;
    int32_t biClrImportant;
} BITMAPINFOHEADER, * LPBITMAPINFOHEADER, * PBITMAPINFOHEADER;

typedef struct tagRGBQUAD {
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[1];
} BITMAPINFO, * LPBITMAPINFO, * PBITMAPINFO;

BITMAPINFO BmpInfo;
BITMAPINFOHEADER BmpInfoHeader;

struct color {
    unsigned char R;
    unsigned char G;
    unsigned char B;
    unsigned char A;
};

struct yuv {
    double Y;
    double U;
    double V;
};

static const int NP = 64;
yuv c64palettesYUV[NP*16]{};

int minSumIndices[16]{};

string PaletteNames[NP]{
"VICE 3.6 Pepto PAL","VICE 3.6 Pepto PAL Old","VICE 3.6 Pepto NTSC Sony","VICE 3.6 Pepto NTSC Sony","VICE 3.6 Colodore","VICE 3.6 VICE","VICE 3.6 C64HQ","VICE 3.6 C64S",
"VICE 3.6 CCS64", "VICE 3.6 Frodo", "VICE 3.6 Godot", "VICE 3.6 PC64", "VICE 3.6 RGB", "VICE 3.6 ChristopherJam", "VICE 3.6 Deekay", "VICE 3.6 PALette",
"VICE 3.6 Ptoing", "VICE 3.6 Community Colors", "VICE 3.6 Pixcen", "VICE 3.6 VICE Internal", "VICE 3.6 Pepto PAL CRT", "VICE 3.6 Pepto PAL Old CRT", "VICE 3.6 Pepto NTSC Sony CRT", "VICE 3.6 Pepto NTSC CRT",
"VICE 3.6 Colodore CRT", "VICE 3.6 VICE CRT", "VICE 3.6 C64HQ CRT", "VICE 3.6 C64S CRT", "VICE 3.6 CCS64 CRT", "VICE 3.6 Frodo CRT", "VICE 3.6 Godot CRT", "VICE 3.6 PC64 CRT",
"VICE 3.6 RGB CRT", "VICE 3.6 ChristopherJam CRT", "VICE 3.6 Deekay CRT", "VICE 3.6 PALette CRT", "VICE 3.6 Ptoing CRT", "VICE 3.6 Community Colors CRT", "VICE 3.6 Pixcen CRT", "VICE 3.8 C64HQ",
"VICE 3.8 C64S", "VICE 3.8 CCS64", "VICE 3.8 ChristopherJam", "VICE 3.8 Colodore", "VICE 3.8 Community Colors", "VICE 3.8 Deekay", "VICE 3.8 Frodo", "VICE 3.8 Godot",
"VICE 3.8 PALette", "VICE 3.8 PALette 6569R1", "VICE 3.8 PALette 6569R5", "VICE 3.8 PALette 8565R2", "VICE 3.8 PC64", "VICE 3.8 Pepto NTSC Sony", "VICE 3.8 Pepto NTSC", "VICE 3.8 Pepto PAL",
"VICE 3.8 Pepto PAL Old", "VICE 3.8 Pixcen", "VICE 3.8 Ptoing", "VICE 3.8 RGB", "VICE 3.8 VICE Original", "VICE 3.8 VICE Internal", "Pixcen Colodore","C64GFX.COM PEPTOette"
};

int VICE_36_Pixcen = 18;    //VICE 3.6 Pixcen

//----------------------------------------------------------------------------------------------------------------------------------------------------------

string ConvertIntToHextString(const int i, const int hexlen)
{
    std::stringstream hexstream;
    hexstream << setfill('0') << setw(hexlen) << hex << i;
    return hexstream.str();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

int ReadBinaryFile(const string& FileName, vector<unsigned char>& prg)
{
    if (FileName.empty())
    {
        return -1;
    }

    if (!fs::exists(FileName))
    {
        return -1;
    }

    prg.clear();

    ifstream infile(FileName, ios_base::binary);

    if (infile.fail())
    {
        return -1;
    }

    infile.seekg(0, ios_base::end);
    int length = infile.tellg();
    infile.seekg(0, ios_base::beg);

    prg.reserve(length);

    copy(istreambuf_iterator<char>(infile), istreambuf_iterator<char>(), back_inserter(prg));

    infile.close();

    return length;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool CreateDirectory(const string& DiskDir)
{
    if (DiskDir.empty())
        return true;

    if (!fs::exists(DiskDir))
    {
        cout << "Creating folder: " << DiskDir << "\n";
        fs::create_directories(DiskDir);
    }

    if (!fs::exists(DiskDir))
    {
        cerr << "***CRITICAL***\tUnable to create the following folder: " << DiskDir << "\n\n";
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool WriteBinaryFile(const string& FileName, unsigned char*& Binary, int FileSize)
{
    if (FileName.empty())
    {
        return false;
    }

    string DiskDir{};

    for (size_t i = 0; i <= FileName.length() - 1; i++)
    {
#if _WIN32 
        if ((FileName[i] == '\\') || (FileName[i] == '/'))
        {
            if (DiskDir[DiskDir.length() - 1] != ':')   //Don't try to create root directory
            {
                if (!CreateDirectory(DiskDir))
                    return false;
            }
        }
#elif __APPLE__ || __linux__
        if ((FileName[i] == '/') && (DiskDir.size() > 0) && (DiskDir != "~"))   //Don't try to create root directory and home directory
        {
            if (!CreateDirectory(DiskDir))
                return false;
        }
#endif
        DiskDir += FileName[i];
    }

    ofstream myFile(FileName, ios::out | ios::binary);

    if (myFile.is_open())
    {
        cout << "Writing " + FileName + "...\n";
        myFile.write((char*)&Binary[0], FileSize);

        if (!myFile.good())
        {
            cerr << "***CRITICAL***\tError during writing " << FileName << "\n";
            myFile.close();
            return false;
        }

        myFile.close();
        return true;
    }
    else
    {
        cerr << "***CRITICAL***\tError opening file for writing " << FileName << "\n\n";
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool WriteBinaryFile(const string& FileName, vector <unsigned char>& Binary)
{
    if (FileName.empty())
    {
        return false;
    }

    string DiskDir{};

    for (size_t i = 0; i <= FileName.length() - 1; i++)
    {
#if _WIN32 
        if ((FileName[i] == '\\') || (FileName[i] == '/'))
        {
            if (DiskDir[DiskDir.length() - 1] != ':')   //Don't try to create root directory
            {
                if (!CreateDirectory(DiskDir))
                    return false;
            }
        }
#elif __APPLE__ || __linux__
        if ((FileName[i] == '/') && (DiskDir.size() > 0) && (DiskDir != "~"))   //Don't try to create root directory and home directory
        {
            if (!CreateDirectory(DiskDir))
                return false;
        }
#endif
        DiskDir += FileName[i];
    }

    ofstream myFile(FileName, ios::out | ios::binary);

    if (myFile.is_open())
    {
        cout << "Writing " + FileName + "...\n";
        myFile.write(reinterpret_cast<const char*>(&Binary[0]), Binary.size());
        
        if (!myFile.good())
        {
            cerr << "***CRITICAL***\tError during writing " << FileName << "\n";
            myFile.close();
            return false;
        }

        myFile.close();
        return true;
    }
    else
    {
        cerr << "***CRITICAL***\tError opening file for writing " << FileName << "\n\n";
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

double YUVDistance(yuv YUV1, yuv YUV2)
{
    double dY = YUV1.Y - YUV2.Y;
    double dU = YUV1.U - YUV2.U;
    double dV = YUV1.V - YUV2.V;

    double YUVD = dY * dY + dU * dU + dV * dV;

    return YUVD;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

yuv RGB2YUV(int RGBColor)
{
    int R = (RGBColor >> 16) & 0xff;
    int G = (RGBColor >> 8) & 0xff;
    int B = RGBColor & 0xff;

    yuv YUV{};

    YUV.Y = 0.299 * R + 0.587 * G + 0.114 * B;
    YUV.U = -0.14713 * R - 0.28886 * G + 0.436 * B;     //0.492 * (B - YUV.Y);
    YUV.V = 0.615 * R - 0.51499 * G - 0.10001 * B;      //0.877 * (R - YUV.Y);

    return YUV;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool SetColor(std::vector<unsigned char>& Img, size_t X, size_t Y, size_t PicWidth, int Col)
{
    size_t Pos = (Y * (size_t)PicWidth * 4) + (X * 4);

    if (Pos + 3 > Img.size())
        return false;

    Img[Pos + 0] = (Col >> 16) & 0xff;
    Img[Pos + 1] = (Col >> 8) & 0xff;
    Img[Pos + 2] = Col & 0xff;
    Img[Pos + 3] = 0xff;    // (Col >> 24) & 0xff;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

unsigned int GetColor(std::vector<unsigned char>& Img, size_t X, size_t Y, size_t PicWidth)
{
    size_t Pos = (Y * (size_t)PicWidth * 4) + (X * 4);

    return (Img[Pos + 0] << 16) + (Img[Pos + 1] << 8) + Img[Pos + 2];
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool SetPixel(std::vector<unsigned char>& Img,size_t X, size_t Y, size_t PicWidth, color Col)
{
    size_t Pos = (Y * (size_t)PicWidth * 4) + (X * 4);

    if (Pos + 3 > Img.size())
        return false;

    Img[Pos + 0] = Col.R;
    Img[Pos + 1] = Col.G;
    Img[Pos + 2] = Col.B;
    Img[Pos + 3] = 0xff;    // Col.A;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

color GetPixel(std::vector<unsigned char>& Img, size_t X, size_t Y, size_t PicWidth)
{
    size_t Pos = (Y * (size_t)PicWidth * 4) + (X * 4);

    color Col{};

    Col.R = Img[Pos + 0];
    Col.G = Img[Pos + 1];
    Col.B = Img[Pos + 2];
    Col.A = Img[Pos + 3];

    return Col;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool SaveBmp()
{
    BITMAPFILEHEADER bfh{}; //0x0e bytes

    int SizeOfBfh = sizeof(bfh.bfType) + sizeof(bfh.bfSize) + sizeof(bfh.bfReserved1) + sizeof(bfh.bfReserved2) + sizeof(bfh.bfOffBits);    //=14

    //sizeof(bfh) = 16 for some reason and bfType takes 4 bytes instead of 2, so we need a workaround

    bfh.bfType = 0x4d42;
    bfh.bfOffBits = SizeOfBfh + sizeof(tagBITMAPINFOHEADER) + 0x40;
    bfh.bfSize = (PicW / 2) * PicH + bfh.bfOffBits;     //4 bits per pixel
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;

    BITMAPINFOHEADER bih{}; //0x28 bytes

    bih.biSize = sizeof(tagBITMAPINFOHEADER);
    bih.biWidth = PicW;
    bih.biHeight = PicH;
    bih.biPlanes = 1;
    bih.biBitCount = 4;
    bih.biCompression = 0;
    bih.biSizeImage = 0;
    bih.biXPelsPerMeter = 0xec4;
    bih.biYPelsPerMeter = 0xec4;
    bih.biClrUsed = 16;
    bih.biClrImportant = 16;

    //Pixcen Palette
    const int c64palette_size = 16;
    unsigned int BmpPalette[c64palette_size] = {}; 

    for (size_t c = 0; c < 16; c++)
    {
        BmpPalette[c] = c64palettes[(size_t)VICE_36_Pixcen * c64palette_size + c] | 0xff000000;
    }
    
    vector <unsigned char> BmpOut(bfh.bfSize, 0);

    //memcpy(BmpOut.data() + 0, &bfh, sizeof(bfh));    //this would copy 16 bytes instead of 14...
    memcpy(BmpOut.data() + 0, &bfh.bfType, sizeof(bfh.bfType));
    memcpy(BmpOut.data() + 2, &bfh.bfSize, sizeof(bfh.bfSize));
    memcpy(BmpOut.data() + 6, &bfh.bfReserved1, sizeof(bfh.bfReserved1));
    memcpy(BmpOut.data() + 8, &bfh.bfReserved2, sizeof(bfh.bfReserved2));
    memcpy(BmpOut.data() + 10, &bfh.bfOffBits, sizeof(bfh.bfOffBits));
    memcpy(BmpOut.data() + SizeOfBfh, &bih, sizeof(bih));
    memcpy(BmpOut.data() + SizeOfBfh + sizeof(bih), &BmpPalette, sizeof(BmpPalette));

    for (int y = 0; y < (int)PicH; y++)
    {
        for (int x = 0; x < (int)PicW; x += 2)
        {
            unsigned int ThisCol = GetColor(C64Bitmap, x, y, PicW) | 0xff000000;
            unsigned int NextCol = GetColor(C64Bitmap, (size_t)x + 1, y, PicW) | 0xff000000;
            
            for (int i = 0; i < 16; i++)
            {
                if (ThisCol == BmpPalette[i])
                {
                    for (int j = 0; j < 16; j++)
                    {
                        if (NextCol == BmpPalette[j])
                        {
                            size_t BmpOffset = bfh.bfOffBits + (((size_t)PicH - 1 - y) * (size_t)PicW / 2) + (size_t)x / 2;
                            BmpOut[BmpOffset] = i * 16 + j;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }

    bool WriteReturn = WriteBinaryFile(OutFile + ".bmp", BmpOut);

    return WriteReturn;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool SaveImgFormat()
{
    if ((OutputPng == 1) ||(OutputBmp == 1))
    {
        //Save PNG or BMP

        if (OutFile.empty())
        {
            return false;
        }

        //Make sure directory exists

        string DiskDir{};

        for (size_t i = 0; i <= OutFile.length() - 1; i++)
        {
#if _WIN32 
            if ((OutFile[i] == '\\') || (OutFile[i] == '/'))
            {
                if (DiskDir[DiskDir.length() - 1] != ':')   //Don't try to create root directory
                {
                    if (!CreateDirectory(DiskDir))
                        return false;
                }
            }
#elif __APPLE__ || __linux__
            if ((OutFile[i] == '/') && (DiskDir.size() > 0) && (DiskDir != "~"))   //Don't try to create root directory and home directory
            {
                if (!CreateDirectory(DiskDir))
                    return false;
            }
#endif
            DiskDir += OutFile[i];
        }
    }
    
    if (OutputPng == 1)
    {
        cout << "Writing " + OutFile + ".png...\n";

        unsigned int error = lodepng::encode(OutFile + ".png", C64Bitmap, PicW, PicH);

        if (error)
        {
            cout << "Error during encoding and saving PNG.\n";
            return false;
        }
    }
    
    if (OutputBmp == 1)
    {
        //cout << "Writing " + OutFile + ".bmp...\n";

        bool SaveReturn = SaveBmp();
        if (!SaveReturn)
        {
            cout << "Error during encoding and saving BMP.\n";
            return false;
        }
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void CreateBitmapData()
{
    //----------------------------------------------------------------------------
    //Rebuild the image
    //----------------------------------------------------------------------------

    arrBMP.resize(((size_t)ColTabSize * 10) + 1);

    unsigned char Col1{}, Col2{}, Col3{};

    if (!IsHires)
    {
        //Replace C64 colors with respective bit pairs
        for (int CY = 0; CY < CharRow; CY++)
        {
            for (int CX = 0; CX < CharCol; CX++)
            {
                int CharIndex = (CY * CharCol) + CX;
                Col1 = ScrHi[CharIndex];        //Fetch colors from tabs
                Col2 = ScrLo[CharIndex];
                Col3 = ColR[CharIndex];
                for (int BY = 0; BY < 8; BY++)
                {
                    for (int BX = 0; BX < 8; BX += 2)
                    {
                        //Calculate pixel position in array
                        int CP = (CY * PicW * 8) + (CX * 8) + (BY * PicW) + BX;
                        if (Pic[CP] == BGCol)
                        {
                            PicMsk[CP] = 0;
                        }
                        else if (Pic[CP] == Col1)
                        {
                            PicMsk[CP] = 1;
                        }
                        else if (Pic[CP] == Col2)
                        {
                            PicMsk[CP] = 2;
                        }
                        else if (Pic[CP] == Col3)
                        {
                            PicMsk[CP] = 3;
                        }
                    }
                }
            }
        }

        //Finally, convert bit pairs to final bitmap
        for (int CY = 0; CY < CharRow; CY++)
        {
            for (int CX = 0; CX < CharCol; CX++)
            {
                for (int BY = 0; BY < 8; BY++)
                {
                    int CP = (CY * PicW * 8) + (CX * 8) + (BY * PicW);
                    unsigned char V = (PicMsk[CP] * 64) + (PicMsk[CP + 2] * 16) + (PicMsk[CP + 4] * 4) + PicMsk[CP + 6];
                    CP = (CY * CharCol * 8) + (CX * 8) + BY;
                    BMP[CP] = V;
                    arrBMP[CP] = V;
                }
            }
        }
    }
    else
    {
        //Replace C64 colors with respective bits
        for (int CY = 0; CY < CharRow; CY++)
        {
            for (int CX = 0; CX < CharCol; CX++)
            {
                int CharIndex = (CY * CharCol) + CX;
                Col1 = ScrHi[CharIndex];        //Fetch colors from tabs
                Col2 = ScrLo[CharIndex];
                for (int BY = 0; BY < 8; BY++)
                {
                    for (int BX = 0; BX < 8; BX ++)
                    {
                        //Calculate pixel position in array
                        int CP = (CY * PicW * 8) + (CX * 8) + (BY * PicW) + BX;
                        if (Pic[CP] == Col1)
                        {
                            PicMsk[CP] = 1;
                        }
                        else if (Pic[CP] == Col2)
                        {
                            PicMsk[CP] = 0;
                        }
                        else
                        {
                            PicMsk[CP] = 2;
                        }
                    }
                }
            }
        }

        //Finally, convert bit pairs to final bitmap
        for (int CY = 0; CY < CharRow; CY++)
        {
            for (int CX = 0; CX < CharCol; CX++)
            {
                for (int BY = 0; BY < 8; BY++)
                {
                    int CP = (CY * PicW * 8) + (CX * 8) + (BY * PicW);
                    unsigned char V = 0;
                    for (int BX = 0; BX < 8; BX++)
                    {
                        V = (V * 2) + PicMsk[(size_t)CP + BX];
                    }
                    CP = (CY * CharCol * 8) + (CX * 8) + BY;
                    BMP[CP] = V;
                    arrBMP[CP] = V;
                }
            }
        }
    }

    for (size_t i = 0; i < (size_t)ColTabSize; i++)
    {
        arrBMP[(size_t)(8 * ColTabSize) + i] = ScrRAM[i];
        arrBMP[(size_t)(9 * ColTabSize) + i] = ColR[i];
    }

    arrBMP[(size_t)(10 * ColTabSize)] = BGCol;
    
    vecBMP.push_back(arrBMP);

}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool RebuildImage(string OutFileName)
{

    string SaveFile = OutFileName;
    if ((NumBGCols > 1) && (CmdColors.size() != 1))
    {
        //If we have more than 1 possible background color AND the user requested more than one background color
        //Then mark the output file name with _0x
        SaveFile += "_" + ConvertIntToHextString((int)BGCol, 2);
    }

    if (OutputKlx)
    {
        if (PicW > 0xffff || PicH > 0xffff)
        {
            cout << "***CRITICAL***\tUnable to save klx format. The dimensions of the image cannot exceed 65535 double pixels x 65535 pixels.\n";
            return false;
        }
        else
        {
            vector<unsigned char> KLX(10 * ColTabSize + 5, 0);
            KLX[0] = PicW & 0xff;
            KLX[1] = (PicW >> 8) & 0xff;
            KLX[2] = PicH & 0xff;
            KLX[3] = (PicH >> 8) & 0xff;

            for (int i = 0; i < ColTabSize * 8; i++)
            {
                KLX[4 + i] = BMP[i];
            }

            for (int i = 0; i < ColTabSize; i++)
            {
                KLX[(size_t)(ColTabSize * 8) + 4 + i] = ScrRAM[i];
                KLX[(size_t)(ColTabSize * 9) + 4 + i] = ColR[i];
            }

            KLX[(size_t)(ColTabSize * 10) + 4] = BGCol;

            //Save KLX
            WriteBinaryFile(SaveFile + ".klx", KLX);
        }
    }

    if ((OutputKla) && (CharRow >= 25) && (CharCol >= 40))
    {
        //Save Koala only if bitmap is at least 320x200 pixels
        vector <unsigned char> KLA(10003,0);

        KLA[1] = 0x60;
        KLA[10002] = BGCol;

        int StartCX = (CharCol / 2) - 20;
        int StartCY = (CharRow / 2) - 12;
        int StartBY = (CharCol * 4) - 160;

        for (size_t CY = 0; CY < 25; CY++)
        {
            for (size_t BY = 0; BY < 320; BY++)
            {
                KLA[(CY * 320) + BY + 2] = BMP[((StartCY + CY) * CharCol * 8) + StartBY + BY];
            }
        }

        for (size_t CY = 0; CY < 25; CY++)
        {
            for (size_t CX = 0; CX < 40; CX++)
            {
                KLA[8002 + (CY * 40) + CX] = ScrRAM[((StartCY + CY) * CharCol) + StartCX + CX];
                KLA[9002 + (CY * 40) + CX] = ColR[((StartCY + CY) * CharCol) + StartCX + CX];
            }
        }
        //Save KLA
        WriteBinaryFile(SaveFile + ".kla", KLA);
    }

    //Save bitmap, color RAM, and screen RAM

    if (OutputMap)
    {
        WriteBinaryFile(SaveFile + ".map", BMP);
    }

    if (OutputCol)
    {
        WriteBinaryFile(SaveFile + ".col", ColR);
    }

    if (OutputScr)
    {
        WriteBinaryFile(SaveFile + ".scr", ScrRAM);
    }

    if (OutputBgc)
    {
        vector <unsigned char> vBGC(1, BGCol);
        WriteBinaryFile(SaveFile + ".bgc", vBGC);
    }

    vector <unsigned char> CCR(ColTabSize / 2, 0);
    //unsigned char* CCR{};
    //CCR = new unsigned char[ColTabSize / 2] {};

    for (int i = 0; i < ColTabSize / 2; i++)
    {
        CCR[i] = ((ColR[i * 2] % 16) * 16) + (ColR[(i * 2) + 1] % 16);
    }

    //Save compressed ColorRAM wih halfbytes combined

    if (OutputCcr)
    {
        WriteBinaryFile(SaveFile + ".ccr", CCR);
    }

    //Save optimized bitmap file format only if bitmap is at least 320x200 pixels
    //Bitmap is stored column wise, color spaces are stored row wise, color RAM is compressed

    if ((OutputObm) && (CharRow >= 25) && (CharCol >= 40))
    {
        vector <unsigned char> OBM(9503,0);
        OBM[1] = 0x60;
        OBM[9502] = BGCol;

        int StartCX = (CharCol / 2) - 20;
        int StartCY = (CharRow / 2) - 12;
        //int StartBY = (CharCol * 4) - 160;

        //Bitmap stored column wise
        for (size_t CX = 0; CX < 40; CX++)
        {
            for (size_t CY = 0; CY < 25; CY++)
            {
                for (size_t BY = 0; BY < 8; BY++)
                {
                    OBM[(CX * 200) + (CY * 8) + BY + 2] = BMP[((StartCY + CY) * CharCol * 8) + ((StartCX + CX) * 8) + BY];
                }
            }
        }

        //Screen RAM stored row wise
        for (size_t CY = 0; CY < 25; CY++)
        {
            for (size_t CX = 0; CX < 40; CX++)
            {
                OBM[8002 + (CY * 40) + CX] = ScrRAM[((StartCY + CY) * CharCol) + StartCX + CX];
            }
        }

        StartCX /= 2;

        //Compressed Color RAM stored row wise
        for (size_t CY = 0; CY < 25; CY++)
        {
            for (size_t CX = 0; CX < 20; CX++)
            {
                OBM[9002 + (CY * 20) + CX] = CCR[((StartCY + CY) * (CharCol / 2)) + StartCX + CX];
            }
        }

        //Save optimized bitmap file format
        WriteBinaryFile(SaveFile + ".obm", OBM);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
/*
int CalculateFragmentation()
{
    int LastOffset[6][256]{};   //The last offset of each possible value (0-255) in each of the 6 different combinations
    int Literals[6][256]{};

    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 256; j++)
        {
            LastOffset[i][j] = -64;
        }
    }

    for (int i = 0; i < ColTabSize; i++)
    {
        if (i - LastOffset[0][(ScrHi[i] * 16 + ScrLo[i])] > 64)
        {
            //New literal
            Literals[0][i]++;
        }
        LastOffset[0][(ScrHi[i] * 16 + ScrLo[i])] = i;

        if (i - LastOffset[1][(ScrLo[i] * 16 + ScrHi[i])] > 64)
        {
            //New literal
            Literals[1][i]++;
        }
        LastOffset[1][(ScrLo[i] * 16 + ScrHi[i])] = i;


        if (i - LastOffset[2][(ScrHi[i] * 16 + ColRAM[i])] > 64)
        {
            //New literal
            Literals[2][i]++;
        }
        LastOffset[2][(ScrHi[i] * 16 + ColRAM[i])] = i;

        if (i - LastOffset[3][(ColRAM[i] * 16 + ScrHi[i])] > 64)
        {
            //New literal
            Literals[3][i]++;
        }
        LastOffset[3][(ColRAM[i] * 16 + ScrHi[i])] = i;


        if (i - LastOffset[4][(ScrLo[i] * 16 + ColRAM[i])] > 64)
        {
            //New literal
            Literals[4][i]++;
        }
        LastOffset[4][(ScrLo[i] * 16 + ColRAM[i])] = i;

        if (i - LastOffset[5][(ColRAM[i] * 16 + ScrLo[i])] > 64)
        {
            //New literal
            Literals[5][i]++;
        }
        LastOffset[5][(ColRAM[i] * 16 + ScrLo[i])] = i;
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
*/
int FindBestLayout()
{
    int FragSR0a = 0;   //Hi * 16 + Lo
    int FragSR0b = 0;   //Lo * 16 + Hi
    int FragCR0 = 0;    //CR
    int FragSR1a = 0;   //Hi * 16 + CR
    int FragSR1b = 0;   //CR * 16 + Hi
    int FragCR1 = 0;    //Lo
    int FragSR2a = 0;   //Lo * 16 + CR
    int FragSR2b = 0;   //CR * 16 + Lo
    int FragCR2 = 0;    //Hi

    // Using XOR for inversion is efficient, but we can initialize with values that won't match the first item
    // to ensure we count the first fragment properly
    unsigned char LastSR0a = (ScrHi[0] * 16 + ScrLo[0]) ^ 0xff;
    unsigned char LastSR0b = (ScrLo[0] * 16 + ScrHi[0]) ^ 0xff;
    unsigned char LastCR0 = ColRAM[0] ^ 0x0f;
    unsigned char LastSR1a = (ScrHi[0] * 16 + ColRAM[0]) ^ 0xff;
    unsigned char LastSR1b = (ColRAM[0] * 16 + ScrHi[0]) ^ 0xff;
    unsigned char LastCR1 = ScrLo[0] ^ 0x0f;
    unsigned char LastSR2a = (ScrLo[0] * 16 + ColRAM[0]) ^ 0xff;
    unsigned char LastSR2b = (ColRAM[0] * 16 + ScrLo[0]) ^ 0xff;
    unsigned char LastCR2 = ScrHi[0] ^ 0x0f;

    // Use std::bitset instead of arrays for better cache utilization and memory efficiency
    std::bitset<256> UsedSR0a;
    std::bitset<256> UsedSR0b;
    std::bitset<16> UsedCR0;
    std::bitset<256> UsedSR1a;
    std::bitset<256> UsedSR1b;
    std::bitset<16> UsedCR1;
    std::bitset<256> UsedSR2a;
    std::bitset<256> UsedSR2b;
    std::bitset<16> UsedCR2;

    // Cache value computations to avoid recalculating
    unsigned char currentVal;

    for (int i = 0; i < ColTabSize; i++)
    {
        // Precompute common values to avoid duplicate calculations
        unsigned char hiVal = ScrHi[i];
        unsigned char loVal = ScrLo[i];
        unsigned char crVal = ColRAM[i];

        // SR0a: Hi * 16 + Lo
        currentVal = hiVal * 16 + loVal;
        if (currentVal != LastSR0a)
        {
            LastSR0a = currentVal;
            FragSR0a++;
        }
        UsedSR0a.set(currentVal);

        if (!IsHires)
        {
            // SR0b: Lo * 16 + Hi
            currentVal = loVal * 16 + hiVal;
            if (currentVal != LastSR0b)
            {
                LastSR0b = currentVal;
                FragSR0b++;
            }
            UsedSR0b.set(currentVal);

            // CR0: ColRAM
            if (crVal != LastCR0)
            {
                LastCR0 = crVal;
                FragCR0++;
            }
            UsedCR0.set(crVal);

            // SR1a: Hi * 16 + CR
            currentVal = hiVal * 16 + crVal;
            if (currentVal != LastSR1a)
            {
                LastSR1a = currentVal;
                FragSR1a++;
            }
            UsedSR1a.set(currentVal);

            // SR1b: CR * 16 + Hi
            currentVal = crVal * 16 + hiVal;
            if (currentVal != LastSR1b)
            {
                LastSR1b = currentVal;
                FragSR1b++;
            }
            UsedSR1b.set(currentVal);

            // CR1: ScrLo
            if (loVal != LastCR1)
            {
                LastCR1 = loVal;
                FragCR1++;
            }
            UsedCR1.set(loVal);

            // SR2a: Lo * 16 + CR
            currentVal = loVal * 16 + crVal;
            if (currentVal != LastSR2a)
            {
                LastSR2a = currentVal;
                FragSR2a++;
            }
            UsedSR2a.set(currentVal);

            // SR2b: CR * 16 + Lo
            currentVal = crVal * 16 + loVal;
            if (currentVal != LastSR2b)
            {
                LastSR2b = currentVal;
                FragSR2b++;
            }
            UsedSR2b.set(currentVal);

            // CR2: ScrHi
            if (hiVal != LastCR2)
            {
                LastCR2 = hiVal;
                FragCR2++;
            }
            UsedCR2.set(hiVal);

        }
    }

    // Count unique colors in each layout configuration
    int NumColSR0a = UsedSR0a.count();
    int NumColSR0b = UsedSR0b.count();
    int NumColCR0 = UsedCR0.count();
    int NumColSR1a = UsedSR1a.count();
    int NumColSR1b = UsedSR1b.count();
    int NumColCR1 = UsedCR1.count();
    int NumColSR2a = UsedSR2a.count();
    int NumColSR2b = UsedSR2b.count();
    int NumColCR2 = UsedCR2.count();

    // Create arrays of the values for each layout
    int NumCol[6] = {
        NumColCR0 + NumColSR0a,
        NumColCR0 + NumColSR0b,
        NumColCR1 + NumColSR1a,
        NumColCR1 + NumColSR1b,
        NumColCR2 + NumColSR2a,
        NumColCR2 + NumColSR2b
    };

    int NumFrag[6] = {
        FragSR0a + FragCR0,    // HiLo, CR
        FragSR0b + FragCR0,    // LoHi, CR
        FragSR1a + FragCR1,    // HiCR, Lo
        FragSR1b + FragCR1,    // CRHi, Lo
        FragSR2a + FragCR2,    // LoCR, Hi
        FragSR2b + FragCR2     // CRLo, Hi
    };

    int Layout = 0;

    if (!IsHires)
    {
        if ((NumFrag[1] <= NumFrag[0]) && (NumFrag[1] <= NumFrag[2]) && (NumFrag[1] <= NumFrag[3]) && (NumFrag[1] <= NumFrag[4]) && (NumFrag[1] <= NumFrag[5]))  //Lo * 16 + Hi, CR
        {
            Layout = 1;
        }
        else if ((NumFrag[0] <= NumFrag[1]) && (NumFrag[0] <= NumFrag[2]) && (NumFrag[0] <= NumFrag[3]) && (NumFrag[0] <= NumFrag[4]) && (NumFrag[0] <= NumFrag[5]))   //Hi * 16 + Lo, CR
        {
            Layout = 1;
        }
        else if ((NumFrag[3] <= NumFrag[0]) && (NumFrag[3] <= NumFrag[1]) && (NumFrag[3] <= NumFrag[2]) && (NumFrag[3] <= NumFrag[4]) && (NumFrag[3] <= NumFrag[5]))  //CR * 16 + Hi, Lo
        {
            Layout = 3;
        }
        else if ((NumFrag[2] <= NumFrag[0]) && (NumFrag[2] <= NumFrag[1]) && (NumFrag[2] <= NumFrag[3]) && (NumFrag[2] <= NumFrag[4]) && (NumFrag[2] <= NumFrag[5]))  //Hi * 16 + CR, Lo
        {
            Layout = 3;
        }
        else if ((NumFrag[4] <= NumFrag[0]) && (NumFrag[4] <= NumFrag[1]) && (NumFrag[4] <= NumFrag[2]) && (NumFrag[4] <= NumFrag[3]) && (NumFrag[4] <= NumFrag[5]))  //Lo * 16 + CR, Hi
        {
            Layout = 4;
        }
        else if ((NumFrag[5] <= NumFrag[0]) && (NumFrag[5] <= NumFrag[1]) && (NumFrag[5] <= NumFrag[2]) && (NumFrag[5] <= NumFrag[3]) && (NumFrag[5] <= NumFrag[4]))  //CR * 16 + Lo, Hi
        {
            Layout = 4;
        }
    }

    BestNumFrag = NumFrag[Layout];
    BestNumFragCol = NumCol[Layout];

    return Layout;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void MoveMatchingSingles()
{
    unsigned char Tmp{};

    for (int i = 1; i < ColTabSize - 1; i++)
    {
        if ((ScrHi[i] != ScrHi[i - 1]) && (ScrLo[i] != ScrLo[i - 1]) && (ScrHi[i] != ScrHi[i + 1]) && (ScrLo[i] != ScrLo[i + 1]))
        {
            if ((ScrHi[i] == ScrLo[i - 1]) || (ScrLo[i] == ScrHi[i - 1]))
            {
                Tmp = ScrLo[i];
                ScrLo[i] = ScrHi[i];
                ScrHi[i] = Tmp;
            }
        }

        if(!IsHires)
        {
            if ((ScrHi[i] != ScrHi[i - 1]) && (ColRAM[i] != ColRAM[i - 1]) && (ScrHi[i] != ScrHi[i + 1]) && (ColRAM[i] != ColRAM[i + 1]))
            {
                if ((ScrHi[i] == ColRAM[i - 1]) || (ColRAM[i] == ScrHi[i - 1]))
                {
                    Tmp = ColRAM[i];
                    ColRAM[i] = ScrHi[i];
                    ScrHi[i] = Tmp;
                }
            }

            if ((ScrLo[i] != ScrLo[i - 1]) && (ColRAM[i] != ColRAM[i - 1]) && (ScrLo[i] != ScrLo[i + 1]) && (ColRAM[i] != ColRAM[i + 1]))
            {
                if ((ScrLo[i] == ColRAM[i - 1]) || (ColRAM[i] == ScrLo[i - 1]))
                {
                    Tmp = ColRAM[i];
                    ColRAM[i] = ScrLo[i];
                    ScrLo[i] = Tmp;
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void FixOverlaps()
{
    //WriteBinaryFile(OutFile + "_H.bin", ScrHi, ColTabSize);
    //WriteBinaryFile(OutFile + "_L.bin", ScrLo, ColTabSize);
    //WriteBinaryFile(OutFile + "_C.bin", ColRAM, ColTabSize);

    unsigned char OverlapColor = 255;
    int OverlapFirstIdx = -1;
    int OverlapLastIdx = -1;

    int i = 0;

    while (i < ColTabSize)
    {
        if (ScrHi[i] == ScrLo[i])
        {
            OverlapColor = ScrHi[i];
            OverlapFirstIdx = i;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrLo[j] == OverlapColor && ScrHi[j] == OverlapColor)
                {
                    OverlapLastIdx = j;
                }
                else
                {
                    break;
                }
            }

            int HiFirstIdx = OverlapFirstIdx;
            int HiLastIdx = OverlapLastIdx;
            int LoFirstIdx = OverlapFirstIdx;
            int LoLastIdx = OverlapLastIdx;

            for (int j = 1; j <= OverlapFirstIdx; j++)
            {
                if (ScrHi[OverlapFirstIdx - j] == OverlapColor)
                {
                    HiFirstIdx--;
                }
                else
                {
                    break;
                }
            }

            for (int j = 1; j <= OverlapFirstIdx; j++)
            {
                if (ScrLo[OverlapFirstIdx - j] == OverlapColor)
                {
                    LoFirstIdx--;
                }
                else
                {
                    break;
                }
            }

            for (int j = OverlapLastIdx + 1; j < ColTabSize; j++)
            {
                if (ScrHi[j] == OverlapColor)
                {
                    HiLastIdx++;
                }
                else
                {
                    break;
                }
            }

            for (int j = OverlapLastIdx + 1; j < ColTabSize; j++)
            {
                if (ScrLo[j] == OverlapColor)
                {
                    LoLastIdx++;
                }
                else
                {
                    break;
                }
            }

            int OverlapHiLen = HiLastIdx - HiFirstIdx + 1;
            int OverlapLoLen = LoLastIdx - LoFirstIdx + 1;

            unsigned char ColorBefore = OverlapColor;
            unsigned char ColorAfter = OverlapColor;

            if (OverlapHiLen >= OverlapLoLen)
            {
                if (OverlapFirstIdx > 0)
                {
                    ColorBefore = ScrLo[OverlapFirstIdx - 1];
                }
                if (OverlapLastIdx < ColTabSize - 1)
                {
                    ColorAfter = ScrLo[OverlapLastIdx + 1];
                }

                unsigned char ReplaceColor = 255;

                if (ColorBefore != OverlapColor)
                {
                    ReplaceColor = ColorBefore;
                }
                else
                {
                    ReplaceColor = ColorAfter;
                }

                if (ReplaceColor != OverlapColor)
                {
                    for (int j = OverlapFirstIdx; j <= OverlapLastIdx; j++)
                    {
                        ScrLo[j] = ReplaceColor;
                    }
                }
            }
            else
            {
                if (OverlapFirstIdx > 0)
                {
                    ColorBefore = ScrHi[OverlapFirstIdx - 1];
                }
                if (OverlapLastIdx < ColTabSize - 1)
                {
                    ColorAfter = ScrHi[OverlapLastIdx + 1];
                }

                unsigned char ReplaceColor = 255;

                if (ColorBefore != OverlapColor)
                {
                    ReplaceColor = ColorBefore;
                }
                else
                {
                    ReplaceColor = ColorAfter;
                }

                if (ReplaceColor != OverlapColor)
                {
                    for (int j = OverlapFirstIdx; j <= OverlapLastIdx; j++)
                    {
                        ScrHi[j] = ReplaceColor;
                    }
                }
            }

            i = OverlapLastIdx;
        }
        i++;
    }

    if (!IsHires)
    {
        OverlapColor = 255;
        OverlapFirstIdx = -1;
        OverlapLastIdx = -1;

        i = 0;

        while (i < ColTabSize)
        {
            if (ScrHi[i] == ColRAM[i])
            {
                OverlapColor = ScrHi[i];
                OverlapFirstIdx = i;

                for (int j = i; j < ColTabSize; j++)
                {
                    if (ColRAM[j] == OverlapColor && ScrHi[j] == OverlapColor)
                    {
                        OverlapLastIdx = j;
                    }
                    else
                    {
                        break;
                    }
                }

                int HiFirstIdx = OverlapFirstIdx;
                int HiLastIdx = OverlapLastIdx;
                int CRFirstIdx = OverlapFirstIdx;
                int CRLastIdx = OverlapLastIdx;

                for (int j = 1; j <= OverlapFirstIdx; j++)
                {
                    if (ScrHi[OverlapFirstIdx - j] == OverlapColor)
                    {
                        HiFirstIdx--;
                    }
                    else
                    {
                        break;
                    }
                }

                for (int j = 1; j <= OverlapFirstIdx; j++)
                {
                    if (ColRAM[OverlapFirstIdx - j] == OverlapColor)
                    {
                        CRFirstIdx--;
                    }
                    else
                    {
                        break;
                    }
                }

                for (int j = OverlapLastIdx + 1; j < ColTabSize; j++)
                {
                    if (ScrHi[j] == OverlapColor)
                    {
                        HiLastIdx++;
                    }
                    else
                    {
                        break;
                    }
                }

                for (int j = OverlapLastIdx + 1; j < ColTabSize; j++)
                {
                    if (ColRAM[j] == OverlapColor)
                    {
                        CRLastIdx++;
                    }
                    else
                    {
                        break;
                    }
                }

                int OverlapHiLen = HiLastIdx - HiFirstIdx + 1;
                int OverlapCRLen = CRLastIdx - CRFirstIdx + 1;

                unsigned char ColorBefore = OverlapColor;
                unsigned char ColorAfter = OverlapColor;

                if (OverlapHiLen >= OverlapCRLen)
                {
                    if (OverlapFirstIdx > 0)
                    {
                        ColorBefore = ColRAM[OverlapFirstIdx - 1];
                    }
                    if (OverlapLastIdx < ColTabSize - 1)
                    {
                        ColorAfter = ColRAM[OverlapLastIdx + 1];
                    }

                    unsigned char ReplaceColor = 255;

                    if (ColorBefore != OverlapColor)
                    {
                        ReplaceColor = ColorBefore;
                    }
                    else
                    {
                        ReplaceColor = ColorAfter;
                    }

                    if (ReplaceColor != OverlapColor)
                    {
                        for (int j = OverlapFirstIdx; j <= OverlapLastIdx; j++)
                        {
                            ColRAM[j] = ReplaceColor;
                        }
                    }
                }
                else
                {
                    if (OverlapFirstIdx > 0)
                    {
                        ColorBefore = ScrHi[OverlapFirstIdx - 1];
                    }
                    if (OverlapLastIdx < ColTabSize - 1)
                    {
                        ColorAfter = ScrHi[OverlapLastIdx + 1];
                    }

                    unsigned char ReplaceColor = 255;

                    if (ColorBefore != OverlapColor)
                    {
                        ReplaceColor = ColorBefore;
                    }
                    else
                    {
                        ReplaceColor = ColorAfter;
                    }

                    if (ReplaceColor != OverlapColor)
                    {
                        for (int j = OverlapFirstIdx; j <= OverlapLastIdx; j++)
                        {
                            ScrHi[j] = ReplaceColor;
                        }
                    }
                }
                i = OverlapLastIdx;
            }
            i++;
        }

        OverlapColor = 255;
        OverlapFirstIdx = -1;
        OverlapLastIdx = -1;

        i = 0;


        while (i < ColTabSize)
        {
            if (ScrLo[i] == ColRAM[i])
            {
                OverlapColor = ScrLo[i];
                OverlapFirstIdx = i;

                for (int j = i; j < ColTabSize; j++)
                {
                    if (ColRAM[j] == OverlapColor && ScrLo[j] == OverlapColor)
                    {
                        OverlapLastIdx = j;
                    }
                    else
                    {
                        break;
                    }
                }

                int LoFirstIdx = OverlapFirstIdx;
                int LoLastIdx = OverlapLastIdx;
                int CRFirstIdx = OverlapFirstIdx;
                int CRLastIdx = OverlapLastIdx;

                for (int j = 1; j <= OverlapFirstIdx; j++)
                {
                    if (ScrLo[OverlapFirstIdx - j] == OverlapColor)
                    {
                        LoFirstIdx--;
                    }
                    else
                    {
                        break;
                    }
                }

                for (int j = 1; j <= OverlapFirstIdx; j++)
                {
                    if (ColRAM[OverlapFirstIdx - j] == OverlapColor)
                    {
                        CRFirstIdx--;
                    }
                    else
                    {
                        break;
                    }
                }

                for (int j = OverlapLastIdx + 1; j < ColTabSize; j++)
                {
                    if (ScrLo[j] == OverlapColor)
                    {
                        LoLastIdx++;
                    }
                    else
                    {
                        break;
                    }
                }

                for (int j = OverlapLastIdx + 1; j < ColTabSize; j++)
                {
                    if (ColRAM[j] == OverlapColor)
                    {
                        CRLastIdx++;
                    }
                    else
                    {
                        break;
                    }
                }

                int OverlapLoLen = LoLastIdx - LoFirstIdx + 1;
                int OverlapCRLen = CRLastIdx - CRFirstIdx + 1;

                unsigned char ColorBefore = OverlapColor;
                unsigned char ColorAfter = OverlapColor;

                if (OverlapLoLen >= OverlapCRLen)
                {
                    if (OverlapFirstIdx > 0)
                    {
                        ColorBefore = ColRAM[OverlapFirstIdx - 1];
                    }
                    if (OverlapLastIdx < ColTabSize - 1)
                    {
                        ColorAfter = ColRAM[OverlapLastIdx + 1];
                    }

                    unsigned char ReplaceColor = 255;

                    if (ColorBefore != OverlapColor)
                    {
                        ReplaceColor = ColorBefore;
                    }
                    else
                    {
                        ReplaceColor = ColorAfter;
                    }

                    if (ReplaceColor != OverlapColor)
                    {
                        for (int j = OverlapFirstIdx; j <= OverlapLastIdx; j++)
                        {
                            ColRAM[j] = ReplaceColor;
                        }
                    }
                }
                else
                {
                    if (OverlapFirstIdx > 0)
                    {
                        ColorBefore = ScrLo[OverlapFirstIdx - 1];
                    }
                    if (OverlapLastIdx < ColTabSize - 1)
                    {
                        ColorAfter = ScrLo[OverlapLastIdx + 1];
                    }

                    unsigned char ReplaceColor = 255;

                    if (ColorBefore != OverlapColor)
                    {
                        ReplaceColor = ColorBefore;
                    }
                    else
                    {
                        ReplaceColor = ColorAfter;
                    }

                    if (ReplaceColor != OverlapColor)
                    {
                        for (int j = OverlapFirstIdx; j <= OverlapLastIdx; j++)
                        {
                            ScrLo[j] = ReplaceColor;
                        }
                    }
                }
                i = OverlapLastIdx;
            }
            i++;
        }
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
/*
void CorrectOverlaps()
{

    //Find color overlaps (same color in different color spaces) and eliminate them by extending previous or following sequence (whichever is longer)

    for (int i = 1; i < ColTabSize - 1; i++)
    {

        int LastMatch = i;

        if (ScrHi[i] == ScrLo[i])
        {
            unsigned char HiBefore = ScrHi[i - 1];
            unsigned char LoBefore = ScrLo[i - 1];
            unsigned char HiAfter = ScrHi[i - 1];
            unsigned char LoAfter = ScrLo[i - 1];

            int SeqBefore1 = 0;
            int SeqBefore2 = 0;
            int SeqAfter1 = 0;
            int SeqAfter2 = 0;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrHi[j] == ScrLo[j])
                {
                    LastMatch = j;
                }
                else
                {
                    HiAfter = ScrHi[j];
                    LoAfter = ScrLo[j];
                    break;
                }
            }

            for (int j = i - 1; j >= 0; j--)
            {
                if (ScrHi[j] == ScrHi[i - 1])
                {
                    SeqBefore1++;
                }
                else
                {
                    break;
                }
            }
            for (int j = i - 1; j >= 0; j--)
            {
                if (ScrLo[j] == ScrLo[i - 1])
                {
                    SeqBefore2++;
                }
                else
                {
                    break;
                }
            }
            for (int j = LastMatch + 1; j < ColTabSize; j++)
            {
                if (ScrHi[j] == ScrHi[LastMatch + 1])
                {
                    SeqAfter1++;
                }
                else
                {
                    break;
                }
            }
            for (int j = LastMatch + 1; j < ColTabSize; j++)
            {
                if (ScrLo[j] == ScrLo[LastMatch + 1])
                {
                    SeqAfter2++;
                }
                else
                {
                    break;
                }
            }

            if ((SeqBefore1 <= SeqBefore2) && (SeqBefore1 <= SeqAfter1) && (SeqBefore1 <= SeqAfter2))
            {
                if (HiBefore != ScrHi[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrHi[j] = HiBefore;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrHi[j] = HiAfter;
                    }
                }
            }
            else if ((SeqBefore2 <= SeqBefore1) && (SeqBefore2 <= SeqAfter1) && (SeqBefore2 <= SeqAfter2))
            {
                if (LoBefore != ScrLo[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrLo[j] = LoBefore;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrLo[j] = LoAfter;
                    }
                }
            }
            else if ((SeqAfter1 <= SeqBefore1) && (SeqAfter1 <= SeqBefore2) && (SeqAfter1 <= SeqAfter2))
            {
                if (HiAfter != ScrHi[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrHi[j] = HiAfter;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrHi[j] = HiBefore;
                    }
                }
            }
            else
            {
                if (LoAfter != ScrLo[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrLo[j] = LoAfter;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrLo[j] = LoBefore;
                    }
                }
            }
        }

        if (ScrHi[i] == ColRAM[i])
        {
            unsigned char HiBefore = ScrHi[i - 1];
            unsigned char CRBefore = ColRAM[i - 1];
            unsigned char HiAfter = ScrHi[i - 1];
            unsigned char CRAfter = ColRAM[i - 1];

            int SeqBefore1 = 0;
            int SeqBefore2 = 0;
            int SeqAfter1 = 0;
            int SeqAfter2 = 0;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrHi[j] == ColRAM[j])
                {
                    LastMatch = j;
                }
                else
                {
                    HiAfter = ScrHi[j];
                    CRAfter = ColRAM[j];
                    break;
                }
            }

            for (int j = i - 1; j >= 0; j--)
            {
                if (ScrHi[j] == ScrHi[i - 1])
                {
                    SeqBefore1++;
                }
                else
                {
                    break;
                }
            }
            for (int j = i - 1; j >= 0; j--)
            {
                if (ColRAM[j] == ColRAM[i - 1])
                {
                    SeqBefore2++;
                }
                else
                {
                    break;
                }
            }
            for (int j = LastMatch + 1; j < ColTabSize; j++)
            {
                if (ScrHi[j] == ScrHi[LastMatch + 1])
                {
                    SeqAfter1++;
                }
                else
                {
                    break;
                }
            }
            for (int j = LastMatch + 1; j < ColTabSize; j++)
            {
                if (ColRAM[j] == ColRAM[LastMatch + 1])
                {
                    SeqAfter2++;
                }
                else
                {
                    break;
                }
            }

            if ((SeqBefore1 >= SeqBefore2) && (SeqBefore1 >= SeqAfter1) && (SeqBefore1 >= SeqAfter2))
            {
                if (HiBefore != ScrHi[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrHi[j] = HiBefore;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrHi[j] = HiAfter;
                    }
                }
            }
            else if ((SeqBefore2 >= SeqBefore1) && (SeqBefore2 >= SeqAfter1) && (SeqBefore2 >= SeqAfter2))
            {
                if (CRBefore != ColRAM[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ColRAM[j] = CRBefore;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ColRAM[j] = CRAfter;
                    }
                }
            }
            else if ((SeqAfter1 >= SeqBefore1) && (SeqAfter1 >= SeqBefore2) && (SeqAfter1 >= SeqAfter2))
            {
                if (HiAfter != ScrHi[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrHi[j] = HiAfter;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrHi[j] = HiBefore;
                    }
                }
            }
            else
            {
                if (CRAfter != ColRAM[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ColRAM[j] = CRAfter;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ColRAM[j] = CRBefore;
                    }
                }
            }
        }

        if (ScrLo[i] == ColRAM[i])
        {
            unsigned char LoBefore = ScrLo[i - 1];
            unsigned char CRBefore = ColRAM[i - 1];
            unsigned char LoAfter = ScrLo[i - 1];
            unsigned char CRAfter = ColRAM[i - 1];

            int SeqBefore1 = 0;
            int SeqBefore2 = 0;
            int SeqAfter1 = 0;
            int SeqAfter2 = 0;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrLo[j] == ColRAM[j])
                {
                    LastMatch = j;
                }
                else
                {
                    LoAfter = ScrLo[j];
                    CRAfter = ColRAM[j];
                    break;
                }
            }

            for (int j = i - 1; j >= 0; j--)
            {
                if (ScrLo[j] == ScrLo[i - 1])
                {
                    SeqBefore1++;
                }
                else
                {
                    break;
                }
            }
            for (int j = i - 1; j >= 0; j--)
            {
                if (ColRAM[j] == ColRAM[i - 1])
                {
                    SeqBefore2++;
                }
                else
                {
                    break;
                }
            }
            for (int j = LastMatch + 1; j < ColTabSize; j++)
            {
                if (ScrLo[j] == ScrLo[LastMatch + 1])
                {
                    SeqAfter1++;
                }
                else
                {
                    break;
                }
            }
            for (int j = LastMatch + 1; j < ColTabSize; j++)
            {
                if (ColRAM[j] == ColRAM[LastMatch + 1])
                {
                    SeqAfter2++;
                }
                else
                {
                    break;
                }
            }

            if ((SeqBefore1 >= SeqBefore2) && (SeqBefore1 >= SeqAfter1) && (SeqBefore1 >= SeqAfter2))
            {
                if (LoBefore != ScrLo[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrLo[j] = LoBefore;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrLo[j] = LoAfter;
                    }
                }
            }
            else if ((SeqBefore2 >= SeqBefore1) && (SeqBefore2 >= SeqAfter1) && (SeqBefore2 >= SeqAfter2))
            {
                if (CRBefore != ColRAM[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ColRAM[j] = CRBefore;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ColRAM[j] = CRAfter;
                    }
                }
            }
            else if ((SeqAfter1 >= SeqBefore1) && (SeqAfter1 >= SeqBefore2) && (SeqAfter1 >= SeqAfter2))
            {
                if (LoAfter != ScrLo[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrLo[j] = LoAfter;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ScrLo[j] = LoBefore;
                    }
                }
            }
            else
            {
                if (CRAfter != ColRAM[i])
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ColRAM[j] = CRAfter;
                    }
                }
                else
                {
                    for (int j = i; j <= LastMatch; j++)
                    {
                        ColRAM[j] = CRBefore;
                    }
                }
            }
        }
    }
}
*/
//----------------------------------------------------------------------------------------------------------------------------------------------------------
///*
struct unusedblock {
    int SeqLenBefore;
    int NumSeqBefore;
    unsigned char SeqColorBefore;
    int UnusedIdx;
    int UnusedLength;
    int SeqLenAfter;
    int NumSeqAfter;
    unsigned char SeqColorAfter;
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void DistributeUnusedBlocks()
{
    //WriteBinaryFile(OutFile + "_H.bin", ScrHi, ColTabSize);
    //WriteBinaryFile(OutFile + "_L.bin", ScrLo, ColTabSize);
    //WriteBinaryFile(OutFile + "_C.bin", ColRAM, ColTabSize);

    vector<unusedblock>UnusedBlocks;
    unusedblock UnusedTmp{};

    int i = 0;

    while (i < ColTabSize)
    {
        if (ScrHi[i] != 255)
        {
            int SeqLen = 0;
            int SeqLenLast = 0;
            unsigned char SeqColor = ScrHi[i];
            int NumCol = 1;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrHi[j] != 255)
                {
                    SeqLen++;
                    if (ScrHi[j] != SeqColor)
                    {
                        SeqColor = ScrHi[j];
                        NumCol++;
                        SeqLenLast = 0;
                    }
                    SeqLenLast++;
                }
                else
                {
                    break;
                }
            }
            UnusedTmp.NumSeqBefore = NumCol;
            UnusedTmp.SeqLenBefore = SeqLenLast;
            UnusedTmp.SeqColorBefore = SeqColor;

            i += SeqLen;
        }

        if (ScrHi[i] == 255)
        {
            UnusedTmp.UnusedLength = 0;
            UnusedTmp.UnusedIdx = i;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrHi[j] == 255)
                {
                    UnusedTmp.UnusedLength++;
                }
                else
                {
                    break;
                }
            }
        }

        i += UnusedTmp.UnusedLength;
        int SeqLen = 0;
        int SeqLenFirst = 0;
        int SeqLenLast = 0;
        int NumCol = 1;
        unsigned char SeqColor = ScrHi[i];
        unsigned char SeqColorFirst = ScrHi[i];

        if (i < ColTabSize)
        {
            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrHi[j] != 255)
                {
                    SeqLen++;
                    if (ScrHi[j] != SeqColor)
                    {
                        SeqColor = ScrHi[j];
                        NumCol++;
                        SeqLenLast = 0;
                    }
                    if (NumCol == 1)
                    {
                        SeqLenFirst++;
                    }
                    SeqLenLast++;
                }
                else
                {
                    break;
                }
            }

            i += SeqLen - 1;

            UnusedTmp.NumSeqAfter = NumCol;
            UnusedTmp.SeqLenAfter = SeqLenFirst;
            UnusedTmp.SeqColorAfter = SeqColorFirst;

            UnusedBlocks.push_back(UnusedTmp);

            UnusedTmp.NumSeqBefore = UnusedTmp.NumSeqAfter;
            UnusedTmp.SeqColorBefore = SeqColor;
            UnusedTmp.SeqLenBefore = SeqLenLast;
        }
        else
        {
            UnusedTmp.NumSeqAfter = UnusedTmp.NumSeqBefore;
            UnusedTmp.SeqLenAfter = UnusedTmp.SeqLenBefore;
            UnusedTmp.SeqColorAfter = UnusedTmp.SeqColorBefore;

            UnusedBlocks.push_back(UnusedTmp);
            }

        i++;
    }

    for (size_t i = 0; i < UnusedBlocks.size(); i++)
    {
        if ((UnusedBlocks[i].SeqColorBefore == UnusedBlocks[i].SeqColorAfter) || (UnusedBlocks[i].SeqLenBefore >= UnusedBlocks[i].SeqLenAfter))
        {
            unsigned char TmpCol = UnusedBlocks[i].SeqColorBefore;
            for (int j = UnusedBlocks[i].UnusedIdx; j < UnusedBlocks[i].UnusedIdx + UnusedBlocks[i].UnusedLength; j++)
            {
                ScrHi[j] = TmpCol;
            }
        }
        else
        {
            unsigned char TmpCol = UnusedBlocks[i].SeqColorAfter;
            for (int j = UnusedBlocks[i].UnusedIdx; j < UnusedBlocks[i].UnusedIdx + UnusedBlocks[i].UnusedLength; j++)
            {
                ScrHi[j] = TmpCol;
            }
        }
    }

    UnusedBlocks.clear();

    UnusedTmp.NumSeqAfter = 0;
    UnusedTmp.NumSeqBefore = 0;
    UnusedTmp.SeqColorAfter = 255;
    UnusedTmp.SeqColorBefore = 255;
    UnusedTmp.SeqLenAfter = 0;
    UnusedTmp.SeqLenBefore = 0;
    UnusedTmp.UnusedIdx = 0;
    UnusedTmp.UnusedLength = 0;

    i = 0;

    while (i < ColTabSize)
    {
        if (ScrLo[i] != 255)
        {
            int SeqLen = 0;
            int SeqLenLast = 0;
            unsigned char SeqColor = ScrLo[i];
            int NumCol = 1;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrLo[j] != 255)
                {
                    SeqLen++;
                    if (ScrLo[j] != SeqColor)
                    {
                        SeqColor = ScrLo[j];
                        NumCol++;
                        SeqLenLast = 0;
                    }
                    SeqLenLast++;
                }
                else
                {
                    break;
                }
            }
            UnusedTmp.NumSeqBefore = NumCol;
            UnusedTmp.SeqLenBefore = SeqLenLast;
            UnusedTmp.SeqColorBefore = SeqColor;

            i += SeqLen;
        }

        if (ScrLo[i] == 255)
        {
            UnusedTmp.UnusedLength = 0;
            UnusedTmp.UnusedIdx = i;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrLo[j] == 255)
                {
                    UnusedTmp.UnusedLength++;
                }
                else
                {
                    break;
                }
            }
        }

        i += UnusedTmp.UnusedLength;

        if (i < ColTabSize)
        {

            int SeqLen = 0;
            int SeqLenFirst = 0;
            int SeqLenLast = 0;
            int NumCol = 1;
            unsigned char SeqColor = ScrLo[i];
            unsigned char SeqColorFirst = ScrLo[i];

            for (int j = i; j < ColTabSize; j++)
            {
                if (ScrLo[j] != 255)
                {
                    SeqLen++;
                    if (ScrLo[j] != SeqColor)
                    {
                        SeqColor = ScrLo[j];
                        NumCol++;
                        SeqLenLast = 0;
                    }
                    if (NumCol == 1)
                    {
                        SeqLenFirst++;
                    }
                    SeqLenLast++;
                }
                else
                {
                    break;
                }
            }

            i += SeqLen - 1;

            UnusedTmp.NumSeqAfter = NumCol;
            UnusedTmp.SeqLenAfter = SeqLenFirst;
            UnusedTmp.SeqColorAfter = SeqColorFirst;

            UnusedBlocks.push_back(UnusedTmp);

            UnusedTmp.NumSeqBefore = UnusedTmp.NumSeqAfter;
            UnusedTmp.SeqColorBefore = SeqColor;
            UnusedTmp.SeqLenBefore = SeqLenLast;

        }
        else
        {
            UnusedTmp.NumSeqAfter = UnusedTmp.NumSeqBefore;
            UnusedTmp.SeqLenAfter = UnusedTmp.SeqLenBefore;
            UnusedTmp.SeqColorAfter = UnusedTmp.SeqColorBefore;

            UnusedBlocks.push_back(UnusedTmp);
        }

        i++;
    }

    for (size_t i = 0; i < UnusedBlocks.size(); i++)
    {
        if ((UnusedBlocks[i].SeqColorBefore == UnusedBlocks[i].SeqColorAfter) || (UnusedBlocks[i].SeqLenBefore >= UnusedBlocks[i].SeqLenAfter))
        {
            unsigned char TmpCol = UnusedBlocks[i].SeqColorBefore;
            for (int j = UnusedBlocks[i].UnusedIdx; j < UnusedBlocks[i].UnusedIdx + UnusedBlocks[i].UnusedLength; j++)
            {
                ScrLo[j] = TmpCol;
            }
        }
        else
        {
            unsigned char TmpCol = UnusedBlocks[i].SeqColorAfter;
            for (int j = UnusedBlocks[i].UnusedIdx; j < UnusedBlocks[i].UnusedIdx + UnusedBlocks[i].UnusedLength; j++)
            {
                ScrLo[j] = TmpCol;
            }
        }
    }

    UnusedBlocks.clear();

    UnusedTmp.NumSeqAfter = 0;
    UnusedTmp.NumSeqBefore = 0;
    UnusedTmp.SeqColorAfter = 255;
    UnusedTmp.SeqColorBefore = 255;
    UnusedTmp.SeqLenAfter = 0;
    UnusedTmp.SeqLenBefore = 0;
    UnusedTmp.UnusedIdx = 0;
    UnusedTmp.UnusedLength = 0;

    i = 0;

    while (i < ColTabSize)
    {
        if (ColRAM[i] != 255)
        {
            int SeqLen = 0;
            int SeqLenLast = 0;
            unsigned char SeqColor = ColRAM[i];
            int NumCol = 1;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ColRAM[j] != 255)
                {
                    SeqLen++;
                    if (ColRAM[j] != SeqColor)
                    {
                        SeqColor = ColRAM[j];
                        NumCol++;
                        SeqLenLast = 0;
                    }
                    SeqLenLast++;
                }
                else
                {
                    break;
                }
            }
            UnusedTmp.NumSeqBefore = NumCol;
            UnusedTmp.SeqLenBefore = SeqLenLast;
            UnusedTmp.SeqColorBefore = SeqColor;

            i += SeqLen;
        }

        if (ColRAM[i] == 255)
        {
            UnusedTmp.UnusedLength = 0;
            UnusedTmp.UnusedIdx = i;

            for (int j = i; j < ColTabSize; j++)
            {
                if (ColRAM[j] == 255)
                {
                    UnusedTmp.UnusedLength++;
                }
                else
                {
                    break;
                }
            }
        }

        i += UnusedTmp.UnusedLength;
        int SeqLen = 0;
        int SeqLenFirst = 0;
        int SeqLenLast = 0;
        int NumCol = 1;
        unsigned char SeqColor = ColRAM[i];
        unsigned char SeqColorFirst = ColRAM[i];

        if (i < ColTabSize)
        {
            for (int j = i; j < ColTabSize; j++)
            {
                if (ColRAM[j] != 255)
                {
                    SeqLen++;
                    if (ColRAM[j] != SeqColor)
                    {
                        SeqColor = ColRAM[j];
                        NumCol++;
                        SeqLenLast = 0;
                    }
                    if (NumCol == 1)
                    {
                        SeqLenFirst++;
                    }
                    SeqLenLast++;
                }
                else
                {
                    break;
                }
            }

            i += SeqLen - 1;

            UnusedTmp.NumSeqAfter = NumCol;
            UnusedTmp.SeqLenAfter = SeqLenFirst;
            UnusedTmp.SeqColorAfter = SeqColorFirst;

            UnusedBlocks.push_back(UnusedTmp);

            UnusedTmp.NumSeqBefore = UnusedTmp.NumSeqAfter;
            UnusedTmp.SeqColorBefore = SeqColor;
            UnusedTmp.SeqLenBefore = SeqLenLast;
        }
        else
        {
            UnusedTmp.NumSeqAfter = UnusedTmp.NumSeqBefore;
            UnusedTmp.SeqLenAfter = UnusedTmp.SeqLenBefore;
            UnusedTmp.SeqColorAfter = UnusedTmp.SeqColorBefore;

            UnusedBlocks.push_back(UnusedTmp);
        }

        i++;
    }

    for (size_t i = 0; i < UnusedBlocks.size(); i++)
    {
        if ((UnusedBlocks[i].SeqColorBefore == UnusedBlocks[i].SeqColorAfter) || (UnusedBlocks[i].SeqLenBefore >= UnusedBlocks[i].SeqLenAfter))
        {
            unsigned char TmpCol = UnusedBlocks[i].SeqColorBefore;
            for (int j = UnusedBlocks[i].UnusedIdx; j < UnusedBlocks[i].UnusedIdx + UnusedBlocks[i].UnusedLength; j++)
            {
                ColRAM[j] = TmpCol;
            }
        }
        else
        {
            unsigned char TmpCol = UnusedBlocks[i].SeqColorAfter;
            for (int j = UnusedBlocks[i].UnusedIdx; j < UnusedBlocks[i].UnusedIdx + UnusedBlocks[i].UnusedLength; j++)
            {
                ColRAM[j] = TmpCol;
            }
        }
    }

    //WriteBinaryFile(OutFile + "_H1.bin", ScrHi, ColTabSize);
    //WriteBinaryFile(OutFile + "_L1.bin", ScrLo, ColTabSize);
    //WriteBinaryFile(OutFile + "_C1.bin", ColRAM, ColTabSize);

}
//*/
//----------------------------------------------------------------------------------------------------------------------------------------------------------

void FillUnusedBlocks()
{
    if (!IsHires && ColRAM[0] == 255)
    {
        ColRAM[0] = 0;      //In case the whole array remained unused

        for (int i = 1; i < ColTabSize; i++)
        {
            if (ColRAM[i] != 255)
            {
                ColRAM[0] = ColRAM[i];
                break;
            }
        }
    }

    if (ScrHi[0] == 255)
    {
        ScrHi[0] = 0;      //In case the whole array remained unused

        for (int i = 1; i < ColTabSize; i++)
        {
            if (ScrHi[i] != 255)
            {
                ScrHi[0] = ScrHi[i];
                break;
            }
        }
    }

    if (ScrLo[0] == 255)
    {
        ScrLo[0] = 0;      //In case the whole array remained unused

        for (int i = 1; i < ColTabSize; i++)
        {
            if (ScrLo[i] != 255)
            {
                ScrLo[0] = ScrLo[i];
                break;
            }
        }
    }

    for (int i = 1; i < ColTabSize; i++)
    {
        if ((ScrHi[i] == 255) || (ScrHi[i] == ScrLo[i]) || ((ScrHi[i] == ColRAM[i]) && (!IsHires)))
        {
            ScrHi[i] = ScrHi[i - 1];
        }
        if ((ScrLo[i] == 255) || (ScrLo[i] == ScrHi[i]) || ((ScrLo[i] == ColRAM[i]) && (!IsHires)))
        {
            ScrLo[i] = ScrLo[i - 1];
        }
        if (!IsHires)
        {
            if ((ColRAM[i] == 255) || (ColRAM[i] == ScrHi[i]) || (ColRAM[i] == ScrLo[i]))
            {
                ColRAM[i] = ColRAM[i - 1];
            }
        }
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void RelocateSingleBlocks()
{
    //Move single blocks if there are adjacent blocks with same color in another color space
    for (int i = 1; i < ColTabSize - 1; i++)
    {
        if ((ScrHi[i] != ScrHi[i - 1]) && (ScrHi[i] != ScrHi[i + 1]) && (ScrHi[i] != 255))
        {
            if (((ScrLo[i] == 255) && (ScrLo[i - 1] == ScrHi[i])) || ((ScrLo[i] == 255) && (ScrLo[i + 1] == ScrHi[i])))
            {
                ScrLo[i] = ScrHi[i];
                ScrHi[i] = 255;
            }
            else if ((!IsHires) && (((ColRAM[i] == 255) && (ColRAM[i - 1] == ScrHi[i])) || ((ColRAM[i] == 255) && (ColRAM[i + 1] == ScrHi[i]))))
            {
                ColRAM[i] = ScrHi[i];
                ScrHi[i] = 255;
            }
        }

        if ((ScrLo[i] != ScrLo[i - 1]) && (ScrLo[i] != ScrLo[i + 1]) && (ScrLo[i] != 255))
        {
            if ((!IsHires) && (((ColRAM[i] == 255) && (ColRAM[i - 1] == ScrLo[i])) || ((ColRAM[i] == 255) && (ColRAM[i + 1] == ScrLo[i]))))
            {
                ColRAM[i] = ScrLo[i];
                ScrLo[i] = 255;
            }
            else if (((ScrHi[i] == 255) && (ScrHi[i - 1] == ScrLo[i])) || ((ScrHi[i] == 255) && (ScrHi[i + 1] == ScrLo[i])))
            {
                ScrHi[i] = ScrLo[i];
                ScrLo[i] = 255;
            }
        }

        if ((!IsHires) && (ColRAM[i] != ColRAM[i - 1]) && (ColRAM[i] != ColRAM[i + 1]) && (ColRAM[i] != 255))
        {
            if (((ScrHi[i] == 255) && (ScrHi[i - 1] == ColRAM[i])) || ((ScrHi[i] == 255) && (ScrHi[i + 1] == ColRAM[i])))
            {
                ScrHi[i] = ColRAM[i];
                ColRAM[i] = 255;
            }
            else if (((ScrLo[i] == 255) && (ScrLo[i - 1] == ColRAM[i])) || ((ScrLo[i] == 255) && (ScrLo[i + 1] == ColRAM[i])))
            {
                ScrLo[i] = ColRAM[i];
                ColRAM[i] = 255;
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void ConnectMatchingSegments()
{
	unsigned char CurrentColor = 255; // Start with an invalid color
	int SeqStart = INT_MAX;           // Start with an invalid index
	int SeqEnd = INT_MIN;             // Start with an invalid index

    for (int i = 0; i < ColTabSize - 1; i++)
    {
        if (ScrHi[i] != CurrentColor)
        {
			if (CurrentColor != 255 && SeqStart <= SeqEnd)
			{
                if (ScrLo[SeqStart - 1] == CurrentColor && ScrLo[SeqEnd + 1] == CurrentColor)
                {
					unsigned char NewColor = ScrLo[SeqStart - 1]; // Use the color from ScrLo
                    int NumSeq = 0;
                    for (int j = SeqStart; j <= SeqEnd; j++)
                    {
                        if (ScrLo[j] != NewColor)
                        {
                            NumSeq++;
							NewColor = ScrLo[j]; // Update to the next color
                        }
                    }

                    if (NumSeq > 1)
                    {
                        for (int j = SeqStart; j <= SeqEnd; j++)
                        {
                            ScrHi[j] = ScrLo[j];
                            ScrLo[j] = CurrentColor;
                        }
                    }
                }
                else if (ColRAM[SeqStart - 1] == CurrentColor && ColRAM[SeqEnd + 1] == CurrentColor)
                {
                    unsigned char NewColor = ColRAM[SeqStart - 1]; // Use the color from ScrLo
                    int NumSeq = 0;
                    for (int j = SeqStart; j <= SeqEnd; j++)
                    {
                        if (ColRAM[j] != NewColor)
                        {
                            NumSeq++;
                            NewColor = ColRAM[j]; // Update to the next color
                        }
                    }

                    if (NumSeq > 1)
                    {
                        for (int j = SeqStart; j <= SeqEnd; j++)
                        {
                            ScrHi[j] = ColRAM[j];
                            ColRAM[j] = CurrentColor;
                        }
                    }
                }
            }
			// If we encounter a new color, update the current color and segment indices                                            
            CurrentColor = ScrHi[i]; // Update current color
			SeqStart = i;            // Start a new segment
			SeqEnd = i;              // Initialize end of the segment
		}
		else
		{
			SeqEnd = i; // Extend the end of the current segment
        }
    }

    CurrentColor = 255; // Start with an invalid color
    SeqStart = INT_MAX;           // Start with an invalid index
    SeqEnd = INT_MIN;             // Start with an invalid index

    for (int i = 0; i < ColTabSize - 1; i++)
    {
        if (ScrLo[i] != CurrentColor)
        {
            if (CurrentColor != 255 && SeqStart <= SeqEnd)
            {
                if (ScrHi[SeqStart - 1] == CurrentColor && ScrHi[SeqEnd + 1] == CurrentColor)
                {
                    unsigned char NewColor = ScrHi[SeqStart - 1]; // Use the color from ScrLo
                    int NumSeq = 0;
                    for (int j = SeqStart; j <= SeqEnd; j++)
                    {
                        if (ScrHi[j] != NewColor)
                        {
                            NumSeq++;
                            NewColor = ScrHi[j]; // Update to the next color
                        }
                    }

                    if (NumSeq > 1)
                    {
                        int Num1 = 0;
                        int Num2 = 0;
						for (int j = 0; j < ColTabSize; j++)
						{
							if (ScrHi[j] == CurrentColor)
							{
								Num1++;
							}
							if (ScrLo[j] == CurrentColor)
							{
								Num2++;
							}
						}

                        if (Num1 >= Num2)
                        {
                            for (int j = SeqStart; j <= SeqEnd; j++)
                            {
                                ScrLo[j] = ScrHi[j];
                                ScrHi[j] = CurrentColor;
                            }
                        }

                    }
                }
                else if (ColRAM[SeqStart - 1] == CurrentColor && ColRAM[SeqEnd + 1] == CurrentColor)
                {
                    unsigned char NewColor = ColRAM[SeqStart - 1]; // Use the color from ScrLo
                    int NumSeq = 0;
                    for (int j = SeqStart; j <= SeqEnd; j++)
                    {
                        if (ColRAM[j] != NewColor)
                        {
                            NumSeq++;
                            NewColor = ColRAM[j]; // Update to the next color
                        }
                    }

                    if (NumSeq > 1)
                    {
                        for (int j = SeqStart; j <= SeqEnd; j++)
                        {
                            ScrLo[j] = ColRAM[j];
                            ColRAM[j] = CurrentColor;
                        }
                    }
                }
            }
            // If we encounter a new color, update the current color and segment indices                                            
            CurrentColor = ScrHi[i]; // Update current color
            SeqStart = i;            // Start a new segment
            SeqEnd = i;              // Initialize end of the segment
        }
        else
        {
            SeqEnd = i; // Extend the end of the current segment
        }
    }

    CurrentColor = 255; // Start with an invalid color
    SeqStart = INT_MAX;           // Start with an invalid index
    SeqEnd = INT_MIN;             // Start with an invalid index

    for (int i = 0; i < ColTabSize - 1; i++)
    {
        if (ColRAM[i] != CurrentColor)
        {
            if (CurrentColor != 255 && SeqStart <= SeqEnd)
            {
                if (ScrHi[SeqStart - 1] == CurrentColor && ScrHi[SeqEnd + 1] == CurrentColor)
                {
                    unsigned char NewColor = ScrHi[SeqStart - 1]; // Use the color from ScrLo
                    int NumSeq = 0;
                    for (int j = SeqStart; j <= SeqEnd; j++)
                    {
                        if (ScrHi[j] != NewColor)
                        {
                            NumSeq++;
                            NewColor = ScrHi[j]; // Update to the next color
                        }
                    }

                    if (NumSeq > 1)
                    {
                        int Num1 = 0;
                        int Num2 = 0;
                        for (int j = 0; j < ColTabSize; j++)
                        {
                            if (ScrHi[j] == CurrentColor)
                            {
                                Num1++;
                            }
                            if (ColRAM[j] == CurrentColor)
                            {
                                Num2++;
                            }
                        }


                        if (Num1 >= Num2)
						{
							for (int j = SeqStart; j <= SeqEnd; j++)
							{
								ColRAM[j] = ScrHi[j];
								ScrHi[j] = CurrentColor;
							}
						}
                    }
                }
                else if (ScrLo[SeqStart - 1] == CurrentColor && ScrLo[SeqEnd + 1] == CurrentColor)
                {
                    unsigned char NewColor = ScrLo[SeqStart - 1]; // Use the color from ScrLo
                    int NumSeq = 0;
                    for (int j = SeqStart; j <= SeqEnd; j++)
                    {
                        if (ScrLo[j] != NewColor)
                        {
                            NumSeq++;
                            NewColor = ScrLo[j]; // Update to the next color
                        }
                    }

                    if (NumSeq > 1)
                    {
                        int Num1 = 0;
                        int Num2 = 0;
                        for (int j = 0; j < ColTabSize; j++)
                        {
                            if (ScrLo[j] == CurrentColor)
                            {
                                Num1++;
                            }
                            if (ColRAM[j] == CurrentColor)
                            {
                                Num2++;
                            }
                        }


                        if (Num1 >= Num2)
                        {
                            for (int j = SeqStart; j <= SeqEnd; j++)
                            {
                                ColRAM[j] = ScrLo[j];
                                ScrLo[j] = CurrentColor;
                            }
                        }
                    }
                }
            }
            // If we encounter a new color, update the current color and segment indices                                            
            CurrentColor = ScrHi[i]; // Update current color
            SeqStart = i;            // Start a new segment
            SeqEnd = i;              // Initialize end of the segment
        }
        else
        {
            SeqEnd = i; // Extend the end of the current segment
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool SortBySeqLen(colorspace A, colorspace B)
{
    return A.SeqLen > B.SeqLen;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool SortByFrequency(colorspace A, colorspace B)
{
    return A.Frequency > B.Frequency;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
/*
bool SortByFrequency(colorspace A, colorspace B)
{
    return A.Frequency > B.Frequency;
}
*/
//----------------------------------------------------------------------------------------------------------------------------------------------------------

//Force color assignment to a specific space
inline void AssignToSpace(int index, unsigned char color, unsigned char space)
{
    switch (space)
    {
        case 0:
            ScrHi[index] = color;
            break;
        case 1:
            ScrLo[index] = color;
            break;
        case 2:
            ColRAM[index] = color;
            break;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

//Attempt color assignment to a specific space
inline bool TryAssignToSpace(int index, unsigned char color, unsigned char space)
{
    switch (space)
    {
        case 0:
            if (ScrHi[index] == 255)
            {
                ScrHi[index] = color;
                return true;
            }
            break;
        case 1:
            if (ScrLo[index] == 255)
            {
                ScrLo[index] = color;
                return true;
            }
            break;
        case 2:
            if (ColRAM[index] == 255)
            {
                ColRAM[index] = color;
                return true;
            }
            break;
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void AssignColor(unsigned char color)
{
    //Precalculate overlaps for all color spaces
    int overlapSH = 0, overlapSL = 0, overlapCR = 0;
    unsigned char* colorMapOffset = &ColMap[color * ColTabSize];

    if (!IsHires)
    {
        //Calculate overlap counts in a single pass
        for (int i = 0; i < ColTabSize; i++)
        {
            if (colorMapOffset[i] == 255)
            {
                overlapSH += (ScrHi[i] != 255);
                overlapSL += (ScrLo[i] != 255);
                overlapCR += (ColRAM[i] != 255);
            }
        }

        //Determine the priority order for color spaces
        unsigned char firstSpace, secondSpace, thirdSpace;

        //Find smallest overlap
        if (overlapSH <= overlapSL && overlapSH <= overlapCR)
        {
            firstSpace = 0; //ScrHi has smallest overlap
            secondSpace = (overlapSL <= overlapCR) ? 1 : 2; // Choose between ScrLo and ColRAM
            thirdSpace = (secondSpace == 1) ? 2 : 1;
        }
        else if (overlapSL <= overlapSH && overlapSL <= overlapCR)
        {
            firstSpace = 1; // ScrLo has smallest overlap
            secondSpace = (overlapSH <= overlapCR) ? 0 : 2;
            thirdSpace = (secondSpace == 0) ? 2 : 0;
        }
        else
        {
            firstSpace = 2; // ColRAM has smallest overlap
            secondSpace = (overlapSH <= overlapSL) ? 0 : 1;
            thirdSpace = (secondSpace == 0) ? 1 : 0;
        }

        //Assign color according to prioritized color spaces
        for (int i = 0; i < ColTabSize; i++)
        {
            if (colorMapOffset[i] != 255)
            {
                continue;
            }

            //Try to assign to preferred spaces in order
            if (TryAssignToSpace(i, color, firstSpace))
            {
                continue;
            }

            if (TryAssignToSpace(i, color, secondSpace))
            {
                continue;
            }

            //If we get here, assign to the third choice
            AssignToSpace(i, color, thirdSpace);
        }
    }
    else
    {
        //Calculate overlap counts in a single pass
        for (int i = 0; i < ColTabSize; i++)
        {
            if (colorMapOffset[i] == 255)
            {
                overlapSH += (ScrHi[i] != 255);
                overlapSL += (ScrLo[i] != 255);
            }
        }

        //Determine the priority order for color spaces
        unsigned char firstSpace, secondSpace;

        //Find smallest overlap
        if (overlapSH <= overlapSL)
        {
            firstSpace = 0; //ScrHi has smallest overlap
            secondSpace = 1;
        }
        else
        {
            firstSpace = 1; // ScrLo has smallest overlap
            secondSpace = 0;
        }

        //Assign color according to prioritized color spaces
        for (int i = 0; i < ColTabSize; i++)
        {
            if (colorMapOffset[i] != 255)
            {
                continue;
            }

            //Try to assign to preferred spaces in order
            if (TryAssignToSpace(i, color, firstSpace))
            {
                continue;
            }

            //If we get here, assign to the third choice
            AssignToSpace(i, color, secondSpace);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void RenderImage(int Layout)
{
    if (Layout == 0)   //Hi * 16 + Lo, CR
    {
        for (int i = 0; i < ColTabSize; i++)
        {
            ScrRAM[i] = (ScrHi[i] * 16) + ScrLo[i];
            ColR[i] = ColRAM[i];
        }
    }
    else if (Layout == 1)  //Lo * 16 + Hi, CR
        for (int i = 0; i < ColTabSize; i++)
        {
            unsigned char Tmp = ScrLo[i];
            ScrLo[i] = ScrHi[i];
            ScrHi[i] = Tmp;
            ScrRAM[i] = (ScrHi[i] * 16) + ScrLo[i];
            ColR[i] = ColRAM[i];
        }
    else if (Layout == 2)  //Hi * 16 + CR, Lo
    {
        for (int i = 0; i < ColTabSize; i++)
        {
            unsigned char Tmp = ColRAM[i];
            ColR[i] = ScrLo[i];
            ScrLo[i] = Tmp;
            ScrRAM[i] = (ScrHi[i] * 16) + ScrLo[i];
        }
    }
    else if (Layout == 3)  //CR * 16 + Hi, Lo
    {
        for (int i = 0; i < ColTabSize; i++)
        {
            unsigned char Tmp = ColRAM[i];
            ColR[i] = ScrLo[i];
            ScrLo[i] = ScrHi[i];
            ScrHi[i] = Tmp;
            ScrRAM[i] = (ScrHi[i] * 16) + ScrLo[i];
        }
    }
    else if (Layout == 4)  //Lo * 16 + CR, Hi
    {
        for (int i = 0; i < ColTabSize; i++)
        {
            unsigned char Tmp = ColRAM[i];
            ColR[i] = ScrHi[i];
            ScrHi[i] = ScrLo[i];
            ScrLo[i] = Tmp;
            ScrRAM[i] = (ScrHi[i] * 16) + ScrLo[i];
        }
    }
    else if (Layout == 5)   //CR * 16 + Lo, Hi
    {
        for (int i = 0; i < ColTabSize; i++)
        {
            unsigned char Tmp = ColRAM[i];
            ColR[i] = ScrHi[i];
            ScrHi[i] = Tmp;
            ScrRAM[i] = (ScrHi[i] * 16) + ScrLo[i];
        }
    }

    CreateBitmapData();

    //RebuildImage(OutFile + "_" + to_string(BestNumFrag) + "_" + to_string(BestNumFragCol));
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool OptimizeKoala()
{    
    ScrHi.clear();
    ScrHi.resize(ColTabSize, 0);
    ScrLo.clear();
    ScrLo.resize(ColTabSize, 0);
    ScrRAM.clear();
    ScrRAM.resize(ColTabSize, 0);
    ColRAM.clear();
    ColRAM.resize(ColTabSize, 0);
    ColR.clear();
    ColR.resize(ColTabSize, 0);
    ColMap.clear();
    ColMap.resize(ColTabSize * 16, 0);             //We still have UnusedColor = 16 here, so we need an extra map???

    int C64Col[17]{};                                           //0x00-0x10 (UnusedColor = 0x10)
    int NumSeq[16]{};

    vecBMP.clear();

    int NumColors = 0;

    for (int c = 0; c < 16; c++)
    {
        ColorSpace[c].Used = false;
    }

    for (int i = 0; i < ColTabSize; i++)
    {
        ColRAM[i] = 255;                                        //Reset color spaces
        ScrHi[i] = 255;
        ScrLo[i] = 255;

        C64Col[ColTab1[i]]++;                                   //Calculate frequency of colors
        C64Col[ColTab2[i]]++;
        C64Col[ColTab3[i]]++;

        if (ColTab1[i] != UnusedColor)
        {
            ColorSpace[ColTab1[i]].Used = true;
        }
        if (ColTab2[i] != UnusedColor)
        {
            ColorSpace[ColTab2[i]].Used = true;
        }
        if (ColTab3[i] != UnusedColor)
        {
            ColorSpace[ColTab3[i]].Used = true;
        }

        if (ColTab1[i] != UnusedColor)
        {
            ColMap[(ColTab1[i] * ColTabSize) + i] = 255;        //Create a separate color map for each color, avoiding UnusedColor
        }
        if (ColTab2[i] != UnusedColor)
        {
            ColMap[(ColTab2[i] * ColTabSize) + i] = 255;
        }
        if (ColTab3[i] != UnusedColor)
        {
            ColMap[(ColTab3[i] * ColTabSize) + i] = 255;
        }
    }

    for (int c = 0; c < 16; c++)
    {
        if (C64Col[c] > 0)
        {
            NumColors++;
        }
        
        ColorSpace[c].Color = c;
        
        int LenCol = 0;
        int ThisSeq = 0;

        for (int i = 0; i < ColTabSize; i++)
        {
            if (ColMap[(c * ColTabSize) + i] == 255)
            {
                if (ThisSeq == 0)
                {
					NumSeq[c]++;  //Count number of sequences for this color
                }
                LenCol++;
                ThisSeq++;
            }
            else
            {
                ThisSeq = 0;
            }
        }

        ColorSpace[c].Frequency = NumSeq[c];

        if (NumSeq[c] != 0)
        {
            ColorSpace[c].SeqLen = (double)LenCol / (double)NumSeq[c];
            ColorSpace[c].Frequency = LenCol;
        }
    }
    if (VerboseMode)
    {
        cout << "Colors used: " << NumColors + 1 << "\n";
    }

    int BestFrag = ColTabSize * 2;
    int BestFragCol = ColTabSize * 2;
    int BestCompress = INT_MAX;

    int Iterations = NumColors > 8 ? 8 : NumColors < 4 ? 4 : NumColors;     //Iterations: min. 4, max. 8

    vector <int> ColorOrder{};

    sort(ColorSpace.begin(), ColorSpace.end(), SortBySeqLen);

    for (int c0 = 0; c0 < Iterations; c0++)
    {
        for (int c1 = 0; c1 < Iterations; c1++)
        {
            if (c1 != c0)
            {
                for (int c2 = 0; c2 < Iterations; c2++)
                {
                    if ((c2 != c0) && (c2 != c1))
                    {
                        for (int c3 = 0; c3 < Iterations; c3++)
                        {
                            if ((c3 != c0) && (c3 != c1) && (c3 != c2))
                            {
                                if (!IsHires)
                                {
                                    fill(ColRAM.begin(), ColRAM.end(), 255);
                                }
                                else
                                {
                                    fill(ColRAM.begin(), ColRAM.end(), 0);
                                }
                                fill(ScrHi.begin(), ScrHi.end(), 255);
                                fill(ScrLo.begin(), ScrLo.end(), 255);

                                int CurrentColorOrder = 0;

                                if (ColorSpace[c0].Used)
                                {
                                    CurrentColorOrder = ColorSpace[c0].Color;
                                    AssignColor(ColorSpace[c0].Color);
                                }
                                if (ColorSpace[c1].Used)
                                {
                                    CurrentColorOrder = CurrentColorOrder * 16 + ColorSpace[c1].Color;
                                    AssignColor(ColorSpace[c1].Color);

                                }
                                if (ColorSpace[c2].Used)
                                {
                                    CurrentColorOrder = CurrentColorOrder * 16 + ColorSpace[c2].Color;
                                    AssignColor(ColorSpace[c2].Color);

                                }
                                if (ColorSpace[c3].Used)
                                {
                                    CurrentColorOrder = CurrentColorOrder * 16 + ColorSpace[c3].Color;
                                    AssignColor(ColorSpace[c3].Color);

                                }

                                int MaxCol = (IsHires) ? 16 : 15;

                                for (int i = 0; i < MaxCol; i++)
                                {
                                    if ((ColorSpace[i].Used) && (i != c0) && (i != c1) && (i != c2) && (i != c3))
                                    {
                                        AssignColor(ColorSpace[i].Color);
                                    }
                                }

                                for (int i = 0; i < 2; i++)
                                {
                                    RelocateSingleBlocks();
                                    //DistributeUnusedBlocks();
                                    FillUnusedBlocks();
                                    FixOverlaps();
                                    MoveMatchingSingles();
                                    //ConnectMatchingSegments();
                                }

                                int Layout = FindBestLayout();

                                if (OnePassMode)
                                {
                                    if ((BestNumFrag < BestFrag) && (BestNumFragCol < BestFragCol))
                                    {

                                        ColorOrder.push_back(CurrentColorOrder);

                                        RenderImage(Layout);

                                        BestFrag = BestNumFrag;
                                        BestFragCol = BestNumFragCol;

                                        Predictors.push_back(BestNumFrag + BestNumFragCol);

                                        if (VerboseMode)
                                        {
                                            cout << "Output candidate #" << Predictors.size() << " with color order ";
                                            cout << (hex);
                                            if (ColorSpace[c0].Used) cout << (int)ColorSpace[c0].Color;
                                            if (ColorSpace[c1].Used) cout << (int)ColorSpace[c1].Color;
                                            if (ColorSpace[c2].Used) cout << (int)ColorSpace[c2].Color;
                                            if (ColorSpace[c3].Used) cout << (int)ColorSpace[c3].Color;
                                            for (int i = 0; i < 15; i++)
                                            {
                                                if ((ColorSpace[i].Used) && (i != c0) && (i != c1) && (i != c2) && (i != c3))
                                                {
                                                    cout << (int)ColorSpace[i].Color;
                                                }
                                            }
                                            cout << (dec) << "\n";
                                        }
                                    }
                                }
                                else
                                {
                                    if ((BestNumFrag + BestNumFragCol < BestFrag + BestFragCol))  // && (BestNumFragCol < BestFragCol))
                                    {
                                        RenderImage(Layout);

                                        BestFrag = BestNumFrag;
                                        BestFragCol = BestNumFragCol;

                                        Predictors.push_back(BestNumFrag + BestNumFragCol);

                                        if (VerboseMode)
                                        {
                                            cout << "Output candidate #" << Predictors.size() << " with color order ";
                                            cout << (hex);
                                            if (ColorSpace[c0].Used) cout << (int)ColorSpace[c0].Color;
                                            if (ColorSpace[c1].Used) cout << (int)ColorSpace[c1].Color;
                                            if (ColorSpace[c2].Used) cout << (int)ColorSpace[c2].Color;
                                            if (ColorSpace[c3].Used) cout << (int)ColorSpace[c3].Color;

                                            int MaxCol = (IsHires) ? 16 : 15;

                                            for (int i = 0; i < MaxCol; i++)                                            {
                                                if ((ColorSpace[i].Used) && (i != c0) && (i != c1) && (i != c2) && (i != c3))
                                                {
                                                    cout << (int)ColorSpace[i].Color;
                                                }
                                            }
                                            cout << (dec) << "\n";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!OnePassMode)
    {
        sort(ColorSpace.begin(), ColorSpace.end(), SortByFrequency);

        BestFrag = ColTabSize * 2;
        BestFragCol = ColTabSize * 2;

        for (int c0 = 0; c0 < Iterations; c0++)
        {
            for (int c1 = 0; c1 < Iterations; c1++)
            {
                if (c1 != c0)
                {
                    for (int c2 = 0; c2 < Iterations; c2++)
                    {
                        if ((c2 != c0) && (c2 != c1))
                        {
                            for (int c3 = 0; c3 < Iterations; c3++)
                            {
                                if ((c3 != c0) && (c3 != c1) && (c3 != c2))
                                {
                                    if (!IsHires)
                                    {
                                        fill(ColRAM.begin(), ColRAM.end(), 255);
                                    }
                                    else
                                    {
                                        fill(ColRAM.begin(), ColRAM.end(), 0);
                                    }
                                    fill(ScrHi.begin(), ScrHi.end(), 255);
                                    fill(ScrLo.begin(), ScrLo.end(), 255);

                                    int CurrentColorOrder = 0;

                                    if (ColorSpace[c0].Used)
                                    {
                                        CurrentColorOrder = ColorSpace[c0].Color;
                                        AssignColor(ColorSpace[c0].Color);
                                    }
                                    if (ColorSpace[c1].Used)
                                    {
                                        CurrentColorOrder = CurrentColorOrder * 16 + ColorSpace[c1].Color;
                                        AssignColor(ColorSpace[c1].Color);

                                    }
                                    if (ColorSpace[c2].Used)
                                    {
                                        CurrentColorOrder = CurrentColorOrder * 16 + ColorSpace[c2].Color;
                                        AssignColor(ColorSpace[c2].Color);

                                    }
                                    if (ColorSpace[c3].Used)
                                    {
                                        CurrentColorOrder = CurrentColorOrder * 16 + ColorSpace[c3].Color;
                                        AssignColor(ColorSpace[c3].Color);

                                    }

                                    if (find(ColorOrder.begin(), ColorOrder.end(), CurrentColorOrder) != ColorOrder.end())
                                    {
                                        continue; // Skip if this color order has already been processed
                                    }

                                    int MaxCol = (IsHires) ? 16 : 15;

                                    for (int i = 0; i < MaxCol; i++)
                                    {
                                        if ((ColorSpace[i].Used) && (i != c0) && (i != c1) && (i != c2) && (i != c3))
                                        {
                                            AssignColor(ColorSpace[i].Color);
                                        }
                                    }

                                    for (int i = 0; i < 2; i++)
                                    {
                                        RelocateSingleBlocks();
                                        //DistributeUnusedBlocks();
                                        FillUnusedBlocks();
                                        FixOverlaps();
                                        MoveMatchingSingles();
                                        //ConnectMatchingSegments();
                                    }

                                    int Layout = FindBestLayout();

                                    if (IsHires)
                                        Layout = 0;

                                    if ((BestNumFrag + BestNumFragCol < BestFrag + BestFragCol))  // && (BestNumFragCol < BestFragCol))
                                    {

                                        //ColorOrder.push_back(CurrentColorOrder);

                                        RenderImage(Layout);

                                        BestFrag = BestNumFrag;
                                        BestFragCol = BestNumFragCol;

                                        Predictors.push_back(BestNumFrag + BestNumFragCol);

                                        if (VerboseMode)
                                        {
                                            cout << "Output candidate #" << Predictors.size() << " with color order ";
                                            cout << (hex);
                                            if (ColorSpace[c0].Used) cout << (int)ColorSpace[c0].Color;
                                            if (ColorSpace[c1].Used) cout << (int)ColorSpace[c1].Color;
                                            if (ColorSpace[c2].Used) cout << (int)ColorSpace[c2].Color;
                                            if (ColorSpace[c3].Used) cout << (int)ColorSpace[c3].Color;

                                    int MaxCol = (IsHires) ? 16 : 15;

                                    for (int i = 0; i < MaxCol; i++)
                                            {
                                                if ((ColorSpace[i].Used) && (i != c0) && (i != c1) && (i != c2) && (i != c3))
                                                {
                                                    cout << (int)ColorSpace[i].Color;
                                                }
                                            }
                                            cout << (dec) << "\n";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    size_t IdxBest = Predictors.size() - 1;

    if (!OnePassMode)
    {
        if (vecBMP.size() > 1)
        {
            for (size_t i = 0; i < vecBMP.size(); i++)
            {
                if (VerboseMode)
                {
                    cout << "Analyzing output candidate #" << i + 1 << ".";
                }

                int ThisCompress = CalculateCompressedSize(vecBMP[i]);

                if (ThisCompress < BestCompress)
                {
                    IdxBest = i;
                    BestCompress = ThisCompress;
                }
                else if ((ThisCompress == BestCompress) && (Predictors[i] < Predictors[IdxBest]))
                {
                    IdxBest = i;
                    BestCompress = ThisCompress;
                }

                if (VerboseMode)
                {
#ifdef DEBUG
                    cout << " Cost: " << ThisCompress << " bytes. Best candidate: #" << IdxBest + 1 << "\n";
#else
                    cout << " Best candidate: #" << IdxBest + 1 << "\n";
#endif // DEBUG

                }
            }
        }
    }

    if (VerboseMode)
    {
        cout << "Creating output using candidate #" << IdxBest + 1 << "\n";
    }

    for (size_t i = 0; i < (size_t)ColTabSize * 8; i++)
    {
        BMP[i] = vecBMP[IdxBest][i];
    }

    for (size_t i = 0; i < (size_t)ColTabSize; i++)
    {
        ScrRAM[i] = vecBMP[IdxBest][i + (size_t)(8 * ColTabSize)];
        ColR[i] = vecBMP[IdxBest][i + (size_t)(9 * ColTabSize)];
    }

    BGCol = vecBMP[IdxBest][(size_t)(10 * ColTabSize)];

    return RebuildImage(OutFile);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool OptimizeImage()
{
    bool ReturnStatus = true;

    SaveImgFormat();

    if ((!OutputKla) && (!OutputMap) && (!OutputCol) && (!OutputScr) && (!OutputCcr) && (!OutputObm) && (!OutputKlx))
    {
        return true;
    }
    else if ((PicW % 8 != 0) || (PicH % 8 != 0))
    {
        cerr << "***CRITICAL***\tUnable to convert image to C64 formats. The dimensions of the image must be multiples of 8.\n";
        return false;
    }
   
    Pic.clear();
    Pic.resize(PicW * PicH, 0);

    PicMsk.clear();
    PicMsk.resize(PicW * PicH, 0);

    ColTabSize = CharCol * CharRow;

    vector <unsigned char> CT0(ColTabSize, UnusedColor), CT1(ColTabSize, UnusedColor), CT2(ColTabSize, UnusedColor), CT3(ColTabSize, UnusedColor);

    ColTab0.clear();
    ColTab0.resize(ColTabSize, 0);
    ColTab1.clear();
    ColTab1.resize(ColTabSize, 0);
    ColTab2.clear();
    ColTab2.resize(ColTabSize, 0);
    ColTab3.clear();
    ColTab3.resize(ColTabSize, 0);
    BMP.clear();
    BMP.resize(CharCol * PicH, 0);

    //Fetch R component of the RGB color code for each pixel, convert it to C64 palette, and save it to Pic array
    
    int Increment = (IsHires) ? 1 : 2;  //If MC bitmap - only check even X offsets

    for (size_t Y = 0; Y < PicH; Y++)
    {
        for (size_t X = 0; X < PicW; X += Increment)
        {
            Pic[(Y * PicW) + X] = paletteconvtab[GetPixel(C64Bitmap, X, Y, PicW).R];
        }
    }

    //Sort R values into 3 Color Tabs per char
    for (int CY = 0; CY < CharRow; CY++)                //Char rows
    {
        for (int CX = 0; CX < CharCol; CX++)            //Chars per row
        {
            int CP = (CY * 8 * PicW) + (CX * 8);        //Char's position within R array

            for (int BY = 0; BY < 8; BY++)              //Pixel's Y-position within char
            {
                for (int BX = 0; BX < 8; BX += Increment)          //Pixel's X-position within char
                {
                    int PicPos = min(CP + (BY * PicW) + BX, (PicW * PicH) - 1); //This min() function is only needed to avoid error message in VS
                    unsigned char V = Pic[PicPos];      //Fetch R value of pixel in char

                    int CharIndex = (CY * CharCol) + CX;

                    if ((CT0[CharIndex] == V) || (CT1[CharIndex] == V) || (CT2[CharIndex] == V) || (CT3[CharIndex] == V))
                    {
                        //Color can only be stored once
                        continue;
                    }
                    else if (CT1[CharIndex] == UnusedColor)
                    {
                        //If color is not in Color Tabs yet, first fill Color Tab 1
                        CT1[CharIndex] = V;
                    }
                    else if (CT2[CharIndex] == UnusedColor)
                    {
                        //Then try Color Tab 2
                        CT2[CharIndex] = V;
                    }
                    else if (CT3[CharIndex] == UnusedColor) //&& !IsHires)
                    {
                        //Then try Color Tab 3
                        CT3[CharIndex] = V;
                    }
                    else if (CT0[CharIndex] == UnusedColor) //&& !IsHires)
                    {
                        //Finally try Color Tab 0
                        CT0[CharIndex] = V;
                    }
                    else
                    {
                        if (IsHires)
                        {
                            cerr << "***CRITICAL***\tThis hi-res picture cannot be converted as it contains more than 2 colors per char block!\n";
                            ReturnStatus = false;
                            break;
                        }
                        else
                        {
                            cerr << "***CRITICAL***\tThis multicolor picture cannot be converted as it contains more than 4 colors per char block!\n";
                            ReturnStatus = false;
                            break;
                        }
                    }
                }
                if (!ReturnStatus)
                {
                    break;
                }
            }
            if (!ReturnStatus)
            {
                break;
            }
        }
        if (!ReturnStatus)
        {
            break;
        }
    }

    if (ReturnStatus)
    {
        //Find possible background colors

        NumBGCols = 0;

        for (int C = 0; C < 16; C++)
        {
            //Try all 16 colors as backgroud color

            bool ColOK = true;
            for (int i = 0; i < ColTabSize; i++)
            {
                int ColCnt = 0;
                if ((CT0[i] != C) && (CT0[i] != UnusedColor)) ColCnt++;
                if ((CT1[i] != C) && (CT1[i] != UnusedColor)) ColCnt++;
                if ((CT2[i] != C) && (CT2[i] != UnusedColor)) ColCnt++;
                if ((CT3[i] != C) && (CT3[i] != UnusedColor)) ColCnt++;
                if (ColCnt == 4)
                {
                    //We can only have 3 non-background colors per char. If we find 4 then the current color cannot be used as a background color

                    ColOK = false;
                    break;
                }
            }
            if (ColOK)
            {
                BGCols[NumBGCols++] = C;
            }
        }

        if (NumBGCols == 0 && !IsHires)
        {
            cerr << "***CRITICAL***\tThis picture cannot be converted as none of the C64 colors can be used as a background color!\n";
            ReturnStatus = false;
        }
        
        if ((NumBGCols > 1) && (CmdColors.size() != 1) && (!IsHires))
        {
            cout << "***INFO***\tMore than one possible background color has been identified.\n";
            cout << "\t\tThe background color will be appended to the output file names.\n";
            cout << "\t\tIf you only want one output background color then specify it using the -b switch,\n";
            cout << "\t\tor use 'x' to only use the first possible background color.\n";
        }

        bool ColFound = false;
        if ((NumBGCols > 0) && (CmdColors != "x") && (!IsHires))
        {
            for (int i = 0; i < NumBGCols; i++)
            {
                string BGC = ConvertIntToHextString((int)BGCols[i], 1);
                if (CmdColors.find(BGC) != string::npos)
                {
                    ColFound = true;
                }
            }

            if (!ColFound)
            {
                cerr << "***CRITICAL***This picture cannot be converted with the requested background color(s)!\n";
                ReturnStatus = false;
            }
        }

        if (ReturnStatus)
        {
            //Optimize bitmap with all possible background colors

            if (!IsHires)
            {
                for (int C = 0; C < NumBGCols; C++)
                {
                    BGCol = BGCols[C];
                    char cCol = '0';

                    if (BGCol < 10)
                    {
                        cCol = '0' + BGCol;
                    }
                    else
                    {
                        cCol = 'a' + (BGCol - 10);
                    }

                    //Check if the current background color is on the list

                    if ((CmdColors == "x") || (CmdColors.find(cCol) != string::npos))
                    {
                        for (int i = 0; i < ColTabSize; i++)
                        {
                            ColTab0[i] = CT0[i];
                            ColTab1[i] = CT1[i];
                            ColTab2[i] = CT2[i];
                            ColTab3[i] = CT3[i];
                            if (ColTab1[i] == BGCol)
                            {
                                ColTab1[i] = ColTab0[i];
                                ColTab0[i] = UnusedColor;

                            }
                            else if (ColTab2[i] == BGCol)
                            {
                                ColTab2[i] = ColTab0[i];
                                ColTab0[i] = UnusedColor;
                            }
                            else if (ColTab3[i] == BGCol)
                            {
                                ColTab3[i] = ColTab0[i];
                                ColTab0[i] = UnusedColor;
                            }
                        }

                        //ColTab0 can only contain UnusedColor and BGCol here!!! - we won't need to use it in OptimizeByColor()

                        OptimizeKoala();

                        if (CmdColors == "x")
                        {
                            CmdColors = "";
                        }
                    }
                }
            }
            else
            {
                for (int i = 0; i < ColTabSize; i++)
                {
                    ColTab1[i] = CT1[i];
                    ColTab2[i] = CT2[i];
                    ColTab3[i] = UnusedColor;
                    ColTab0[i] = UnusedColor;
                }
                OptimizeKoala();
            }
        }
    }

    return ReturnStatus;  
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool ConvertPicToC64Palette()
{
    for (int i = 0; i < c64palettes_size; i++)
    {
        c64palettesYUV[i] = RGB2YUV(c64palettes[i]);
    }

    CharCol = PicW / 8;
    CharRow = PicH / 8;

    if ((OutputKla) || (OutputObm))
    {
        if ((CharCol < 40) || (CharRow < 25))
        {
            cout << "***INFO***\tThis image cannot be saved as Koala (.kla) or optimized bitmap (.obm) as it is smaller than 320x200 pixels!\n";
            cout << "\t\tAll other selected output format will be created.\n";
        }
    }

    unsigned int ThisPalette[16]{};
    for (int i = 0; i < 16; i++)
    {
        ThisPalette[i] = 0xffffffff;
    }

    C64Bitmap.resize((size_t)PicW * PicH * 4);

    int Increment = (IsHires) ? 1 : 2;

    //Count colors in bitmap
    int NumColors = 0;
    for (size_t y = 0; y < PicH; y++)
    {
        for (size_t x = 0; x < PicW; x += Increment)
        {
            unsigned int ThisCol = GetColor(Image, x, y, PicW);
            bool ColorMatch = false;
            for (int c = 0; c < NumColors; c++)
            {
                ColorMatch = (ThisPalette[c] == ThisCol);
                if (ColorMatch)
                    break;
            }
            if (!ColorMatch)
            {
                if (NumColors == 16)
                {
                    cerr << "This picture cannot be processed because it contains more than 16 colors!";
                    return false;
                }
                else
                {
                    ThisPalette[NumColors++] = ThisCol;
                }
            }
        }
    }

    //First check if there is an exact palette match

    int PaletteIdx = -1;

    for (int i = 0; i < NumPalettes; i++)
    {
        bool PaletteMatch = false;
        for (int t = 0; t < NumColors; t++)
        {
            bool ColorMatch = false;
            for (int c = 0; c < 16; c++)
            {
                ColorMatch = (c64palettes[(i * 16) + c] == ThisPalette[t]);
                if (ColorMatch)
                {
                    break;
                }
            }
            PaletteMatch = ColorMatch;
            if (!PaletteMatch)
            {
                break;
            }
        }
        if (PaletteMatch)
        {
            PaletteIdx = i;
            break;
        }
    }

    if (PaletteIdx > -1)
    {
        if (VerboseMode)
        {
            cout << "Exact palette match found: " << PaletteNames[PaletteIdx] << "\n";
        }
        //Palette match found, convert to Pixcen palette
        for (size_t y = 0; y < PicH; y++)
        {
            for (size_t x = 0; x < PicW; x++)
            {
                unsigned int ThisCol = GetColor(Image, x, y, PicW);

                for (int i = 0; i < 16; i++)
                {
                    if (c64palettes[(PaletteIdx * 16) + i] == ThisCol)
                    {
                        int DefaultColor = c64palettes[VICE_36_Pixcen * 16 + i];  //Use Pixcen palette because paletteconvtab works with that one.
                        SetColor(C64Bitmap, x, y, PicW, DefaultColor);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        if (VerboseMode)
        {
            cout << "No exact palette match found. Closest palette: ";
        }
        
        int BestPaletteIdx = 0;

        yuv ThisPaletteYUV[16]{};

        for (int i = 0; i < NumColors; i++)
        {
            ThisPaletteYUV[i] = RGB2YUV(ThisPalette[i]);;
        }

        double BestDistance = numeric_limits<double>::max();
        int BestColorIdx[16]{};

        for (int p = 0; p < NumPalettes; p++)
        {
            double ColorDistance[16][16]{};

            for (int c = 0; c < 16; c++)
            {
                for (int t = 0; t < 16; t++)
                {
                    if (ThisPalette[t] == 0xffffffff)
                    {
                        ColorDistance[c][t] = 0;
                    }
                    else
                    {
                        ColorDistance[c][t] = YUVDistance(c64palettesYUV[p * 16 + c], ThisPaletteYUV[t]);
                    }
                }
            }

            double ThisDistance = HungarianAlgorithm(ColorDistance);
            
            if (ThisDistance < BestDistance)
            {
                BestDistance = ThisDistance;
                BestPaletteIdx = p;
                for (int i = 0; i < 16; i++)
                {
                    BestColorIdx[i] = PaletteAssignment[i];
                }
            }
        }

        if (VerboseMode)
        {
            cout << PaletteNames[BestPaletteIdx] << "\n";
        }

        //Palette match found, convert to Pixcen palette
        for (size_t y = 0; y < PicH; y++)
        {
            for (size_t x = 0; x < PicW; x++)
            {
                unsigned int ThisCol = GetColor(Image, x, y, PicW);

                for (int i = 0; i < NumColors; i++)
                {
                    if (ThisPalette[i] == ThisCol)  //Find the index of the color in ThisPalette
                    {
                        int ColorIndex = BestColorIdx[i];
                        unsigned int DefaultColor = c64palettes[VICE_36_Pixcen * 16 + ColorIndex]; //Identify the color in the Best Match Palette

                        SetColor(C64Bitmap, x, y, PicW, DefaultColor);
                        break;
                    }
                }
            }
        }
    }

    return OptimizeImage();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool DecodeBmp()
{
    const size_t BIH = 0x0e;                                            //Offset of Bitmap Info Header within raw data
    const size_t DATA_OFFSET = 0x0a;                                    //Offset of Bitmap Data Start within raw data
    const size_t MINSIZE = sizeof(tagBITMAPINFOHEADER) + BIH;           //Minimum header size

    if (ImgRaw.size() < MINSIZE)
    {
        cerr << "***CRITICAL***\tThe size of this BMP file is smaller than the minimum size allowed.\n";
        return false;
    }

    memcpy(&BmpInfoHeader, &ImgRaw[BIH], sizeof(BmpInfoHeader));

    if ((BmpInfoHeader.biCompression != 0) && (BmpInfoHeader.biCompression != 3))
    {
        cerr << "***CRITICAL***\tUnsupported BMP format. Sparkle can only work with uncompressed BMP files.\nT";
        return false;
    }

    int ColMax = (BmpInfoHeader.biBitCount < 24) ? (1 << BmpInfoHeader.biBitCount) : 0;

    PBITMAPINFO BmpInfo = (PBITMAPINFO)new char[sizeof(BITMAPINFOHEADER) + (ColMax * sizeof(RGBQUAD))];

    //Copy info header into structure
    memcpy(&BmpInfo->bmiHeader, &BmpInfoHeader, sizeof(BmpInfoHeader));

    //Copy palette into structure
    for (size_t i = 0; i < (size_t)ColMax; i++)
    {
        memcpy(&BmpInfo->bmiColors[i], &ImgRaw[0x36 + (i * 4)], sizeof(RGBQUAD));
    }

    //Calculate data offset
    size_t DataOffset = (size_t)ImgRaw[DATA_OFFSET] + (size_t)(ImgRaw[DATA_OFFSET + 1] * 0x100) + (size_t)(ImgRaw[DATA_OFFSET + 2] * 0x10000) + (size_t)(ImgRaw[DATA_OFFSET + 3] * 0x1000000);

    //Calculate length of pixel rows in bytes
    size_t RowLen = ((size_t)BmpInfo->bmiHeader.biWidth * (size_t)BmpInfo->bmiHeader.biBitCount) / 8;

    //BMP pads pixel rows to a multiple of 4 in bytes
    size_t PaddedRowLen = (RowLen % 4) == 0 ? RowLen : RowLen - (RowLen % 4) + 4;
    //size_t PaddedRowLen = ((RowLen + 3) / 4) * 4;

    size_t CalcSize = DataOffset + (BmpInfo->bmiHeader.biHeight * PaddedRowLen);

    if (ImgRaw.size() != CalcSize)
    {
        cerr << "***CRITICAL***\tCorrupted BMP file size.\n";

        delete[] BmpInfo;

        return false;
    }

    //Calculate size of our image vector (we will use 4 bytes per pixel in RGBA format)
    size_t BmpSize = (size_t)BmpInfo->bmiHeader.biWidth * 4 * (size_t)BmpInfo->bmiHeader.biHeight;

    //Resize image vector
    Image.resize(BmpSize, 0);

    size_t BmpOffset = 0;

    if (ColMax != 0)    //1 bit/pixel, 4 Bits/pixel, 8 Bits/pixel modes - use palette data
    {
        int BitsPerPx = BmpInfo->bmiHeader.biBitCount;   //1         4          8
        int bstart = 8 - BitsPerPx;                      //1-bit: 7; 4-bit:  4; 8-bit =  0;
        int mod = 1 << BitsPerPx;                        //1-bit: 2; 4-bit: 16; 8-bit: 256;

        for (int y = (BmpInfo->bmiHeader.biHeight - 1); y >= 0; y--)    //Pixel rows are read from last bitmap row to first
        {
            size_t RowOffset = DataOffset + (y * PaddedRowLen);

            for (size_t X = 0; X < RowLen; X++)                         //Pixel rows are read left to right
            {
                unsigned int Pixel = ImgRaw[RowOffset + X];

                for (int b = bstart; b >= 0; b -= BitsPerPx)
                {
                    int PaletteIndex = (Pixel >> b) % mod;

                    Image[BmpOffset++] = BmpInfo->bmiColors[PaletteIndex].rgbRed;
                    Image[BmpOffset++] = BmpInfo->bmiColors[PaletteIndex].rgbGreen;
                    Image[BmpOffset++] = BmpInfo->bmiColors[PaletteIndex].rgbBlue;
                    Image[BmpOffset++] = 0xff;
                }
            }
        }
    }
    else
    {
        int BytesPerPx = BmpInfo->bmiHeader.biBitCount / 8;             //24 Bits/pixel: 3; 32 Bits/pixel: 4

        for (int y = (BmpInfo->bmiHeader.biHeight - 1); y >= 0; y--)    //Pixel rows are read from last bitmap row to first
        {
            size_t RowOffset = DataOffset + (y * PaddedRowLen);

            for (size_t X = 0; X < RowLen; X += BytesPerPx)              //Pixel rows are read left to right
            {
                Image[BmpOffset++] = ImgRaw[RowOffset + X + 2];
                Image[BmpOffset++] = ImgRaw[RowOffset + X + 1];
                Image[BmpOffset++] = ImgRaw[RowOffset + X + 0];
                Image[BmpOffset++] = 0xff;
            }
        }
    }

    PicW = BmpInfo->bmiHeader.biWidth;      //Double pixels, effective width is half of BMP width
    PicH = BmpInfo->bmiHeader.biHeight;

    delete[] BmpInfo;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool IsMCImage()
{
    for (size_t Y = 0; Y < PicH; Y++)
    {
        for (size_t X = 0; X < PicW; X += 2)
        {
            color Col1 = GetPixel(Image, X, Y, PicW);
            color Col2 = GetPixel(Image, X + 1, Y, PicW);
            if ((Col1.R != Col2.R) || (Col1.G != Col2.G) || (Col1.B != Col2.B))
            {
                //cerr << "***CRITICAL***SPOT only accepts multicolor (double-pixel) pictures as input. This image file cannot be converted!\n";
                return false;
            }
        }
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool CountColorsPerBlock()
{
    for (size_t cy = 0; cy < PicH; cy += 8)
    {
        for (size_t cx = 0; cx < PicW; cx += 8)
        {
            unsigned int BlockCols[2]{};
            unsigned int NoCol = 0xff000000;
            BlockCols[0] = NoCol;
            BlockCols[1] = NoCol;

            for (size_t y = 0; y < 8; y++)
            {
                for (size_t x = 0; x < 8; x++)
                {
                    unsigned int ThisCol = GetColor(Image, cx + x, cy + y, PicW);
                    if (ThisCol != BlockCols[0] && ThisCol != BlockCols[1])
                    {
                        if (BlockCols[0] == NoCol)
                        {
                            BlockCols[0] = ThisCol;
                            continue;
                        }
                        else if (BlockCols[1] == NoCol)
                        {
                            BlockCols[1] = ThisCol;
                            continue;
                        }
                        else
                        {
                            cout << "***CRITICAL***\tThis image file has more than 2 colors per char block and cannot be converted to hires bitmap.\n";
                            return false;
                        }
                    }
                }
            }
        }
    }

    IsHires = true;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromImage()
{
    if (ReadBinaryFile(InFile, ImgRaw) == -1)
    {
        cerr << "***CRITICAL***\tUnable to open image file.\n";
        return false;
    }
    else if (ImgRaw.size() == 0)
    {
        cerr << "***CRITICAL***\tThe image file cannot be 0 bytes long.\n";
        return false;
    }

    if (FExt == "png")
    {
        //Load and decode PNG image using LodePNG (Copyright (c) 2005-2023 Lode Vandevenne)
        unsigned int error = lodepng::decode(Image, PicW, PicH, ImgRaw);
        
        if (error)
        {
            cerr << "***CRITICAL***\tPNG decoder error: " << error << ": " << lodepng_error_text(error) << "\n";
            return false;
        }
        //PicW /= 2;  //Double pixels, effective width is half of PNG width

    }
    else if (FExt == "bmp")
    {
        if (!DecodeBmp())
        {
            return false;
        }
    }   

    if (PicW == VICE_PicW && PicH == VICE_PicH)
    {
    //We have a VICE screenshot, trim borders

        if (VerboseMode)
        {
            cout << "VICE screenshot input image size detected. Borders will be trimmed.\n";
        }

        int StartOffset = PicW * 4 * 35 + 32 * 4;
        int RowLen = 320 * 4;
        int NumRows = 200;
        int o = 0;
        for (int j = 0; j < NumRows; j++)
        {
            for (int i = 0; i < RowLen; i++)
            {
                Image[o++] = Image[StartOffset + (j * PicW * 4) + i];
            }
        }
        
        Image.resize(320 * 200 * 4);
        PicW = 320;
        PicH = 200;
    }

    //Make sure this is a MC (double pixel) picture
    //if (!IsMCImage())
    //  return false;

    IsHires = !IsMCImage();

    if (IsHires || CmdMode == "h")
    {
        if (!CountColorsPerBlock())
        {
            return false;
        }
    }

    //Here we have the image decoded in the Image vector of unsigned chars, 4 bytes representing a pixel in RGBA format
    //Next we will find the best palette match
    return ConvertPicToC64Palette();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromKoala()
{
    PrgLen = ReadBinaryFile(InFile, ImgRaw);

    if (PrgLen == -1)
    {
        cerr << "***CRITICAL***\tUnable to open Koala" << (FExt == "klx" ? "X" : "") << " file.\n";
        return false;
    }

    size_t StartOffset{};

    if (FExt == "klx")
    {
        PicW = (ImgRaw[0] + ImgRaw[1] * 256);            //Double pixels, this is the effective width
        PicH = ImgRaw[2] + ImgRaw[3] * 256;
        StartOffset = 4;
    }
    else
    {
        PicW = 320;         //Double pixels, this is the effective width
        PicH = 200;
        StartOffset = 2;
    }

    CharCol = PicW / 8;
    CharRow = PicH / 8;

    ColTabSize = CharCol * CharRow;

    if ((size_t)PrgLen != (size_t)ColTabSize * 10 + 1 + StartOffset)
    {
        cerr << "***CRITICAL***\tInvalid Koala" << (FExt == "klx" ? "X" : "") << " file size.\n";
        return false;
    }

    ColTab1.resize(ColTabSize, 0);
    ColTab2.resize(ColTabSize, 0);
    ColTab3.resize(ColTabSize, 0);

    for (int i = 0; i < ColTabSize; i++)
    {
        ColTab1[i] = ImgRaw[(size_t)(8 * ColTabSize) + StartOffset + i] / 16;
        ColTab2[i] = ImgRaw[(size_t)(8 * ColTabSize) + StartOffset + i] % 16;
        ColTab3[i] = ImgRaw[(size_t)(9 * ColTabSize) + StartOffset + i] % 16;
    }

    for (int i = 1; i < 16; i++)
    {
        BGCols[i] = 0xff;
    }
    BGCols[0] = ImgRaw[ImgRaw.size() - 1];
    BGCol = BGCols[0];

    C64Bitmap.resize((size_t)PicW * PicH * 4, 0);

    Image.resize((size_t)PicW * PicH * 4, 0);

    int CI = 0;         //Color tab index
    int PxI = 0;        //Pixel index
    unsigned char BitMask = 0;
    unsigned char Bits = 0;
    unsigned char Col = 0;

    for (size_t Y = 0; Y < PicH; Y++)
    {
        for (size_t X = 0; X < PicW; X += 2)
        {
            CI = ((Y / 8) * CharCol) + (X / 8);                      //ColorTab index from X and Y
            PxI = ((X / 8) * 8) + (Y % 8) + ((Y / 8) * PicW);    //Pixel index in Bitmap

            if (X % 8 == 0)
            {
                BitMask = 0xc0;
            }
            else if (X % 8 == 2)
            {
                BitMask = 0x30;
            }
            else if (X % 8 == 4)
            {
                BitMask = 0x0c;
            }
            else if (X % 8 == 6)
            {
                BitMask = 0x03;
            }

            Bits = (ImgRaw[StartOffset + PxI] & BitMask);

            if (Bits == 0)
            {
                Col = BGCol;
            }
            else if ((Bits == 0x01) || (Bits == 0x04) || (Bits == 0x10) || (Bits == 0x40))
            {
                Col = ColTab1[CI];
            }
            else if ((Bits == 0x02) || (Bits == 0x08) || (Bits == 0x20) || (Bits == 0x80))
            {
                Col = ColTab2[CI];
            }
            else if ((Bits == 0x03) || (Bits == 0x0c) || (Bits == 0x30) || (Bits == 0xc0))
            {
                Col = ColTab3[CI];
            }

            //color C64Color{ oldc64palettes[Col], oldc64palettes[Col + 16], oldc64palettes[Col + 32],0 };

            int C64Col = c64palettes[VICE_36_Pixcen * 16 + Col];

            color C64Color{ (unsigned char)((C64Col >> 16) & 0xff),(unsigned char)((C64Col >> 8) & 0xff),(unsigned char)(C64Col & 0xff) };

            SetPixel(Image, X, Y, PicW, C64Color);
            SetPixel(Image, X + 1, Y, PicW, C64Color);
            SetPixel(C64Bitmap, X, Y, PicW, C64Color);
            SetPixel(C64Bitmap, X + 1, Y, PicW, C64Color);
        }
    }

    return OptimizeImage();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void ShowHelp()
{
    cout << "SPOT is a small cross-platform command-line tool that converts .png, .bmp, .kla, and .klx images into C64 file formats\n";
    cout << "optimized for better compression. The following output file formats can be selected: Koala (.kla), bitmap (.map),\n";
    cout << "screen RAM (.scr), color RAM (.col), compressed color RAM (.ccr)*, optimized bitmap (.obm)**, and KoalaX (.klx)***.\n\n";
    cout << "*Compressed color RAM (.ccr) format: two adjacent half bytes are combined to reduce the size of the color RAM to\n";
    cout << "500 bytes.\n\n";
    cout << "**Optimized bitmap (.obm) format: bitmap data is stored column wise. Screen RAM and compressed color RAM stored\n";
    cout << "row wise. First two bytes are address bytes ($00, $60) and the last one is the background color as in the Koala format.\n";
    cout << "File size: 9503 bytes. In most cases, this format compresses somewhat better than Koala but it also needs a more\n";
    cout << "complex display routine.\n\n";
    cout << "***KoalaX (.klx) format: similar to the Koala format, but replaces the first two address header bytes with 4 bytes\n";
    cout << "describing the width (double pixels) and height of the bitmap in low/high order. Bitmap pixel data, screen RAM, color\n";
    cout << "RAM, and background color are stored similar to the Koala format. Meant for handling non-standard image sizes.\n\n";
    cout << "Usage\n";
    cout << "-----\n\n";
    cout << "spot input -o [output] -f [format] -b [bgcolor] -v -s\n\n";
    cout << "input:   An input image file to be optimized/converted. Only .png, .bmp, .kla, and .klx file types are accepted.\n\n";
    cout << "-o       The output folder and file name. File extension (if exists) will be ignored. If omitted, SPOT will create\n";
    cout << "         a <spot/input> folder and the input file's name will be used as output file name.\n\n";
    cout << "-f       Output file formats: kmscg2opb. Select as many as you want in any order:\n";
    cout << "         k - .kla (Koala - 10003 bytes)\n";
    cout << "         m - .map (bitmap data)\n";
    cout << "         s - .scr (screen RAM data)\n";
    cout << "         c - .col (color RAM data)\n";
    cout << "         g - .bgc (background color)\n";
    cout << "         2 - .ccr (compressed color RAM data)\n";
    cout << "         o - .obm (optimized bitmap - 9503 bytes)\n";
    cout << "         p - .png (portable network graphics)\n";
    cout << "         b - .bmp (bitmap)\n";
    cout << "         x - .klx (KoalaX)\n";
    cout << "         This parameter is optional. If omitted, then a Koala file will be created by default.\n\n";
    cout << "-b       Output background color(s): 0123456789abcdef or x. SPOT will only create C64 files using the selected\n";
    cout << "         background color(s). If x is used as value then only the first possible background color will be used,\n";
    cout << "         all other possible background colors will be ignored. If this option is omitted, then SPOT will generate\n";
    cout << "         output files using all possible background colors. If more than one background color is possible (and\n";
    cout << "         allowed) then SPOT will append the background color to the output file name.\n\n";
    cout << "-v       Verbose mode.\n\n";
    cout << "-s       Simple/speedy mode. Skips compression cost calculation and selects the best candidate based on predictors.\n";
    cout << "         This mode can be helpful in the case of huge, non-standard images where standard mode could be extremely slow.\n\n";
    cout << "Examples\n";
    cout << "--------\n\n";
    cout << "spot picture.bmp -o newfolder/newfile -f msc -b 0\n";
    cout << "SPOT will convert <picture.bmp> to .map, .scr, and .col formats with black as background color and will save them to\n";
    cout << "the <newfolder> folder using <newfile> as output base filename.\n\n";
    cout << "spot picture.png -o newfolder/newfile -f msc\n";
    cout << "SPOT will convert <picture.png> to .map, .scr, and .col formats with all possible background colors and will save them\n";
    cout << "to the <newfolder> folder using <newfile> as output base filename.\n\n";
    cout << "spot picture.png -o newfolder/newfile\n";
    cout << "SPOT will convert <picture.png> to the default Koala format with all possible background colors and will save the\n";
    cout << "output to the <newfolder> folder using <newfile> as output base filename.\n\n";
    cout << "spot picture.png\n";
    cout << "SPOT will convert <picture.png> to the default Koala format with all possible background colors and will save the\n";
    cout << "output to the <spot/picture> folder using <picture> as output base filename\n\n";
    cout << "Notes\n";
    cout << "-----\n\n";
    cout << "SPOT recognizes several C64 palettes. If an exact palette match is not found then it will convert colors to\n";
    cout << "a standard C64 palette using a lowest-cost algorithm.\n\n";
    cout << "SPOT can handle non-standard image sizes (such as the vertical bitmap in Memento Mori and the diagonal bitmap\n";
    cout << "in Christmas Megademo). When a .kla or .obm file is created from a non-standard sized image, SPOT takes a centered\n";
    cout << "\"snapshot\" of the image and saves that as .kla or .obm. Map, screen RAM, and color RAM files can be of any size.\n\n";
    cout << "SPOT is meant to convert and optimize multicolor bitmaps (hi-res images get converted to multicolor).\n\n";
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    auto cstart = chrono::system_clock::now();

    cout << "\n";
    cout << "*********************************************************************\n";
    cout << "SPOT 1.5 - Sparta's Picture Optimizing Tool for the C64 (C) 2021-2025\n";
    cout << "*********************************************************************\n";
    cout << "\n";

    if (argc == 1)
    {
#ifdef DEBUG
        InFile = "c:/spot/Hires/gp.png";
        OutFile = "c:/spot/Hires/gp";
        CmdOptions = "mspb";
        CmdColors = "x";
        CmdMode = "h";
        VerboseMode = true;
        //OnePassMode = true;
#else
        cout << "Usage: spot input [options]\n";
        cout << "options:    -o [output path and filename without extension]\n";
        cout << "            -f [output format(s)]\n";
        cout << "            -b [background color(s)]\n";
        cout << "            -m [bitmap mod]\n";
        cout << "            -v (verbose mode)\n";
        cout << "            -s (simple/speedy mode)\n";
        cout << "\n";
        cout << "Help:  spot -h\n";
        cout << "\n";

        return EXIT_SUCCESS;
#endif
    }

    vector <string> args;
    args.resize(argc);

    for (int c = 0; c < argc; c++)
    {
        args[c] = argv[c];
    }

    int i = 1;

    while (i < argc)
    {
        if (i == 1)
        {
            InFile = args[1];

            if (InFile == "-h")
            {
                ShowHelp();
                return EXIT_SUCCESS;
            }
        }
        else if ((args[i] == "-o") || (args[i] == "-O"))        //output file base name
        {
            if (i + 1 < argc)
            {
                OutFile = args[++i];
            }
            else
            {
                cerr << "***CRITICAL***\tMissing [output] parameter.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-f") || (args[i] == "-F"))        //output type(s)
        {
            if (i + 1 < argc)
            {
                CmdOptions = args[++i];
            }
            else
            {
                cerr << "***CRITICAL***\tMissing [format] parameter.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-b") || (args[i] == "-B"))        //output background color(s)
        {
            if (i + 1 < argc)
            {
                CmdColors = args[++i];
            }
            else
            {
                cerr << "***CRITICAL***\tMissing [bgcolor] parameter.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-v") || (args[i] == "-V"))        //verbose mode
        {
            VerboseMode = true;
        }
        else if ((args[i] == "-s") || (args[i] == "-S"))        //simple/speedy, 1-pass mode
        {
            OnePassMode = true;
        }
        else if ((args[i] == "-m") || (args[i] == "-M"))        //simple/speedy, 1-pass mode
        {
            if (i + 1 < argc)
            {
                CmdMode = args[++i];

                CmdMode = tolower(CmdMode[0]);

                if (CmdMode != "m" && CmdMode != "h")
                {
                    cerr << "***CRITICAL***\tUnrecognized [bitmap mode] parameter.\n";
                    return EXIT_FAILURE;
                }
            }
            else
            {
                cerr << "***CRITICAL***\tMissing [bitmap mode] parameter.\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            cerr << "***CRITICAL***\tUnrecognized option: " << args[i] << "\n";
            return EXIT_FAILURE;
        }
        i++;
    }

    if (VerboseMode)
    {
        cout << "Verbose mode on.\n";
        if (OnePassMode)
        {
            cout << "Simple/speedy mode on.\n";
        }
    }

    for (size_t i = 0; i < CmdOptions.size(); i++)
    {
        CmdOptions[i] = tolower(CmdOptions[i]);
    }

    OutputKla = (CmdOptions.find('k') != string::npos);
    OutputMap = (CmdOptions.find('m') != string::npos);
    OutputScr = (CmdOptions.find('s') != string::npos);
    OutputCol = (CmdOptions.find('c') != string::npos);
    OutputBgc = (CmdOptions.find('g') != string::npos);
    OutputCcr = (CmdOptions.find('2') != string::npos);
    OutputObm = (CmdOptions.find('o') != string::npos);
    OutputPng = (CmdOptions.find('p') != string::npos);
    OutputBmp = (CmdOptions.find('b') != string::npos);
    OutputKlx = (CmdOptions.find('x') != string::npos);

    for (size_t i = 0; i < CmdColors.size(); i++)
    {
        CmdColors[i] = tolower(CmdColors[i]);
    }

    //Replace "\" file path separator in InFile with "/", Windows can also handle it
    while (InFile.find('\\') != string::npos)
    {
        InFile.replace(InFile.find('\\'), 1, "/");
    }

    //Same for OutFile
    while (OutFile.find('\\') != string::npos)
    {
        OutFile.replace(OutFile.find('\\'), 1, "/");
    }

    //Find input file path, name, and extension
    if (InFile.find_last_of("/.") != string::npos)
    {
        if (InFile.substr(InFile.find_last_of("/."), 1) == ".")
        {
            FExt = InFile.substr(InFile.find_last_of('.') + 1);
            if (InFile.find_last_of('/') != string::npos)
            {
                FName = InFile.substr(InFile.find_last_of('/') + 1, InFile.find_last_of('.') - InFile.find_last_of('/') - 1);
                FPath = InFile.substr(0, InFile.find_last_of('/') + 1);
            }
            else
            {
                FName = InFile.substr(0, InFile.find_last_of('.'));
                FPath = "";
            }
        }
    }
    
    //Make input file extension lower case
    for (size_t i = 0; i < FExt.size(); i++)
    {
        FExt[i] = tolower(FExt[i]);
    }

    //Check we have the right input file type
    if ((FExt != "png") && (FExt != "bmp") && (FExt != "kla") && (FExt != "koa") && (FExt != "klx"))
    {
        cerr << "***CRITICAL***\tSPOT only accepts PNG, BMP and Koala (.kla, .koa) input file types!\n";
        return EXIT_FAILURE;
    }

    //Find output file path, name and extension (extension will be omitted)
    if (!OutFile.empty())
    {
        if (OutFile.find_last_of("/.") != string::npos)
        {
            if (OutFile.substr(OutFile.find_last_of("/."), 1) == ".")
            {
                OutFile = OutFile.substr(0, OutFile.find_last_of('.'));     //Trim outfile extension
            }
            else
            {
                if (OutFile[OutFile.size() - 1] == '/')
                {
                    cout << "***INFO***\tOutput file name is missing from the output parameter.\n";
                    cout << "\t\tSPOT will use the intput file's name as output file name.\n";
                    OutFile += FName;
                }
            }
        }
    }
    else
    {
        OutFile = FPath + "spot/" + FName + "/" + FName;
    }

    if ((FExt == "kla") || (FExt == "koa") || (FExt == "klx"))
    {
        if (!ImportFromKoala())
            return EXIT_FAILURE;
    }
    else if ((FExt == "png") ||(FExt == "bmp"))
    {
        if (!ImportFromImage())
            return EXIT_FAILURE;
    }

    auto cend = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = cend - cstart;

    if (VerboseMode)
    {
        cout << "Elapsed time: " << elapsed_seconds.count() << "s\n";
    }

    cout << "Done!\n";

    return EXIT_SUCCESS;
}
