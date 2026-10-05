#include "fectty/acquisition.hpp"
#include <algorithm>
#include <array>
#include <cmath>
namespace fectty {
static constexpr std::array<int,25> pattern={0,1,2,3,0,2,1,3,1,0,3,2,2,3,0,1,3,1,2,0,0,3,1,2,3};
std::vector<float> acquisition_waveform(const FskConfig& c){size_t sps=size_t(std::llround(c.sample_rate/c.symbol_rate));std::vector<float> out;out.reserve(pattern.size()*sps);double ph=0,twopi=2*std::acos(-1.0);for(int t:pattern){double d=twopi*c.tones[t]/c.sample_rate;for(size_t k=0;k<sps;k++){out.push_back(float(std::sin(ph)));ph+=d;if(ph>twopi)ph-=twopi;}}return out;}
static double energy(std::span<const float>x,size_t pos,size_t n,double coefficient,size_t stride){double previous=0,older=0;for(size_t k=0;k<n;k+=stride){const double current=x[pos+k]+coefficient*previous-older;older=previous;previous=current;}return std::max(0.0,previous*previous+older*older-coefficient*previous*older);}
AcquisitionResult acquire(std::span<const float>x,const FskConfig&c,double search,double step,size_t analysis_stride){
 if(!std::isfinite(search)||!std::isfinite(step)||search<0||step<=0||!std::isfinite(c.sample_rate)||!std::isfinite(c.symbol_rate)||c.sample_rate<=0||c.symbol_rate<=0)return {};
 const double period=c.sample_rate/c.symbol_rate;
 if(period<1||period>double(x.size()/pattern.size()))return {};
 size_t sps=size_t(std::llround(period));size_t need=pattern.size()*sps;if(x.size()<need)return {};
 // Acquisition needs narrow-band tone energy, not the full audio bandwidth.
 // The coarse live search may use every fourth sample, but only when all tones remain
 // below that sample rate's Nyquist limit. Generic configurations fall back
 // to full-rate analysis. This is a search optimization, not a wire change.
 size_t stride=analysis_stride==4?4:1;
 for(double tone:c.tones){
  if(!std::isfinite(tone)||tone-search<=0||tone+search>=c.sample_rate/(2*stride))stride=1;
 }
 // A few matching tail symbols followed by silence can have a ratio near 1.
 // Require most of the actual 25-symbol field before accepting its score.
 size_t active_symbols=0;
 for(size_t symbol=0;symbol<pattern.size();++symbol){
  const auto block=x.subspan(symbol*sps,sps);
  if(std::any_of(block.begin(),block.end(),[](float value){return std::isfinite(value)&&std::abs(value)>=0.000001f;}))++active_symbols;
 }
 if(active_symbols<20)return {};
 AcquisitionResult best;
 for(double off=-search;off<=search+1e-9;off+=step){
  double coefficients[4];
  for(int t=0;t<4;++t)coefficients[t]=2*std::cos(2*std::acos(-1.0)*(c.tones[t]+off)*stride/c.sample_rate);
  double good=0,total=0;
  for(size_t s=0;s<pattern.size();s++){double es[4];for(int t=0;t<4;t++)es[t]=energy(x,s*sps,sps,coefficients[t],stride);double wanted=es[pattern[s]]; double others=0; for(int t=0;t<4;t++) if(t!=pattern[s]) others+=es[t]; good+=wanted; total+=wanted+others;}
  double score=total>0?good/total:0;if(score>best.score){best={score>0.70,need,off,score};}
 }
 return best;}
}
