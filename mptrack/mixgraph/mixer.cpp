// mostly lifted from Tables.cpp, by Olivier
#include "stdafx.h"

#include "mixer.h"
#include "../pervasives/pervasives.h"

using namespace modplug::pervasives;

namespace modplug {
namespace mixgraph {

double sinc_table[SINC_SIZE];
const double kaiser_beta = 9.6377;
const double lowpass_factor = 1.0;

// see https://ccrma.stanford.edu/~jos/sasp/Kaiser_Window.html
// see http://mathworld.wolfram.com/ModifiedBesselFunctionoftheFirstKind.html
// izero : double -> double is an approximation of the zero-order modified bessel function of the first kind
// [(x / 2)^2k / k!^2] / [(x / 2)^2(k - 1) / (k - 1)!^2] = x^2 / (2k)^2
double izero(double x) {
    static const double tolerance = 1e-21; // lifted from jos
    double i_0 = 1;
    double term = 1;
    double denom = 0;
    do {
        denom += 2;
        term *= (x * x) / (denom * denom);
        i_0 += term;
    } while (term > tolerance * i_0);
    return i_0;
}

// see https://ccrma.stanford.edu/~jos/sasp/Kaiser_Window.html
double kaiser(double window_length, double beta, double n) {
    double det = (n / (window_length / 2));
    return izero(beta * sqrt(1 - det * det)) / izero(beta);
}

double normalized_sinc(double x) {
    if (x == 0) return 1;
    return sin(PI * x) / (PI * x);
}

static void prefill_sinc_fir2(double *psinc, const int phases_per_tap, const int taps)
{
    const double izero_beta = izero(kaiser_beta);
    const double kPi = 4.0*atan(1.0)*lowpass_factor;
    for (int isrc=0; isrc<8*phases_per_tap; isrc++)
    {
        double fsinc;
        int ix = 7 - (isrc & 7);
        ix = (ix*phases_per_tap)+(isrc>>3);
        if (ix == (4*phases_per_tap))
        {
            fsinc = 1.0;
        } else
        {
            double x = (double)(ix - (4*phases_per_tap)) * (double)(1.0/phases_per_tap);
            fsinc = sin(x*kPi) * izero(kaiser_beta*sqrt(1-x*x*(1.0/16.0))) / (izero_beta*x*kPi); // Kaiser window
        }
        *psinc++ = fsinc * lowpass_factor;
    }
}

void init_tables() {
    prefill_sinc_fir2(sinc_table, TAP_PHASES, FIR_TAPS);
}


}
}
