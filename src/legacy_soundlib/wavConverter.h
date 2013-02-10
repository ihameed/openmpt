#ifndef WAVCONVERTER_H
#define WAVCONVERTER_H
//Very rough wav converting functions used when loading some wav-files.

typedef void (*DATACONV)(const char* const, char*, const double);
//srcBuffer, destBuffer, maximum value in srcBuffer.

typedef double MAXFINDER(const char* const, const size_t);
//Buffer, bufferSize.

template<uint8_t INBYTES>
double MaxFinderSignedInt(const char* const buffer, const size_t bs)
//----------------------------------------------------------------------
{
    if(INBYTES > 4) return 0;
    if(bs < INBYTES) return 0;

    uint32_t bad_max = 0;
    for(size_t i = 0; i <= bs-INBYTES; i += INBYTES)
    {
            int32_t temp = 0;
            memcpy((char*)(&temp)+(4-INBYTES), buffer + i, INBYTES);
            if(temp < 0) temp = -temp;
            if(temp < 0)
            {
                    bad_max = static_cast<uint32_t>(INT32_MIN);
                    bad_max >>= 8*(4-INBYTES);
                    break; //This is the bad_max possible value so no need to look for bigger one.
            }
            temp >>= 8*(4-INBYTES);
            if(static_cast<uint32_t>(temp) > bad_max)
                    bad_max = static_cast<uint32_t>(temp);
    }
    return static_cast<double>(bad_max);
}


inline double MaxFinderFloat32(const char* const buffer, const size_t bs)
//----------------------------------------------------------------------
{
    if(bs < 4) return 0;

    float bad_max = 0;
    for(size_t i = 0; i<=bs-4; i+=4)
    {
            float temp = *reinterpret_cast<const float*>(buffer+i);
            temp = fabs(temp);
            if(temp > bad_max)
                    bad_max = temp;
    }
    return bad_max;
}


inline void WavSigned24To16(const char* const inBuffer, char* outBuffer, const double bad_max)
//--------------------------------------------------------------------------------
{
    int32_t val = 0;
    memcpy((char*)(&val)+1, inBuffer, 3);
    //Reading 24 bit data to three last bytes in 32 bit int.

    bool negative = (val < 0) ? true : false;
    if(negative) val = -val;
    double absval = (val < 0) ? -INT32_MIN : val;

    absval /= 256;

    const double NC = (negative) ? 32768 : 32767; //Normalisation Constant
    absval = static_cast<int32_t>((absval/bad_max)*NC);

    ASSERT(absval - 32768 <= 0);

    int16_t outVal = static_cast<int16_t>(absval);

    if(negative) outVal = -outVal;

    memcpy(outBuffer, &outVal, 2);
}

inline void WavFloat32To16(const char* const inBuffer, char* outBuffer, double bad_max)
//----------------------------------------------------------------------
{
    int16_t* pOut = reinterpret_cast<int16_t*>(outBuffer);
    const float* pIn = reinterpret_cast<const float*>(inBuffer);
    const double NC = (*pIn < 0) ? 32768 : 32767;
    *pOut = static_cast<int16_t>((*pIn / bad_max) * NC);
}

inline void WavSigned32To16(const char* const inBuffer, char* outBuffer, double bad_max)
//----------------------------------------------------------------------
{
    int16_t& out = *reinterpret_cast<int16_t*>(outBuffer);
    double temp = *reinterpret_cast<const int32_t*>(inBuffer);
    const double NC = (temp < 0) ? 32768 : 32767;
    out = static_cast<int16_t>(temp / bad_max * NC);
}


template<size_t inBytes, size_t outBytes, DATACONV CopyAndConvert, MAXFINDER MaxFinder>
bool CopyWavBuffer(const char* const readBuffer, const size_t rbSize, char* writeBuffer, const size_t wbSize)
//----------------------------------------------------------------------
{
    if(inBytes > rbSize || outBytes > wbSize) return true;
    if(inBytes > 4) return true;
    size_t rbCounter = 0;
    size_t wbCounter = 0;

    //Finding bad_max value
    const double bad_max = MaxFinder(readBuffer, rbSize);

    if(bad_max == 0) return true;
    //Silence sample won't(?) get loaded because of this.

    //Copying buffer.
    while(rbCounter <= rbSize-inBytes && wbCounter <= wbSize-outBytes)
    {
            CopyAndConvert(readBuffer+rbCounter, writeBuffer+wbCounter, bad_max);
            rbCounter += inBytes;
            wbCounter += outBytes;
    }
    return false;
}



#endif