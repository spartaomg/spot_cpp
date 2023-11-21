
#include "common.h"

int PrgLen = 0;
string InFile{};
string FPath{}, FName{}, FExt{}, SpotFolder{}, SavePath{}, SaveName{}, SaveExt{}, CmdIn{}, OutFile{};

string CmdOptions = "k";                    //Default output file type = kla
string CmdColors = "0123456789abcdef";      //Default output background color: all possible colors

bool C64Formats = false;

unsigned int PicW = 0;
unsigned int PicH = 0;
int CharCol = 0;
int CharRow = 0;

int PaletteCnt = c64palettes_size / 48;

bool OutputKla = false;
bool OutputMap = false;
bool OutputCol = false;
bool OutputScr = false;
bool OutputCcr = false;
bool OutputObm = false;
bool OutputPng = false;
bool OutputBmp = false;

int NumBGCols = -1;
unsigned char BGCol, BGCols[16]{};   //Background color
unsigned char UnusedColor = 0x10;

vector <unsigned char> ImgRaw;      //raw PNG/KLA/BMP
vector <unsigned char> Image;       //pixels in RGBA format (4 bytes per pixel)
vector <unsigned char> C64Bitmap;

unsigned char MUC[16]{};

int ColTabSize = 0;
unsigned char* ColTab0, * ColTab1, * ColTab2, * ColTab3;
unsigned char* Pic, * PicMsk;       //Original picture array
unsigned char* BMP;                 //C64 bitmap array

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

//----------------------------------------------------------------------------------------------------------------------------------------------------------

string ConvertIntToHextString(const int& i, const int& hexlen)
{
    std::stringstream hexstream;
    hexstream << setfill('0') << setw(hexlen) << hex << i;
    return hexstream.str();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

int ReadBinaryFile(const string& FileName, vector<unsigned char>& prg)
{

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

    return length;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool CreateDirectory(const string& DiskDir)
{
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
        if ((DiskName[i] == '/') && (DiskDir.size() > 0) && (DiskDir != "~"))   //Don't try to create root directory and home directory
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
        return true;
    }
    else
    {
        cerr << "***CRITICAL***\tError during writing " << FileName << "\n\n";
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool WriteBinaryFile(const string& FileName, vector <unsigned char>& Binary)
{

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
        if ((DiskName[i] == '/') && (DiskDir.size() > 0) && (DiskDir != "~"))   //Don't try to create root directory and home directory
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
        myFile.write((char*)&Binary[0], Binary.size());
        return true;
    }
    else
    {
        cerr << "***CRITICAL***\tError during writing " << FileName << "\n\n";
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool SetPixel(std::vector<unsigned char>& Img,size_t X, size_t Y, color Col)
{
    size_t Pos = (Y * (size_t)PicW * 2 * 4) + (X * 4);

    if (Pos + 3 > Img.size())
        return false;

    Img[Pos + 0] = Col.R;
    Img[Pos + 1] = Col.G;
    Img[Pos + 2] = Col.B;
    Img[Pos + 3] = Col.A;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

color GetPixel(std::vector<unsigned char>& Img, size_t X, size_t Y)
{
    size_t Pos = (Y * (size_t)PicW * 2 * 4) + (X * 4);

    color Col{};

    Col.R = Img[Pos + 0];
    Col.G = Img[Pos + 1];
    Col.B = Img[Pos + 2];
    Col.A = Img[Pos + 3];

    return Col;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool SaveImgFormat()
{
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool OptimizeByColor()
{ 
    unsigned char* ColRAM, * ScrHi, * ScrLo, * ScrRAM, * ColMap;
    ScrHi = new unsigned char[ColTabSize] {};
    ScrLo = new unsigned char[ColTabSize] {};
    ScrRAM = new unsigned char[ColTabSize] {};
    ColRAM = new unsigned char[ColTabSize] {};
    ColMap = new unsigned char[16 * ColTabSize] {};         //We still have UnusedColor = 16 here, so we need an extra map!!!

    int C64Col[17]{};       //0x00-0x10 (UnusedColor = 0x10)

    for (int I = 0; I < ColTabSize; I++)
    {
        ColRAM[I] = 255;                                    //Reset color spaces
        ScrHi[I] = 255;
        ScrLo[I] = 255;

        C64Col[ColTab1[I]]++;                               //Calculate frequency of colors
        C64Col[ColTab2[I]]++;
        C64Col[ColTab3[I]]++;
        if (ColTab1[I] != UnusedColor)
        {
            ColMap[(ColTab1[I] * ColTabSize) + I] = 255;       //Create a separate color map for each color, avoiding UnusedColor
        }
        if (ColTab2[I] != UnusedColor)
        {
            ColMap[(ColTab2[I] * ColTabSize) + I] = 255;
        }
        if (ColTab3[I] != UnusedColor)
        {
            ColMap[(ColTab3[I] * ColTabSize) + I] = 255;
        }
    }

    for (int I = 0; I < 16; I++)
    {
        MUC[I] = I;                         //Initialize most used color ranklist
    }

    bool Chg = true;

    while (Chg == true)
    {
        Chg = false;
        for (int I = 0; I < 15; I++)        //Sort colors based on frequency
        {
            if (C64Col[I] < C64Col[I + 1])
            {
                int Tmp = C64Col[I];
                C64Col[I] = C64Col[I + 1];
                C64Col[I + 1] = Tmp;
                Tmp = MUC[I];
                MUC[I] = MUC[I + 1];
                MUC[I + 1] = Tmp;
                Chg = true;
            }
        }
    }

    for (int I = 0; I < ColTabSize; I++)
    {
        //Most used color goes to ScrHi
        if ((ColTab1[I] == MUC[0]) || (ColTab2[I] == MUC[0]) || (ColTab3[I] == MUC[0]))
        {
            ScrHi[I] = MUC[0];
        }
        //Second most used color goes to ScrLo
        if ((ColTab1[I] == MUC[1]) || (ColTab2[I] == MUC[1]) || (ColTab3[I] == MUC[1]))
        {
            ScrLo[I] = MUC[1];
        }
        //Third most used color goes to ColRAM
        if ((ColTab1[I] == MUC[2]) || (ColTab2[I] == MUC[2]) || (ColTab3[I] == MUC[2]))
        {
            ColRAM[I] = MUC[2];
        }

    }

    for (int J = 3; J < 15; J++)
    {
        int OverlapSH = 0;      //Overlap with ScrHi
        int OverlapSL = 0;      //Overlap with ScrLo
        int OverlapCR = 0;      //Overlap with ColRAM

        //Calculate overlap with the color spaces separately for each color
        for (int I = 0; I < ColTabSize; I++)
        {
            if (ColMap[(MUC[J] * ColTabSize) + I] == 255)
            {
                if (ScrHi[I] != 255)
                {
                    OverlapSH++;
                }
                
                if (ScrLo[I] != 255)           //IS ELSE IF IN VB CODE!!! BUT THIS RESULTS IN A LITTLE BIT BETTER COMPRESSIBILITY
                {
                    OverlapSL++;
                }
                
                if (ColRAM[I] != 255)          //IS ELSE IF IN VB CODE!!! BUT THIS RESULTS IN A LITTLE BIT BETTER COMPRESSIBILITY
                {
                    OverlapCR++;
                }
            }
        }

        for (int I = 0; I < ColTabSize; I++)
        {
            if (ColMap[(MUC[J] * ColTabSize) + I] == 255)
            {
                if ((OverlapSH <= OverlapSL) && (OverlapSH <= OverlapCR))       //OverlapSH is smallest
                {
                    if (OverlapSL <= OverlapCR)                                 //OverlapSL is second smallest
                    {   
                        if (ScrHi[I] == 255)
                        {
                            ScrHi[I] = MUC[J];
                        }
                        else if (ScrLo[I] == 255)
                        {
                            ScrLo[I] = MUC[J];
                        }
                        else
                        {
                            ColRAM[I] = MUC[J];
                        }
                    }
                    else
                    {
                        if (ScrHi[I] == 255)                                    //OverlapCR is second smallest
                        {
                            ScrHi[I] = MUC[J];
                        }
                        else if (ColRAM[I] == 255)
                        {
                            ColRAM[I] = MUC[J];
                        }
                        else
                        {
                            ScrLo[I] = MUC[J];
                        }
                    }
                }
                else if ((OverlapSL <= OverlapSH) && (OverlapSL <= OverlapCR))  //OverlapSL is smallest
                {
                    if (OverlapSH <= OverlapCR)                                 //OverlapHi is second smallest
                    {
                        if (ScrLo[I] == 255)
                        {
                            ScrLo[I] = MUC[J];
                        }
                        else if (ScrHi[I] == 255)
                        {
                            ScrHi[I] = MUC[J];
                        }
                        else
                        {
                            ColRAM[I] = MUC[J];
                        }
                    }
                    else
                    {
                        if (ScrLo[I] == 255)                                    //OverlapCR is second smallest
                        {
                            ScrLo[I] = MUC[J];
                        }
                        else if (ColRAM[I] == 255)
                        {
                            ColRAM[I] = MUC[J];
                        }
                        else
                        {
                            ScrHi[I] = MUC[J];
                        }
                    }
                }
                else
                {                                                               //OverlapCR is smallest
                    if (OverlapSH <= OverlapSL)                                 //OverlapSH is second smallest
                    {
                        if (ColRAM[I] == 255)
                        {
                            ColRAM[I] = MUC[J];
                        }
                        else if (ScrHi[I] == 255)
                        {
                            ScrHi[I] = MUC[J];
                        }
                        else
                        {
                            ScrLo[I] = MUC[J];
                        }
                    }
                    else
                    {
                        if (ColRAM[I] == 255)                                    //OverlapSL is second smallest
                        {
                            ColRAM[I] = MUC[J];
                        }
                        else if (ScrLo[I] == 255)
                        {
                            ScrLo[I] = MUC[J];
                        }
                        else
                        {
                            ScrHi[I] = MUC[J];
                        }
                    }
                }
            }
        }
    }

    //----------------------------------------------------------------------------

    for (int I = 1; I < ColTabSize - 1; I++)
    {
        if ((ScrHi[I] != ScrHi[I - 1]) && (ScrHi[I] != ScrHi[I + 1]) && (ScrHi[I] != 255) && (ScrHi[I - 1] != 255) && (ScrHi[I + 1] != 255))
        {
            if (ScrLo[I] == 255)
            {
                ScrLo[I] = ScrHi[I];
                ScrHi[I] = 255;
            }
            else if (ColRAM[I] == 255)
            {
                ColRAM[I] = ScrHi[I];
                ScrHi[I] = 255;
            }
        }

        if ((ScrLo[I] != ScrLo[I - 1]) && (ScrLo[I] != ScrLo[I + 1]) && (ScrLo[I] != 255) && (ScrLo[I - 1] != 255) && (ScrLo[I + 1] != 255))
        {
            if (ColRAM[I] == 255)
            {
                ColRAM[I] = ScrLo[I];
                ScrLo[I] = 255;
            }
            else if (ScrHi[I] == 255)
            {
                ScrHi[I] = ScrLo[I];
                ScrLo[I] = 255;
            }
        }
        if ((ColRAM[I] != ColRAM[I - 1]) && (ColRAM[I] != ColRAM[I + 1]) && (ColRAM[I] != 255) && (ColRAM[I - 1] != 255) && (ColRAM[I + 1] != 255))
        {
            if (ScrHi[I] == 255)
            {
                ScrHi[I] = ColRAM[I];
                ColRAM[I] = 255;
            }
            else if (ScrLo[I] == 255)
            {
                ScrLo[I] = ColRAM[I];
                ColRAM[I] = 255;
            }
        }
    }

    //----------------------------------------------------------------------------

    //Find loner bytes that can be swapped
    for (int I = 1; I < ColTabSize - 1; I++)
    {
        if ((ScrHi[I] != ScrHi[I - 1]) && (ScrHi[I] != ScrHi[I + 1]) && (ScrHi[I] != 255))
        {
            if (ScrHi[I] == ScrLo[I + 1])
            {
                if ((ScrLo[I] != ScrLo[I - 1]))
                {
                    ScrHi[I] = ScrLo[I];
                    ScrLo[I] = ScrLo[I + 1];
                }
            }
            else if (ScrHi[I] == ScrLo[I - 1])
            {
                if ((ScrLo[I] != ScrLo[I + 1]))
                {
                    ScrHi[I] = ScrLo[I];
                    ScrLo[I] = ScrLo[I - 1];
                }
            }
            else if (ScrHi[I] == ColRAM[I + 1])
            {
                if ((ColRAM[I] != ColRAM[I - 1]))
                {
                    ScrHi[I] = ColRAM[I];
                    ColRAM[I] = ColRAM[I + 1];
                }
            }
            else if (ScrHi[I] == ColRAM[I - 1])
            {
                if ((ColRAM[I] != ColRAM[I + 1]))
                {
                    ScrHi[I] = ColRAM[I];
                    ColRAM[I] = ColRAM[I - 1];
                }
            }
        }

        if ((ScrLo[I] != ScrLo[I - 1]) && (ScrLo[I] != ScrLo[I + 1]) && (ScrLo[I] != 255))
        {
            if (ScrLo[I] == ScrHi[I + 1])
            {
                if ((ScrHi[I] != ScrHi[I - 1]))
                {
                    ScrLo[I] = ScrHi[I];
                    ScrHi[I] = ScrHi[I + 1];
                }
            }
            else if (ScrLo[I] == ScrHi[I - 1])
            {
                if ((ScrHi[I] != ScrHi[I + 1]))
                {
                    ScrLo[I] = ScrHi[I];
                    ScrHi[I] = ScrHi[I - 1];
                }
            }
            else if (ScrLo[I] == ColRAM[I + 1])
            {
                if ((ColRAM[I] != ColRAM[I - 1]))
                {
                    ScrLo[I] = ColRAM[I];
                    ColRAM[I] = ColRAM[I + 1];
                }
            }
            else if (ScrLo[I] == ColRAM[I - 1])
            {
                if ((ColRAM[I] != ColRAM[I + 1]))
                {
                    ScrLo[I] = ColRAM[I];
                    ColRAM[I] = ColRAM[I - 1];
                }
            }
        }

        if ((ColRAM[I] != ColRAM[I - 1]) && (ColRAM[I] != ColRAM[I + 1]) && (ColRAM[I] != 255))
        {
            if (ColRAM[I] == ScrLo[I + 1])
            {
                if ((ScrLo[I] != ScrLo[I - 1]))
                {
                    ColRAM[I] = ScrLo[I];
                    ScrLo[I] = ScrLo[I + 1];
                }
            }
            else if (ColRAM[I] == ScrLo[I - 1])
            {
                if ((ScrLo[I] != ScrLo[I + 1]))
                {
                    ColRAM[I] = ScrLo[I];
                    ScrLo[I] = ScrLo[I - 1];
                }
            }
            else if (ColRAM[I] == ScrHi[I + 1])
            {
                if ((ScrHi[I] != ScrHi[I - 1]))
                {
                    ColRAM[I] = ScrHi[I];
                    ScrHi[I] = ScrHi[I + 1];
                }
            }
            else if (ColRAM[I] == ScrHi[I - 1])
            {
                if ((ScrHi[I] != ScrHi[I + 1]))
                {
                    ColRAM[I] = ScrHi[I];
                    ScrHi[I] = ScrHi[I - 1];
                }
            }
        }
    }

    if (ColRAM[0] == 255)
    {
        ColRAM[0] = 0;      //In case the whole array remained unused

        for (int I = 1; I < ColTabSize; I++)
        {
            if (ColRAM[I] != 255)
            {
                ColRAM[0] = ColRAM[I];
                    break;
            }
        }
    }

    if (ScrHi[0] == 255)
    {
        ScrHi[0] = 0;      //In case the whole array remained unused

        for (int I = 1; I < ColTabSize; I++)
        {
            if (ScrHi[I] != 255)
            {
                ScrHi[0] = ScrHi[I];
                break;
            }
        }
    }

    if (ScrLo[0] == 255)
    {
        ScrLo[0] = 0;      //In case the whole array remained unused

        for (int I = 1; I < ColTabSize; I++)
        {
            if (ScrLo[I] != 255)
            {
                ScrLo[0] = ScrLo[I];
                break;
            }
        }
    }

    for (int I = 1; I < ColTabSize; I++)
    {
        if (ScrHi[I] == 255)
        {
            ScrHi[I] = ScrHi[I - 1];
        }
        if ((ScrLo[I] == ScrHi[I]) || (ScrLo[I] == 255))
        {
            ScrLo[I] = ScrLo[I - 1];
        }

        if ((ColRAM[I] == ScrHi[I]) || (ColRAM[I] == ScrLo[I]) || (ColRAM[I] == 255))
        {
            ColRAM[I] = ColRAM[I - 1];
        }
    }

    //Combine screen RAM hi and low nibbles
    for (int I = 0; I < ColTabSize; I++)
    {
        unsigned char Tmp = ColRAM[I];
        ColRAM[I] = ScrLo[I];                   //Swap ColRAM with ScrLo - this results in improved compressibility
        ScrLo[I] = Tmp;
        ScrRAM[I] = (ScrHi[I] * 16) + ScrLo[I];
    }

    //----------------------------------------------------------------------------
    //Rebuild the image
    //----------------------------------------------------------------------------

    unsigned char Col1{}, Col2{}, Col3{};
    int CP{};               //Char Position within array
    unsigned char V{};      //One byte to work with...

    //Replace C64 colors with respective bit pairs
    for (int CY = 0; CY < CharRow; CY++)
    {
        for (int CX = 0; CX < CharCol; CX++)
        {
            int CharIndex = (CY * CharCol) + CX;
            Col1 = ScrHi[CharIndex];        //Fetch colors from tabs
            Col2 = ScrLo[CharIndex];
            Col3 = ColRAM[CharIndex];
            for (int BY = 0; BY < 8; BY++)
            {
                for (int BX = 0; BX < 4; BX++)
                {
                    //Calculate pixel position in array
                    CP = (CY * PicW * 8) + (CX * 4) + (BY * PicW) + BX;
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
        for(int CX = 0; CX < CharCol; CX++)
        {
            for (int BY = 0; BY < 8; BY++)
            {
                CP = (CY * PicW * 8) + (CX * 4) + (BY * PicW);
                V = (PicMsk[CP] * 64) + (PicMsk[CP + 1] * 16) + (PicMsk[CP + 2] * 4) + PicMsk[CP + 3];
                CP = (CY * CharCol * 8) + (CX * 8) + BY;
                BMP[CP] = V;

            }
        }
    }

    string SaveFile = SavePath + SaveName; 
    if ((NumBGCols > 1) && (CmdColors.size() != 1))
    {
        //If we have more than 1 possible background color AND the user requested more than one background color
        //Then mark the output file name with _0x
        SaveFile += "_"+ ConvertIntToHextString((int)BGCol, 2);
    }

    if ((OutputKla) && (CharRow >= 25) && (CharCol >= 40))
    {
        //Save Koala only if bitmap is at least 320x200 pixels
        vector <unsigned char> KLA;
        KLA.resize(10003);
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
                KLA[9002 + (CY * 40) + CX] = ColRAM[((StartCY + CY) * CharCol) + StartCX + CX];
            }
        }
        //Save KLA
        WriteBinaryFile(SaveFile + ".kla", KLA);
    }

    //Save bitmap, color RAM, and screen RAM

    if (OutputMap)
    {
        WriteBinaryFile(SaveFile + ".map", BMP, CharCol*PicH);
    }

    if (OutputCol)
    {
        WriteBinaryFile(SaveFile + ".col", ColRAM, ColTabSize);
    }

    if (OutputScr)
    {
        WriteBinaryFile(SaveFile + ".scr", ScrRAM, ColTabSize);
    }

    unsigned char* CCR{};
    CCR = new unsigned char[ColTabSize / 2] {};

    for(int I = 0; I < ColTabSize / 2; I++)
    {
        CCR[I] = ((ColRAM[I * 2] % 16) * 16) + (ColRAM[(I * 2) + 1] % 16);
    }

    //Save compressed ColorRAM with halfbytes combined

    if (OutputCcr)
    {
        WriteBinaryFile(SaveFile + ".ccr", CCR, ColTabSize/2);
    }

    //Save Optimized Bitmap file format only if bitmap is at least 320x200 pixels
    //Bitmap is stored column wise, color spaces are stored row wise, color RAM is compressed

    if ((OutputObm) && (CharRow >= 25) && (CharCol >= 40))
    {
        vector <unsigned char> OBM;
        OBM.resize(9503);
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

    delete[] ColRAM;
    delete[] ScrLo;
    delete[] ScrHi;
    delete[] ScrRAM;
    delete[] ColMap;
    delete[] CCR;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool OptimizeImage()
{
    bool ReturnStatus = true;

    SaveImgFormat();

    if ((OutputKla) || (OutputMap) || (OutputCol) || (OutputScr) || (OutputCcr) || (OutputObm))
    {
        if ((PicW % 4 != 0) || (PicH % 8 != 0))
        {
            cerr << "***CRITICAL***\tUnable to convert image to C64 formats. The dimensions of the image must be multiples of 8.\n";
            return false;       //This return is OK, we are before creating dynamic arrays
        }
    }

    Pic = new unsigned char[PicW * PicH] {};
    PicMsk = new unsigned char[PicW * PicH] {};

    ColTabSize = CharCol * CharRow;

    unsigned char* CT0,* CT1,* CT2,* CT3;
    CT0 = new unsigned char[ColTabSize] {};
    CT1 = new unsigned char[ColTabSize] {};
    CT2 = new unsigned char[ColTabSize] {};
    CT3 = new unsigned char[ColTabSize] {};

    ColTab0 = new unsigned char[ColTabSize] {};
    ColTab1 = new unsigned char[ColTabSize] {};
    ColTab2 = new unsigned char[ColTabSize] {};
    ColTab3 = new unsigned char[ColTabSize] {};

    BMP = new unsigned char[CharCol * PicH] {};

    //Fetch R component of the RGB color code for each pixel, convert it to C64 palette, and save it to Pic array
    for (size_t Y = 0; Y < PicH; Y++)
    {
        for (size_t X = 0; X < PicW; X++)
        {
            Pic[(Y * PicW) + X] = paletteconvtab[GetPixel(C64Bitmap, X * 2, Y).R];
        }
    }

    //Fill all three Color Tabs with a value that is not used by either the image or the C64
    for (int i = 0; i < ColTabSize; i++)
    {
        CT0[i] = UnusedColor;
        CT1[i] = UnusedColor;
        CT2[i] = UnusedColor;
        CT3[i] = UnusedColor;
    }

    //Sort R values into 3 Color Tabs per char
    for (int CY = 0; CY < CharRow; CY++)                //Char rows
    {
        for (int CX = 0; CX < CharCol; CX++)            //Chars per row
        {
            int CP = (CY * 8 * PicW) + (CX * 4);        //Char's position within R array(160 * 1600) = Y * 160 * 8 + X * 4

            for (int BY = 0; BY < 8; BY++)              //Pixel's Y-position within char
            {
                for (int BX = 0; BX < 4; BX++)          //Pixel's X-position within char
                {
                    int PicPos = CP + (BY * PicW) + BX;
                    unsigned char V = Pic[PicPos];      //Fetch R value of pixel in char

                    int CharIndex = (CY * CharCol) + CX;

                    if ((CT0[CharIndex] == V) || (CT1[CharIndex] == V) || (CT2[CharIndex] == V) || (CT3[CharIndex] == V))
                    {
                        //Color can only be stored once
                    }
                    else if (CT1[CharIndex] == UnusedColor)
                    {
                        //If color is not in Color Tabs yet, first fill Color Tab   
                        CT1[CharIndex] = V;
                    }
                    else if (CT2[CharIndex] == UnusedColor)
                    {
                        //Then try Color Tab 2
                        CT2[CharIndex] = V;
                    }
                    else if (CT3[CharIndex] == UnusedColor)
                    {
                        //Then try Color Tab 3
                        CT3[CharIndex] = V;
                    }
                    else if (CT0[CharIndex] == UnusedColor)
                    {
                        //Finally try Color Tab 0
                        CT0[CharIndex] = V;
                    }
                    else
                    {
                        cerr << "***CRITICAL***\tThis picture cannot be converted as it contains more than 4 colors per char block!\n";
                        ReturnStatus = false;
                    }
                }
            }
        }
    }

    if (ReturnStatus)
    {
        NumBGCols = 0;

        for (int C = 0; C < 16; C++)
        {
            bool ColOK = true;
            for (int I = 0; I < ColTabSize; I++)
            {
                int ColCnt = 0;
                if ((CT0[I] != C) && (CT0[I] != UnusedColor)) ColCnt++;
                if ((CT1[I] != C) && (CT1[I] != UnusedColor)) ColCnt++;
                if ((CT2[I] != C) && (CT2[I] != UnusedColor)) ColCnt++;
                if ((CT3[I] != C) && (CT3[I] != UnusedColor)) ColCnt++;
                if (ColCnt == 4)
                {
                    ColOK = false;
                    break;
                }
            }
            if (ColOK)
            {
                BGCols[NumBGCols++] = C;
            }
        }

        if (NumBGCols == 0)
        {
            cerr << "***CRITICAL***\tThis picture cannot be converted as none of the C64 colors can be used as a background color!\n";
            ReturnStatus = false;
        }
        
        if ((NumBGCols > 1) && (CmdColors.size() != 1))
        {
            cout << "***INFO***\tMore than one possible background color has been identified.\n";
            cout << "***INFO***\tThe background color will be appened to the output file names.\n";
            cout << "***INFO***\tIf you only want one output background color then please specify it in the command-line.\n";
        }

        bool ColFound = false;
        if (NumBGCols > 0)
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
                cerr << "***CRITICAL***This picture cannot be converted as the requested color(s) cannot be used as a background color!\n";
                ReturnStatus = false;
            }
        }

        if (ReturnStatus)
        {
            //Optimize bitmap with all possible background color

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
                if (CmdColors.find(cCol) != string::npos)
                {
                    for (int I = 0; I < ColTabSize; I++)
                    {
                        ColTab0[I] = CT0[I];
                        ColTab1[I] = CT1[I];
                        ColTab2[I] = CT2[I];
                        ColTab3[I] = CT3[I];
                        if (ColTab1[I] == BGCol)
                        {
                            ColTab1[I] = ColTab0[I];
                            ColTab0[I] = UnusedColor;

                        }
                        else if (ColTab2[I] == BGCol)
                        {
                            ColTab2[I] = ColTab0[I];
                            ColTab0[I] = UnusedColor;
                        }
                        else if (ColTab3[I] == BGCol)
                        {
                            ColTab3[I] = ColTab0[I];
                            ColTab0[I] = UnusedColor;
                        }
                    }

                    //ColTab0 can only contain UnusedColor and BGCol here!!! - we won't need to use it in OptimizeByColor()

                    //OptimizeByColorSequence()
                    OptimizeByColor();
                }
            }
        }
    }

    delete[] Pic;
    delete[] PicMsk;
    delete[] CT0;
    delete[] CT1;
    delete[] CT2;
    delete[] CT3;
    delete[] ColTab0;
    delete[] ColTab1;
    delete[] ColTab2;
    delete[] ColTab3;
    delete[] BMP;

    return ReturnStatus;  
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool ConvertPicToC64Palette()
{
    CharCol = PicW / 4;
    CharRow = PicH / 8;

    if (OutputKla)
    {
        if ((CharCol < 40) || (CharRow < 25))
        {
            cout << "***INFO***\tThis image cannot be saved as Koala (KLA) or Optimized Bitmap (OBM) as it is smaller than 320x200 pixels!\n";
            cout << "***INFO***\tAll other selected output format will be created.\n";
        }
    }

    unsigned char R{}, G{}, B{};
    color PicCol{};
    int dR{}, dG{}, dB{}, uR{};
    int BestMatchIndex{};
    double BestMatch{};
    int BestPaletteIndex{};

    int* BestPalette;
    BestPalette = new int[PaletteCnt] {};

    C64Bitmap.resize((size_t)PicW * 2 * PicH * 4);
    
    //First check if there is a direct palette match
    bool PaletteMatch = false;
    for (int P = 0; P < PaletteCnt; P++)
    {
        for (size_t Y = 0; Y < PicH; Y++)
        {
            for (size_t X = 0; X < PicW; X++)
            {
                PaletteMatch = false;
                PicCol = GetPixel(Image, X * 2, Y);
                for (int J = 0; J < 16; J++)
                {
                    if ((c64palettes[(48 * P) + J] == PicCol.R) && (c64palettes[(48 * P) + 16 + J] == PicCol.G) && (c64palettes[(48 * P) + 32 + J] == PicCol.B))
                    {
                        //Use default palette

                        color C64Color{ c64palettes[J], c64palettes[J + 16], c64palettes[J + 32] ,0};

                        SetPixel(C64Bitmap, X * 2, Y, C64Color);
                        SetPixel(C64Bitmap, (X * 2) + 1, Y, C64Color);

                        PaletteMatch = true;
                        break;
                    }
                }
                if (!PaletteMatch)
                    break;
            }
            if (!PaletteMatch)
                break;
        }
        if (PaletteMatch)
            break;
    }

    if (!PaletteMatch)
    {
        //No direct match, identify the best matching C64 palette
        for (size_t Y = 0; Y < PicH; Y++)
        {
            for (size_t X = 0; X < PicW; X++)
            {
                R = GetPixel(Image, X * 2, Y).R;
                G = GetPixel(Image, X * 2, Y).G;
                B = GetPixel(Image, X * 2, Y).B;

                BestMatch = 0x10000000;

                for (int P = 0; P < PaletteCnt; P++)
                {
                    for (int J = 0; J < 16; J++)
                    {
                        uR = (R + c64palettes[(P * 48) + J]) / 2;
                        dR = R - c64palettes[(P * 48) + J];
                        dG = G - c64palettes[(P * 48) + J + 16];
                        dB = B - c64palettes[(P * 48) + J + 32];
                        double cDiff = (((512 + uR) * dR * dR) >> 8) + (4 * dG * dG) + (((767 - uR) * dB * dB) >> 8);

                        if (cDiff < BestMatch)
                        {
                            BestMatch = cDiff;
                            BestMatchIndex = J;
                            BestPaletteIndex = P;
                        }
                    }
                }
                BestPalette[BestPaletteIndex]++;
            }
        }

        BestPaletteIndex = -1;
        int BP = 0;
        for (int I = 0; I < PaletteCnt; I++)
        {
            if (BestPalette[I] > BP)
            {
                BP = BestPalette[I];
                BestPaletteIndex = I;
            }
        }

        //Now use the best matching palette to match colors
        for (size_t Y = 0; Y < PicH; Y++)
        {
            for (size_t X = 0; X < PicW; X++)
            {
                R = GetPixel(Image, X * 2, Y).R;
                G = GetPixel(Image, X * 2, Y).G;
                B = GetPixel(Image, X * 2, Y).B;

                BestMatch = 0x10000000;

                for (int J = 0; J < 16; J++)
                {
                    uR = (R + c64palettes[(BestPaletteIndex * 48) + J]) / 2;
                    dR = R - c64palettes[(BestPaletteIndex * 48) + J];
                    dG = G - c64palettes[(BestPaletteIndex * 48) + J + 16];
                    dB = B - c64palettes[(BestPaletteIndex * 48) + J + 32];
                    double cDiff = (((512 + uR) * dR * dR) >> 8) + (4 * dG * dG) + (((767 - uR) * dB * dB) >> 8);

                    if (cDiff < BestMatch)
                    {
                        BestMatch = cDiff;
                        BestMatchIndex = J;
                    }
                }

                //Use default palette
                color C64Color{ c64palettes[BestMatchIndex], c64palettes[BestMatchIndex + 16], c64palettes[BestMatchIndex + 32], 0 };

                SetPixel(C64Bitmap, X * 2, Y, C64Color);
                SetPixel(C64Bitmap, (X * 2) + 1, Y, C64Color);
            }
        }
    }
    
    delete[] BestPalette;

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
    size_t DataOffset = (size_t)ImgRaw[DATA_OFFSET] + (size_t)(ImgRaw[DATA_OFFSET + 1] * 0x100) + (size_t)(ImgRaw[DATA_OFFSET + 2] * 0x10000) + (size_t)(ImgRaw[DATA_OFFSET + 3] * 01000000);

    //Calculate length of pixel rows in bytes
    size_t RowLen = ((size_t)BmpInfo->bmiHeader.biWidth * (size_t)BmpInfo->bmiHeader.biBitCount) / 8;

    //BMP pads pixel rows to a multiple of 4 in bytes
    size_t PaddedRowLen = (RowLen % 4) == 0 ? RowLen : RowLen - (RowLen % 4) + 4;
    //size_t PaddedRowLen = ((RowLen + 3) / 4) * 4;

    size_t CalcSize = DataOffset + (BmpInfo->bmiHeader.biHeight * PaddedRowLen);

    if (ImgRaw.size() != CalcSize)
    {
        cerr << "***CRITICAL***\tCorrupted BMP file size.\n";
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

            for (size_t X = 0; X < RowLen; X++)                         //Pixel row are read left to right
            {
                unsigned int Pixel = ImgRaw[RowOffset + X];

                for (int b = bstart; b >= 0; b -= BitsPerPx)
                {
                    int PaletteIndex = (Pixel >> b) % mod;

                    Image[BmpOffset + 0] = BmpInfo->bmiColors[PaletteIndex].rgbRed;
                    Image[BmpOffset + 1] = BmpInfo->bmiColors[PaletteIndex].rgbGreen;
                    Image[BmpOffset + 2] = BmpInfo->bmiColors[PaletteIndex].rgbBlue;
                    BmpOffset += 4;
                }
            }
        }
    }
    else
    {
        int BytesPerPx = BmpInfo->bmiHeader.biBitCount / 8;    //24 Bits/pixel: 3; 32 Bits/pixel: 4

        for (int y = (BmpInfo->bmiHeader.biHeight - 1); y >= 0; y--)    //Pixel rows are read from last bitmap row to first
        {
            size_t RowOffset = DataOffset + (y * PaddedRowLen);

            for (size_t X = 0; X < RowLen; X += BytesPerPx)              //Pixel row are read left to right
            {
                Image[BmpOffset + 0] = ImgRaw[RowOffset + X + 2];
                Image[BmpOffset + 1] = ImgRaw[RowOffset + X + 1];
                Image[BmpOffset + 2] = ImgRaw[RowOffset + X + 0];
                BmpOffset += 4;
            }
        }
    }

    PicW = BmpInfo->bmiHeader.biWidth / 2;      //Double pixels, effective width is half of BMP width
    PicH = BmpInfo->bmiHeader.biHeight;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool IsMCImage()
{
    for (size_t Y = 0; Y < PicH; Y++)
    {
        for (size_t X = 0; X < PicW; X++)
        {
            color Col1 = GetPixel(Image, X * 2, Y);
            color Col2 = GetPixel(Image, (X * 2) + 1, Y);
            if ((Col1.R != Col2.R) || (Col1.G != Col2.G) || (Col1.B != Col2.B))
            {
                cerr << "***CRITICAL***SPOT only accepts multicolor (double-pixel) pictures as input. This image file cannot be converted!\n";
                return false;
            }
        }
    }
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
        PicW /= 2;  //Double pixels, effective width is half of PNG width

    }
    else if (FExt == "bmp")
    {
        if (!DecodeBmp())
        {
            return false;
        }
    }
    

    //Make sure this is a MC (double pixel) picture
    if (!IsMCImage())
        return false;

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
        cerr << "***CRITICAL***\tUnable to open Koala file.\n";
        return false;
    }
    else if (PrgLen != 10003)
    {
        cerr << "***CRITICAL***\tInvalid Koala file size. Unable optimize this Koala file\n";
        return false;
    }

    PicW = 160;         //Double pixels, this is the effective width
    PicH = 200;
    CharCol = PicW / 4;
    CharRow = PicH / 8;

    ColTab1 = new unsigned char[CharCol * CharRow] {};
    ColTab2 = new unsigned char[CharCol * CharRow] {};
    ColTab3 = new unsigned char[CharCol * CharRow] {};

    for (size_t i = 0; i < 1000; i++)
    {
        ColTab1[i] = ImgRaw[8002 + i] / 16;
        ColTab2[i] = ImgRaw[8002 + i] % 16;
        ColTab3[i] = ImgRaw[9002 + i] % 16;
    }

    for (int i = 1; i < 16; i++)
    {
        BGCols[i] = 0xff;
    }
    BGCols[0] = ImgRaw[10002];
    BGCol = BGCols[0];

    C64Bitmap.resize((size_t)PicW * 2 * PicH * 4);

    Image.resize((size_t)PicW * 2 * PicH * 4);

    int CI = 0;         //Color tab index
    int PxI = 0;        //Pixel index
    unsigned char BitMask = 0;
    unsigned char Bits = 0;
    unsigned char Col = 0;

    for (size_t Y= 0; Y< PicH; Y++)
    {
        for (size_t X = 0; X < PicW; X++)
        {
            CI = ((Y/ 8) * CharCol) + (X / 4);                      //ColorTab index from X and Y
            PxI = ((X / 4) * 8) + (Y % 8) + ((Y/ 8) * PicW * 2);    //Pixel index in Bitmap

            if (X % 4 == 0)
            {
                BitMask = 0xc0;
            }
            else if (X % 4 == 1)
            {
                BitMask = 0x30;
            }
            else if (X % 4 == 2)
            {
                BitMask = 0x0c;
            }
            else if (X % 4 == 3)
            {
                BitMask = 0x03;
            }
            
            Bits = (ImgRaw[(size_t)2 + PxI] & BitMask);
            
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

            color C64Color{ c64palettes[Col], c64palettes[Col + 16], c64palettes[Col + 32],0 };

            SetPixel(Image, 2 * X, Y, C64Color);
            SetPixel(Image, (2 * X) + 1, Y, C64Color);
            SetPixel(C64Bitmap, 2 * X, Y, C64Color);
            SetPixel(C64Bitmap, (2 * X) + 1, Y, C64Color);
        }
    }

    delete[] ColTab1;
    delete[] ColTab2;
    delete[] ColTab3;

    return OptimizeImage();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void ShowHelp()
{
    cout << "SPOT is a small cross-platform command-line tool that converts .png, .bmp, and .kla images into C64 file formats\n";
    cout << "optimized for better compression. The following output file formats can be selected: koala (.kla), bitmap (.map),\n";
    cout << "screen RAM (.scr), color RAM (.col), compressed color RAM (.ccr)*, and optimized bitmap (.obm)**.\n\n";
    cout << "*Compressed color RAM (.ccr) format: two adjacent half bytes are combined to reduce the size of the color RAM to\n";
    cout << "500 bytes.\n\n";
    cout << "**Optimized bitmap (.obm) format: bitmap info is stored column wise. Screen RAM and compressed color RAM stored\n";
    cout << "row wise. First two bytes are address bytes ($00, $60) and the last one is the background color as in the Koala format.\n";
    cout << "File size: 9503 bytes. In most cases, this format compresses somewhat better than koala but it also needs a more\n";
    cout << "complex display routine.\n\n";
    cout << "Usage\n";
    cout << "-----\n\n";
    cout << "spot input [output] [format] [bgcolor]\n\n";
    cout << "input:   An input image file to be optimized/converted. Only .png, .bmp, and .kla file types are accepted.\n\n";
    cout << "output:  The output folder and file name. File extension (if exists) will be ignored. If omitted, SPOT will create\n";
    cout << "         a <spot/input> folder and the input file's name will be used as output file name.\n\n";
    cout << "format:  Output file formats. Select as many as you want in any order:\n";
    cout << "         k - .kla (Koala - 10003 bytes)\n";
    cout << "         m - .map (bitmap data)\n";
    cout << "         s - .scr (screen RAM data)\n";
    cout << "         c - .col (color RAM data)\n";
    cout << "         2 - .ccr (compressed color RAM data)\n";
    cout << "         o - .obm (optimized bitmap - 9503 bytes)\n";
    cout << "         If omitted, then the default Koala file will be created.\n\n";
    cout << "bgcolor: Output background color(s): 0123456789abcdef. SPOT will only create C64 files using the selected\n";
    cout << "         background color(s). If omitted, SPOT will generate output files using all possible background colors.\n";
    cout << "         If more than one background color is possible (and allowed) then SPOT will append the background color\n";
    cout << "         to the output file name.\n\n";
    cout << "The last three arguments can be omitted, but each one is dependent on the one on its left. In other words, the\n";
    cout << "second argument is always interpreted as [output]. Therefore, if one omits the [output] argument, then the [format]\n";
    cout << "and [bgcolor] arguments must be omitted too.\n\n";
    cout << "Examples\n";
    cout << "--------\n\n";
    cout << "spot picture.bmp newfolder/newfile msc 0\n";
    cout << "SPOT will convert <picture.bmp> to .map, .scr, and .col formats with black as background color and will save them to\n";
    cout << "the <newfolder> folder using <newfile> as output base filename.\n\n";
    cout << "spot picture.png newfolder/newfile msc\n";
    cout << "SPOT will convert <picture.png> to .map, .scr, and .col formats with all possible background colors and will save them\n";
    cout << "to the <newfolder> folder using <newfile> as output base filename.\n\n";
    cout << "spot picture.png newfolder/newfile\n";
    cout << "SPOT will convert <picture.png> to the default Koala format with all possible background colors and will save the\n";
    cout << "output to the <newfolder> folder using <newfile> as output base filename.\n\n";
    cout << "spot picture.png\n";
    cout << "SPOT will convert <picture.png> to the default Koala format with all possible background colors and will save the\n";
    cout << "output to the <spot/picture> folder using <picture> as output base filename\n\n";
    cout << "Notes\n";
    cout << "-----\n\n";
    cout << "SPOT recognizes several C64 palettes. If a palette match is not found then it attempts to convert colors to\n";
    cout << "a standard C64 palette.\n\n";
    cout << "SPOT can handle non-standard image sizes (such as the vertical bitmap in Memento Mori and the diagonal bitmap\n";
    cout << "in Christmas Megademo). When a .kla or .obm file is created from a non-standard sized image, SPOT takes a centered\n";
    cout << "\"snapshot\" of the image and saves that as .kla or .obm. Map, screen RAM, and color RAM files can be of any size.\n\n";
    cout << "SPOT is meant to convert and optimize multicolor bitmaps (hi-res images get converted to multicolor).\n\n";
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    cout << "\n";
    cout << "*********************************************************************\n";
    cout << "SPOT 1.2 - Sparta's Picture Optimizing Tool for the C64 (C) 2021-2023\n";
    cout << "*********************************************************************\n";
    cout << "\n";

    if (argc == 1)
    {
        cout << "Usage: spot input [output] [kmsc2o] [0123456789abcdef]\n";
        cout << "\n";
        cout << "Help:  spot -?\n";
        cout << "\n";

        return EXIT_SUCCESS;
        //InFile = "bin\\windows\\d.png";
        //OutFile = "bin\\windows\\d\\d";
        //CmdColors = "0";
    }
    
    //Input
    if (argc > 1)
    {
        InFile = argv[1];

        if (InFile == "-?")
        {
            ShowHelp();
            return EXIT_SUCCESS;
        }
    }
    //Output
    if (argc > 2)
    {
        OutFile = argv[2];
    }

    //Format
    if (argc > 3)
    {
        CmdOptions = argv[3];
    }

    //BGColor
    if (argc > 4)
    {
        CmdColors = argv[4];
    }

    for (size_t i = 0; i < CmdOptions.size(); i++)
    {
        CmdOptions[i] = tolower(CmdOptions[i]);
    }

    OutputKla = (CmdOptions.find('k') != string::npos);
    OutputMap = (CmdOptions.find('m') != string::npos);
    OutputScr = (CmdOptions.find('s') != string::npos);
    OutputCol = (CmdOptions.find('c') != string::npos);
    OutputCcr = (CmdOptions.find('2') != string::npos);
    OutputObm = (CmdOptions.find('o') != string::npos);
    OutputPng = (CmdOptions.find('p') != string::npos);
    OutputBmp = (CmdOptions.find('b') != string::npos);

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
            FExt = InFile.substr(InFile.find_last_of(".") + 1);
            if (InFile.find_last_of("/") != string::npos)
            {
                FName = InFile.substr(InFile.find_last_of("/") + 1, InFile.find_last_of(".") - InFile.find_last_of("/") - 1);
                FPath = InFile.substr(0, InFile.find_last_of("/") + 1);
            }
            else
            {
                FName = InFile.substr(0, InFile.find_last_of("."));
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
    if ((FExt != "png") && (FExt != "bmp") && (FExt != "kla") && (FExt != "koa"))
    {
        cerr << "***CRITICAL***\tSPOT only accepts PNG, BMP and Koala (KLA, KOA) input file types!\n";
        return EXIT_FAILURE;
    }

    //Find output file path, name and extension (extension will be omitted)
    if (OutFile != "")
    {
        if (OutFile.find_last_of("/.") != string::npos)
        {
            if (OutFile.substr(OutFile.find_last_of("/."), 1) == ".")
            {
                SaveExt = OutFile.substr(OutFile.find_last_of(".") + 1);
                if (OutFile.find_last_of("/") != string::npos)
                {
                    SaveName = OutFile.substr(OutFile.find_last_of("/") + 1, OutFile.find_last_of(".") - OutFile.find_last_of("/") - 1);
                    SavePath = OutFile.substr(0, OutFile.find_last_of("/") + 1);
                }
                else
                {
                    SaveName = OutFile.substr(0, OutFile.find_last_of("."));
                    SavePath = "";
                }
            }
            else
            {
                SaveExt = "";
                if (OutFile.find_last_of("/") != string::npos)
                {
                    SaveName = OutFile.substr(OutFile.find_last_of("/") + 1);
                    SavePath = OutFile.substr(0, OutFile.find_last_of("/") + 1);
                }
                else
                {
                    SaveName = OutFile;
                    SavePath = "";
                }
            }
        }
    }
    else
    {
        SavePath = FPath + "spot/" + FName + "/";
        SaveName = FName;
    }

    //cout << InFile << "\t" << OutFile << "\t" << CmdOptions << "\t" << CmdColors << "\n";

    if ((FExt == "kla") || (FExt == "koa"))
    {
        if (!ImportFromKoala())
            return EXIT_FAILURE;
    }
    else if ((FExt == "png") ||(FExt == "bmp"))
    {
        if (!ImportFromImage())
            return EXIT_FAILURE;
    }

    cout << "Done!\n";

    return EXIT_SUCCESS;
}
