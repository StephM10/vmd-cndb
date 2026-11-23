#include "reader.hpp"
#include <molfile_plugin.h>
#include <tcl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
struct Transaction { std::shared_ptr<cndb::Reader> reader; std::string error; std::size_t read=0; };
// The molfile plugin is explicitly THREADUNSAFE. A Tcl load owns this context
// until waitfor all completes; every file handle owns a shared reader snapshot.
Transaction transaction;
struct Handle { std::shared_ptr<cndb::Reader> reader; std::size_t next=0; };
void error(const std::exception& e) {
  transaction.error=e.what();std::fprintf(stderr,"%s\n",e.what());
}
void* open_read(const char* path,const char*,int* natoms) {
  try {
    std::shared_ptr<cndb::Reader> r;
    if(transaction.reader) {
      if(!std::filesystem::equivalent(std::filesystem::u8path(path),std::filesystem::u8path(transaction.reader->metadata().path))) throw std::runtime_error("CNDB: load context filename mismatch");
      r=transaction.reader;
    } else r=std::make_shared<cndb::Reader>(path);
    *natoms=static_cast<int>(r->metadata().beads);return new Handle{r,0};
  } catch(const std::exception& e) {error(e);return nullptr;}
}
template<std::size_t N> void field(char (&dest)[N],const std::string& value) {
  if(value.size()>=N) throw std::runtime_error("CNDB: internal field alias exceeds VMD width");
  std::memcpy(dest,value.c_str(),value.size()+1);
}
int read_structure(void* data,int* flags,molfile_atom_t* atoms) {
  try {
    const auto& m=static_cast<Handle*>(data)->reader->metadata();
    std::vector<std::string> names,resnames,chains;
    for(const auto& a:m.atoms) {names.push_back(a.name);resnames.push_back(a.resname);chains.push_back(a.chain);}
    names=cndb::aliases(names,15);resnames=cndb::aliases(resnames,7);
    auto segids=cndb::aliases(chains,7);
    // segid carries all chains even beyond VMD's one-character chain capacity.
    std::map<std::string,std::string> shortchains;
    const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for(const auto& c:chains) if(c.size()==1 && alphabet.find(c)!=std::string::npos)shortchains[c]=c;
    std::size_t nextchain=0;
    for(const auto& c:chains) if(!shortchains.count(c)) {
      while(nextchain<alphabet.size()) {
        auto token=alphabet.substr(nextchain,1);
        bool used=std::any_of(shortchains.begin(),shortchains.end(),[&](const auto& entry){return entry.second==token;});
        if(!used)break;
        ++nextchain;
      }
      shortchains[c]=nextchain<alphabet.size()?alphabet.substr(nextchain++,1):"";
    }
    for(std::size_t i=0;i<m.beads;++i) {
      auto& a=atoms[i];std::memset(&a,0,sizeof(a));
      field(a.name,names[i]);field(a.type,m.aliases[i]);field(a.resname,resnames[i]);
      field(a.chain,shortchains.at(chains[i]));field(a.segid,segids[i]);a.resid=m.atoms[i].resid;
      a.radius=static_cast<float>(m.default_radius);a.mass=1.0f;a.atomicnumber=0;
    }
    *flags=MOLFILE_RADIUS|MOLFILE_MASS|MOLFILE_ATOMICNUMBER;return MOLFILE_SUCCESS;
  } catch(const std::exception& e) {error(e);return MOLFILE_ERROR;}
}
int read_bonds(void* data,int* n,int** from,int** to,float** order,int** type,int* nt,char*** names) {
  const auto& m=static_cast<Handle*>(data)->reader->metadata();
  *n=static_cast<int>(m.bond_from.size());*from=const_cast<int*>(m.bond_from.data());*to=const_cast<int*>(m.bond_to.data());
  *order=nullptr;*type=nullptr;*nt=0;*names=nullptr;return MOLFILE_SUCCESS;
}
int read_next(void* data,int natoms,molfile_timestep_t* ts) {
  auto& h=*static_cast<Handle*>(data);const auto& m=h.reader->metadata();
  if(h.next==m.frames.size()) return MOLFILE_EOF;
  try {
    if(natoms!=static_cast<int>(m.beads)) throw std::runtime_error("CNDB: VMD atom count changed");
    if(ts) {
      auto f=h.reader->read(h.next);std::copy(f.xyz.begin(),f.xyz.end(),ts->coords);
      const auto& fm=m.frames[h.next];ts->A=fm.cell[0];ts->B=fm.cell[1];ts->C=fm.cell[2];
      ts->alpha=fm.cell[3];ts->beta=fm.cell[4];ts->gamma=fm.cell[5];ts->physical_time=fm.time_ps;
    }
    ++h.next;if(h.reader==transaction.reader)transaction.read=h.next;
    return MOLFILE_SUCCESS;
  } catch(const std::exception& e) {error(e);return MOLFILE_ERROR;}
}
void close_read(void* data) {delete static_cast<Handle*>(data);}
molfile_plugin_t plugin{};

Tcl_Obj* s(const std::string& value) {return Tcl_NewStringObj(value.data(),static_cast<int>(value.size()));}
Tcl_Obj* integer(std::size_t n) {return Tcl_NewWideIntObj(static_cast<Tcl_WideInt>(n));}
Tcl_Obj* dict() {return Tcl_NewDictObj();}
Tcl_Obj* list() {return Tcl_NewListObj(0,nullptr);}
void put(Tcl_Obj* d,const char* key,Tcl_Obj* value) {Tcl_DictObjPut(nullptr,d,Tcl_NewStringObj(key,-1),value);}
void append(Tcl_Obj* l,Tcl_Obj* value) {Tcl_ListObjAppendElement(nullptr,l,value);}
Tcl_Obj* strings(const std::vector<std::string>& v) {auto* l=list();for(const auto& x:v)append(l,s(x));return l;}
template<class Range> Tcl_Obj* numbers(const Range& v) {
  auto* l=list();
  for(auto x:v) {
    if constexpr(std::is_integral_v<decltype(x)>)append(l,Tcl_NewWideIntObj(static_cast<Tcl_WideInt>(x)));
    else append(l,Tcl_NewDoubleObj(x));
  }
  return l;
}
Tcl_Obj* attrs(const cndb::Attributes& a) {
  auto* d=dict();
  for(const auto& [name,v]:a) {
    auto* entry=dict();put(entry,"kind",s(v.kind));put(entry,"shape",numbers(v.shape));put(entry,"values",strings(v.values));
    Tcl_DictObjPut(nullptr,d,s(name),entry);
  }
  return d;
}
Tcl_Obj* metadata(const cndb::Metadata& m) {
  auto* d=dict();put(d,"path",s(m.path));put(d,"trajectory",s(m.trajectory));put(d,"layout",s(m.layout));
  put(d,"beads",integer(m.beads));put(d,"total_frames",integer(m.total_frames));put(d,"units",s(m.units));put(d,"default_radius",Tcl_NewDoubleObj(m.default_radius));
  put(d,"header",attrs(m.header));put(d,"raw_types",strings(m.raw_types));put(d,"types",strings(m.types));put(d,"aliases",strings(m.aliases));
  put(d,"type_kind",s(m.type_kind));put(d,"type_source",s(m.type_source));put(d,"topology_source",s(m.topology_source));
  auto* map=dict();for(const auto& [type,alias]:m.type_aliases)Tcl_DictObjPut(nullptr,map,s(type),s(alias));put(d,"type_aliases",map);
  auto* bonds=list();for(std::size_t i=0;i<m.bond_from.size();++i){auto* pair=list();append(pair,integer(m.bond_from[i]));append(pair,integer(m.bond_to[i]));append(bonds,pair);}put(d,"bonds",bonds);
  auto* genomic=list();for(const auto& g:m.genomic){auto* row=list();for(auto x:g)append(row,Tcl_NewWideIntObj(x));append(genomic,row);}put(d,"genomic_position",genomic);
  auto* atoms=list();for(const auto& a:m.atoms){auto* row=dict();put(row,"name",s(a.name));put(row,"resname",s(a.resname));put(row,"chain",s(a.chain));put(row,"resid",Tcl_NewIntObj(a.resid));append(atoms,row);}put(d,"atoms",atoms);
  auto* frames=list();for(const auto& f:m.frames){auto* row=dict();put(row,"id",s(f.id));put(row,"path",s(f.path));put(row,"attributes",attrs(f.attributes));put(row,"box_vectors",numbers(f.box_vectors));put(row,"cell",numbers(f.cell));put(row,"rotation",numbers(f.rotation));put(row,"time_ps",Tcl_NewDoubleObj(f.time_ps));append(frames,row);}put(d,"frames",frames);
  return d;
}
cndb::Options options(Tcl_Interp* ip,Tcl_Obj* obj) {
  cndb::Options o;Tcl_DictSearch search;Tcl_Obj *key,*value;int done;
  if(Tcl_DictObjFirst(ip,obj,&search,&key,&value,&done)!=TCL_OK)throw std::runtime_error("CNDB: options must be a dictionary");
  struct End {Tcl_DictSearch* s;~End(){Tcl_DictObjDone(s);}}end{&search};
  for(;!done;Tcl_DictObjNext(&search,&key,&value,&done)) {
    std::string k=Tcl_GetString(key),v=Tcl_GetString(value);
    if(k=="trajectory")o.trajectory=v;else if(k=="labels")o.labels=v;else if(k=="psf")o.psf=v;
    else if(k=="radius") {
      if(Tcl_GetDoubleFromObj(ip,value,&o.radius)!=TCL_OK)throw std::runtime_error("CNDB: radius must be numeric");
    }
    else if(k=="first" || k=="last" || k=="stride") {
      Tcl_WideInt n;if(Tcl_GetWideIntFromObj(ip,value,&n)!=TCL_OK)throw std::runtime_error("CNDB: "+k+" must be an integer");
      if(k=="first")o.first=n;else if(k=="last")o.last=n;else o.stride=n;
    } else if(k=="box") {
      int n;Tcl_Obj** elems;
      if(Tcl_ListObjGetElements(ip,value,&n,&elems)!=TCL_OK || (n!=0 && n!=9))throw std::runtime_error("CNDB: box must be a list of nine vector components");
      for(int i=0;i<n;++i){double x;if(Tcl_GetDoubleFromObj(ip,elems[i],&x)!=TCL_OK)throw std::runtime_error("CNDB: nonnumeric box component");o.box.push_back(x);}
    } else throw std::runtime_error("CNDB: unknown option: "+k);
  }
  return o;
}
int inspect_cmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const objv[]) {
  if(objc!=2){Tcl_WrongNumArgs(ip,1,objv,"file");return TCL_ERROR;}
  try {
    auto summaries=cndb::inspect(Tcl_GetString(objv[1]));auto* l=list();
    for(const auto& v:summaries){auto* d=dict();put(d,"name",s(v.name));put(d,"beads",integer(v.beads));put(d,"frames",integer(v.frames));append(l,d);}
    Tcl_SetObjResult(ip,l);return TCL_OK;
  }catch(const std::exception& e){Tcl_SetObjResult(ip,s(e.what()));return TCL_ERROR;}
}
int prepare_cmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const objv[]) {
  if(objc!=3){Tcl_WrongNumArgs(ip,1,objv,"file options");return TCL_ERROR;}
  try {
    if(transaction.reader)throw std::runtime_error("CNDB: another synchronous load is active");
    auto r=std::make_shared<cndb::Reader>(Tcl_GetString(objv[1]),options(ip,objv[2]));
    transaction={r,"",0};Tcl_SetObjResult(ip,metadata(r->metadata()));return TCL_OK;
  }catch(const std::exception& e){Tcl_SetObjResult(ip,s(e.what()));return TCL_ERROR;}
}
int finish_cmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const objv[]) {
  if(objc!=1){Tcl_WrongNumArgs(ip,1,objv,"");return TCL_ERROR;}
  auto* d=dict();put(d,"error",s(transaction.error));put(d,"frames_read",integer(transaction.read));transaction={};Tcl_SetObjResult(ip,d);return TCL_OK;
}
int radius_cmd(ClientData,Tcl_Interp* ip,int objc,Tcl_Obj* const objv[]) {
  if(objc!=2){Tcl_WrongNumArgs(ip,1,objv,"radius");return TCL_ERROR;}
  double value;if(Tcl_GetDoubleFromObj(ip,objv[1],&value)!=TCL_OK)return TCL_ERROR;
  try {cndb::validate_radius(value);return TCL_OK;}
  catch(const std::exception& e){Tcl_SetObjResult(ip,s(e.what()));return TCL_ERROR;}
}
} // namespace

VMDPLUGIN_API int VMDPLUGIN_init() {
  plugin.abiversion=vmdplugin_ABIVERSION;plugin.type=MOLFILE_PLUGIN_TYPE;plugin.name="vmdcndb";
  plugin.prettyname="CNDB (native, vmd-cndb)";plugin.author="Leren Wang";plugin.majorv=0;plugin.minorv=2;
  plugin.is_reentrant=VMDPLUGIN_THREADUNSAFE;plugin.filename_extension="cndb";
  plugin.open_file_read=open_read;plugin.read_structure=read_structure;plugin.read_bonds=read_bonds;
  plugin.read_next_timestep=read_next;plugin.close_file_read=close_read;return VMDPLUGIN_SUCCESS;
}
VMDPLUGIN_API int VMDPLUGIN_register(void* v,vmdplugin_register_cb cb) {return cb(v,reinterpret_cast<vmdplugin_t*>(&plugin));}
VMDPLUGIN_API int VMDPLUGIN_fini() {return VMDPLUGIN_SUCCESS;}
extern "C" __declspec(dllexport) int Vmdcndb_Init(Tcl_Interp* ip) {
  if(!Tcl_InitStubs(ip,"8.6",0))return TCL_ERROR;
  if(!Tcl_FindNamespace(ip,"::vmdcndb",nullptr,TCL_GLOBAL_ONLY))Tcl_CreateNamespace(ip,"::vmdcndb",nullptr,nullptr);
  Tcl_CreateObjCommand(ip,"::vmdcndb::inspect",inspect_cmd,nullptr,nullptr);
  Tcl_CreateObjCommand(ip,"::vmdcndb::_prepare",prepare_cmd,nullptr,nullptr);
  Tcl_CreateObjCommand(ip,"::vmdcndb::_finish",finish_cmd,nullptr,nullptr);
  Tcl_CreateObjCommand(ip,"::vmdcndb::_validate_radius",radius_cmd,nullptr,nullptr);
  return Tcl_PkgProvide(ip,"vmdcndb_native","0.2.1");
}
