#include "reader.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
std::string q(const std::string& s) {
  std::string out="\""; const char* hex="0123456789abcdef";
  for(unsigned char c:s) {
    if(c=='"' || c=='\\') { out+='\\';out+=static_cast<char>(c); }
    else if(c<32) { out+="\\u00";out+=hex[c>>4];out+=hex[c&15]; }
    else out+=static_cast<char>(c);
  }
  return out+'"';
}
template<class Range,class Fn> void array(std::ostream& out,const Range& r,Fn f) {
  out<<'[';bool first=true;for(const auto& x:r) {if(!first)out<<',';first=false;f(x);}out<<']';
}
void strings(std::ostream& out,const std::vector<std::string>& r) {array(out,r,[&](const auto& x){out<<q(x);});}
void attrs(std::ostream& out,const cndb::Attributes& attrs) {
  out<<'{';bool first=true;
  for(const auto& [name,a]:attrs) {
    if(!first)out<<',';first=false;
    out<<q(name)<<":{\"kind\":"<<q(a.kind)<<",\"shape\":";
    array(out,a.shape,[&](auto v){out<<v;});out<<",\"values\":";strings(out,a.values);out<<'}';
  }
  out<<'}';
}
}
int run(int argc,char** argv) {
  try {
    if(argc<3) throw std::runtime_error("usage: cndb_check inspect|read FILE [--trajectory NAME --labels TSV --psf PSF --radius N --first N --last N --stride N --box a,b,c,... --stress N]");
    const std::string command=argv[1],path=argv[2];
    if(command=="inspect") {
      auto summary=cndb::inspect(path);
      array(std::cout,summary,[](const auto& s){std::cout<<"{\"name\":"<<q(s.name)<<",\"beads\":"<<s.beads<<",\"frames\":"<<s.frames<<'}';});std::cout<<'\n';return 0;
    }
    if(command!="read" && command!="failure-loop") throw std::runtime_error("unknown command");
    cndb::Options options;int stress=0;
    for(int i=3;i<argc;i+=2) {
      if(i+1>=argc) throw std::runtime_error("missing option value");
      std::string k=argv[i],v=argv[i+1];
      if(k=="--trajectory")options.trajectory=v;
      else if(k=="--labels")options.labels=v;
      else if(k=="--psf")options.psf=v;
      else if(k=="--radius")options.radius=std::stod(v);
      else if(k=="--units")throw std::runtime_error("CNDB: --units was removed; v0.2 preserves trajectory length units");
      else if(k=="--first")options.first=std::stoll(v);
      else if(k=="--last")options.last=std::stoll(v);
      else if(k=="--stride")options.stride=std::stoll(v);
      else if(k=="--stress")stress=std::stoi(v);
      else if(k=="--box") {std::istringstream in(v);std::string part;while(std::getline(in,part,','))options.box.push_back(std::stod(part));}
      else throw std::runtime_error("unknown option: "+k);
    }
    if(command=="failure-loop") {
      for(int i=0;i<20;++i) {
        bool failed=false;
        try {cndb::Reader r(path,options);for(std::size_t j=0;j<r.metadata().frames.size();++j)r.read(j);}
        catch(const std::exception&) {failed=true;}
        if(!failed || cndb::open_hdf5_objects()!=0)throw std::runtime_error("expected failure or HDF5 resource leak");
      }
      std::cout<<"{}\n";return 0;
    }
    for(int i=0;i<stress;++i) {cndb::Reader r(path,options);for(std::size_t j=0;j<r.metadata().frames.size();++j)r.read(j);}
    if(cndb::open_hdf5_objects()!=0) throw std::runtime_error("HDF5 handle leak after repeated reads");
    cndb::Reader r(path,options);const auto& m=r.metadata();std::cout.precision(17);
    // Buffer the diagnostic JSON so an invalid later frame never yields partial JSON.
    std::ostringstream out;out.precision(17);
    out<<"{\"beads\":"<<m.beads<<",\"trajectory\":"<<q(m.trajectory)<<",\"layout\":"<<q(m.layout)<<",\"total_frames\":"<<m.total_frames<<",\"units\":"<<q(m.units)<<",\"default_radius\":"<<m.default_radius<<",\"type_kind\":"<<q(m.type_kind)<<",\"type_source\":"<<q(m.type_source)<<",\"topology_source\":"<<q(m.topology_source)<<",\"header\":";attrs(out,m.header);
    out<<",\"raw_types\":";strings(out,m.raw_types);out<<",\"types\":";strings(out,m.types);out<<",\"aliases\":";strings(out,m.aliases);
    out<<",\"atoms\":";array(out,m.atoms,[&](const auto& a){out<<"{\"name\":"<<q(a.name)<<",\"resname\":"<<q(a.resname)<<",\"chain\":"<<q(a.chain)<<",\"resid\":"<<a.resid<<'}';});
    out<<",\"bonds\":[";for(std::size_t i=0;i<m.bond_from.size();++i){if(i)out<<',';out<<'['<<m.bond_from[i]<<','<<m.bond_to[i]<<']';}out<<']';
    out<<",\"genomic_position\":";array(out,m.genomic,[&](const auto& g){out<<'['<<g[0]<<','<<g[1]<<']';});
    out<<",\"frames\":[";
    for(std::size_t i=0;i<m.frames.size();++i) {
      if(i)out<<',';const auto& fm=m.frames[i];auto frame=r.read(i);
      out<<"{\"id\":"<<q(fm.id)<<",\"cell\":";array(out,fm.cell,[&](auto x){out<<x;});
      out<<",\"box_vectors\":";array(out,fm.box_vectors,[&](auto x){out<<x;});
      out<<",\"rotation\":";array(out,fm.rotation,[&](auto x){out<<x;});
      out<<",\"time_ps\":"<<fm.time_ps<<",\"attributes\":";attrs(out,fm.attributes);
      out<<",\"xyz\":";array(out,frame.xyz,[&](auto x){out<<x;});out<<'}';
    }
    out<<"]}\n";std::cout<<out.str();return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
  std::vector<std::string> utf8;std::vector<char*> args;
  for(int i=0;i<argc;++i) {
    int n=WideCharToMultiByte(CP_UTF8,0,argv[i],-1,nullptr,0,nullptr,nullptr);
    std::string text(static_cast<std::size_t>(n),'\0');
    WideCharToMultiByte(CP_UTF8,0,argv[i],-1,text.data(),n,nullptr,nullptr);text.pop_back();utf8.push_back(std::move(text));
  }
  for(auto& s:utf8)args.push_back(s.data());
  return run(argc,args.data());
}
#else
int main(int argc,char** argv) {return run(argc,argv);}
#endif
