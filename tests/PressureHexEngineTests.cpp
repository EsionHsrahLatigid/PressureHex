#include "pressurehex/PressureHexEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
using pressurehex::PressureHexEngine; using pressurehex::PressureHexParameters;
namespace {
float renderPeak(PressureHexParameters p, float input){ PressureHexEngine e; e.prepare(48000.0); e.setParameters(p); e.reset(); float peak=0; for(int i=0;i<8192;++i){ auto f=e.processSample(input,input); if(i>1024) peak=std::max(peak,std::max(std::fabs(f.left),std::fabs(f.right))); } return peak; }
void testSilence(){ PressureHexEngine e; e.prepare(48000.0); for(int i=0;i<4096;++i){ auto f=e.processSample(0,0); assert(std::fabs(f.left)<=1e-7f); assert(std::fabs(f.right)<=1e-7f);} }
void testCompression(){ PressureHexParameters soft; soft.threshold=0; soft.ratio=1; soft.lookahead=0; PressureHexParameters hard; hard.threshold=-30; hard.ratio=12; hard.attack=.1f; hard.lookahead=5; auto clean=renderPeak(soft,.9f); auto clamped=renderPeak(hard,.9f); assert(clamped < clean * .55f); }
void testFocus(){ PressureHexParameters peak; peak.threshold=-24; peak.ratio=8; peak.focus=1; PressureHexParameters rms=peak; rms.focus=0; assert(std::fabs(renderPeak(peak,.75f)-renderPeak(rms,.75f)) < .12f); }
void testDeterministic(){ PressureHexParameters p; p.threshold=-28; p.ratio=6; p.knee=12; auto a=renderPeak(p,.8f); auto b=renderPeak(p,.8f); assert(std::fabs(a-b)<=1e-7f); }
void testFinite(){ PressureHexEngine e; PressureHexParameters p; p.threshold=1000; p.ratio=1000; p.attack=0; p.release=0; p.knee=1000; p.focus=1000; p.lookahead=1000; e.prepare(0); e.setParameters(p); for(int i=0;i<4096;++i){ auto f=e.processSample(1000,-1000); assert(std::isfinite(f.left)); assert(f.left>=-.9801f&&f.left<=.9801f); assert(std::isfinite(f.right)); } }
}
int main(){ testSilence(); testCompression(); testFocus(); testDeterministic(); testFinite(); std::cout<<"PressureHexEngineTests passed\n"; }
