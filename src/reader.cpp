#include "reader.hpp"
#include <hdf5.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cndb {
namespace {
[[noreturn]] void fail(const std::string& s) { throw std::runtime_error("CNDB: " + s); }
void require(bool ok, const std::string& s) { if (!ok) fail(s); }
class H {
  hid_t id_ = -1;
  herr_t (*close_)(hid_t) = nullptr;
public:
  H() = default;
  H(hid_t id, herr_t (*close)(hid_t)) : id_(id), close_(close) {
    require(id >= 0, "HDF5 operation failed (file may be damaged or use an unavailable filter)");
  }
  ~H() { if (id_ >= 0) close_(id_); }
  H(H&& other) noexcept : id_(std::exchange(other.id_, -1)), close_(other.close_) {}
  H& operator=(H&& other) noexcept {
    if (id_ >= 0) close_(id_);
    id_ = std::exchange(other.id_, -1); close_ = other.close_; return *this;
  }
  H(const H&) = delete;
  operator hid_t() const { return id_; }
};
// Suppress HDF5's stderr stack only within our calls. We report one actionable error.
class Quiet {
  H5E_auto2_t previous_; void* data_;
public:
  Quiet() { H5Eget_auto2(H5E_DEFAULT, &previous_, &data_); H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr); }
  ~Quiet() { H5Eset_auto2(H5E_DEFAULT, previous_, data_); }
};
H file(const std::string& path) {
  auto id = H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
  require(id >= 0, "cannot open HDF5 file: " + path);
  return H(id, H5Fclose);
}
bool exists(hid_t loc, const std::string& name) { return H5Lexists(loc, name.c_str(), H5P_DEFAULT) > 0; }
bool is_group(hid_t loc, const std::string& name) {
  if (!exists(loc, name)) return false;
  H5O_info_t info{};
  require(H5Oget_info_by_name(loc, name.c_str(), &info, H5P_DEFAULT) >= 0, "cannot inspect " + name);
  return info.type == H5O_TYPE_GROUP;
}
std::vector<std::string> children(hid_t loc) {
  std::vector<std::string> out;
  auto callback = [](hid_t, const char* name, const H5L_info_t*, void* data) -> herr_t {
    try { static_cast<std::vector<std::string>*>(data)->emplace_back(name); return 0; }
    catch (...) { return -1; }
  };
  require(H5Literate(loc, H5_INDEX_NAME, H5_ITER_NATIVE, nullptr, callback, &out) >= 0, "cannot enumerate group");
  return out;
}
std::vector<std::size_t> shape(hid_t space) {
  int n = H5Sget_simple_extent_ndims(space);
  require(n >= 0, "cannot read dataset dimensions");
  std::vector<hsize_t> dims(static_cast<std::size_t>(n));
  H5Sget_simple_extent_dims(space, dims.data(), nullptr);
  return {dims.begin(), dims.end()};
}
std::size_t count(const std::vector<std::size_t>& dims) {
  std::size_t result = 1;
  for (auto d : dims) {
    require(!d || result <= std::numeric_limits<std::size_t>::max() / d, "dataset dimensions overflow");
    result *= d;
  }
  return result;
}
std::string number(double x) {
  std::ostringstream s; s << std::setprecision(17) << x; return s.str();
}
bool valid_utf8(const std::string& s) {
  std::uint32_t value = 0, minimum = 0; int left = 0;
  for (unsigned char c : s) {
    if (!left) {
      if (c < 128) { if (!c) return false; continue; }
      if (c >= 0xc2 && c <= 0xdf) { left=1; value=c&31; minimum=0x80; }
      else if (c >= 0xe0 && c <= 0xef) { left=2; value=c&15; minimum=0x800; }
      else if (c >= 0xf0 && c <= 0xf4) { left=3; value=c&7; minimum=0x10000; }
      else return false;
    } else {
      if ((c & 0xc0) != 0x80) return false;
      value = (value << 6) | (c & 63);
      if (--left == 0 && (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))) return false;
    }
  }
  return left == 0;
}
template<class Read> Attribute values(hid_t type, hid_t space, Read read) {
  Attribute a; a.shape = shape(space);
  const auto n = count(a.shape); const auto cls = H5Tget_class(type);
  if (cls == H5T_STRING) {
    a.kind = "string";
    if (H5Tis_variable_str(type) > 0) {
      std::vector<char*> strings(n, nullptr);
      auto status = read(type, strings.data());
      // Reclaim even partially populated reads and exceptions during string copying.
      struct Reclaim { hid_t t, s; char** p; ~Reclaim() { H5Dvlen_reclaim(t,s,H5P_DEFAULT,p); } } reclaim{type,space,strings.data()};
      require(status >= 0, "cannot read string values");
      for (auto p : strings) a.values.emplace_back(p ? p : "");
    } else {
      const auto width = H5Tget_size(type);
      require(width && n <= std::numeric_limits<std::size_t>::max() / width, "string dataset too large");
      std::vector<char> buffer(n * width);
      require(read(type, buffer.data()) >= 0, "cannot read fixed strings");
      for (std::size_t i=0; i<n; ++i) {
        std::string s(buffer.data()+i*width, width);
        auto nul = s.find('\0'); if (nul != std::string::npos) s.resize(nul);
        if (H5Tget_strpad(type) == H5T_STR_SPACEPAD) while (!s.empty() && s.back()==' ') s.pop_back();
        a.values.push_back(std::move(s));
      }
    }
    for (const auto& s : a.values) require(valid_utf8(s), "labels/metadata must use valid UTF-8 without NUL");
  } else if (cls == H5T_INTEGER || cls == H5T_ENUM) {
    require(H5Tget_size(type) <= 8, "integer values wider than 64 bits are unsupported");
    a.kind = "integer";
    H integer_type(H5Tcopy(type),H5Tclose);
    if(cls==H5T_ENUM) integer_type=H(H5Tget_super(type),H5Tclose);
    if (H5Tget_sign(integer_type) == H5T_SGN_NONE) {
      std::vector<unsigned long long> v(n);
      require(read(H5T_NATIVE_ULLONG,v.data())>=0, "cannot read unsigned integers");
      for (auto x:v) a.values.push_back(std::to_string(x));
    } else {
      std::vector<long long> v(n);
      require(read(H5T_NATIVE_LLONG,v.data())>=0, "cannot read integers");
      for (auto x:v) a.values.push_back(std::to_string(x));
    }
  } else if (cls == H5T_FLOAT) {
    a.kind = "float";
    std::vector<double> v(n);
    require(read(H5T_NATIVE_DOUBLE,v.data())>=0, "cannot read numeric values");
    for (auto x:v) a.values.push_back(number(x));
  } else fail("unsupported label/attribute HDF5 datatype");
  return a;
}
Attribute dataset(hid_t loc, const std::string& name) {
  H d(H5Dopen2(loc,name.c_str(),H5P_DEFAULT),H5Dclose);
  H t(H5Dget_type(d),H5Tclose), s(H5Dget_space(d),H5Sclose);
  return values(t,s,[&](hid_t mt,void* out){return H5Dread(d,mt,H5S_ALL,H5S_ALL,H5P_DEFAULT,out);});
}
Attributes attributes(hid_t loc) {
  Attributes result;
  const int n = H5Aget_num_attrs(loc);
  require(n >= 0, "cannot enumerate attributes");
  for (int i=0;i<n;++i) {
    H a(H5Aopen_by_idx(loc,".",H5_INDEX_NAME,H5_ITER_INC,static_cast<hsize_t>(i),H5P_DEFAULT,H5P_DEFAULT),H5Aclose);
    auto len=H5Aget_name(a,0,nullptr); require(len>=0,"cannot read attribute name");
    std::vector<char> buf(static_cast<std::size_t>(len)+1); H5Aget_name(a,buf.size(),buf.data());
    H t(H5Aget_type(a),H5Tclose), s(H5Aget_space(a),H5Sclose);
    auto cls=H5Tget_class(t);
    // Compound/opaque producer metadata is outside the simulation schema.
    require(cls==H5T_STRING || cls==H5T_INTEGER || cls==H5T_FLOAT || cls==H5T_ENUM, "unsupported attribute datatype: "+std::string(buf.data()));
    result.emplace(buf.data(),values(t,s,[&](hid_t mt,void* out){return H5Aread(a,mt,out);}));
  }
  return result;
}
std::string scalar(const Attributes& a, const std::string& key, const std::string& fallback="") {
  auto it=a.find(key); if(it==a.end()) return fallback;
  require(it->second.values.size()==1,"attribute must be scalar: "+key);
  return it->second.values[0];
}
std::optional<std::uint64_t> frame_number(std::string s) {
  if(s.rfind("t_",0)==0) s=s.substr(2);
  if(s.empty() || s.find_first_not_of("0123456789")!=std::string::npos) return {};
  try { std::size_t used; auto n=std::stoull(s,&used); require(used==s.size(),"invalid frame id"); return n; }
  catch(const std::out_of_range&) { fail("frame ID exceeds unsigned 64 bits: "+s); }
}
std::vector<std::pair<std::uint64_t,std::string>> frame_names(hid_t loc) {
  std::vector<std::pair<std::uint64_t,std::string>> out;
  for(const auto& key:children(loc)) {
    auto n=frame_number(key); if(!n) continue;
    require(!is_group(loc,key),"frame must be a dataset: "+key);
    out.emplace_back(*n,key);
  }
  std::sort(out.begin(),out.end());
  for(std::size_t i=1;i<out.size();++i) require(out[i-1].first!=out[i].first,"duplicate numeric frame ID: "+out[i].second);
  return out;
}
struct Trajectory { std::string name, base, coords; };
std::vector<Trajectory> trajectories(hid_t f) {
  std::vector<Trajectory> out;
  if(exists(f,"types") && !frame_names(f).empty()) out.push_back({"/","/","/"});
  for(const auto& name:children(f)) if(is_group(f,name)) {
    H g(H5Gopen2(f,name.c_str(),H5P_DEFAULT),H5Gclose);
    if(is_group(g,"spatial_position")) out.push_back({"/"+name,"/"+name+"/","/"+name+"/spatial_position/"});
  }
  require(!out.empty(),"no simulation trajectory found (expected types and numeric coordinate datasets)");
  return out;
}
std::size_t beads(hid_t f,const std::string& path) {
  H d(H5Dopen2(f,path.c_str(),H5P_DEFAULT),H5Dclose),s(H5Dget_space(d),H5Sclose),t(H5Dget_type(d),H5Tclose);
  auto dims=shape(s);
  require(dims.size()==2 && dims[1]==3 && dims[0]>0 && dims[0]<=static_cast<std::size_t>(std::numeric_limits<int>::max()),"coordinates must have shape (N,3), 0 < N <= INT_MAX: "+path);
  require(H5Tget_class(t)==H5T_FLOAT && (H5Tget_size(t)==4 || H5Tget_size(t)==8),"coordinates must be float32 or float64: "+path);
  return dims[0];
}
std::vector<std::string> read_labels(const std::string& path,std::size_t n) {
  std::ifstream in(std::filesystem::u8path(path),std::ios::binary);
  require(in.good(),"cannot open labels TSV: "+path);
  std::string line; require(static_cast<bool>(std::getline(in,line)),"empty labels TSV");
  if(line.rfind("\xef\xbb\xbf",0)==0) line.erase(0,3);
  if(!line.empty() && line.back()=='\r') line.pop_back();
  require(line=="bead_id\ttype","labels header must be bead_id<TAB>type");
  std::vector<std::string> out(n); std::vector<bool> seen(n);
  while(std::getline(in,line)) {
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line.empty()) continue;
    auto tab=line.find('\t'); require(tab!=std::string::npos && line.find('\t',tab+1)==std::string::npos,"each labels row must have two tab-separated columns");
    auto id=line.substr(0,tab); auto label=line.substr(tab+1);
    require(!id.empty() && id.find_first_not_of("0123456789")==std::string::npos,"bead_id must be a one-based integer");
    std::uint64_t i; try { i=std::stoull(id); } catch(...) { fail("invalid bead_id: "+id); }
    require(i>0 && i<=n,"bead_id outside selected trajectory: "+id);
    require(!seen[static_cast<std::size_t>(i-1)],"duplicate bead_id: "+id);
    require(!label.empty() && valid_utf8(label),"type must be nonempty UTF-8");
    out[static_cast<std::size_t>(i-1)]=label; seen[static_cast<std::size_t>(i-1)]=true;
  }
  require(in.eof(),"error reading labels TSV");
  require(std::all_of(seen.begin(),seen.end(),[](bool x){return x;}),"labels TSV must cover every bead exactly once");
  return out;
}
void validate_bonds(Metadata& m,const std::vector<std::int64_t>& endpoints) {
  require(endpoints.size()%2==0 && endpoints.size()/2<=static_cast<std::size_t>(std::numeric_limits<int>::max()),"invalid bond count");
  std::set<std::pair<int,int>> seen;
  for(std::size_t i=0;i<endpoints.size();i+=2) {
    auto a=endpoints[i],b=endpoints[i+1];
    require(a>=0 && b>=0 && a<static_cast<std::int64_t>(m.beads) && b<static_cast<std::int64_t>(m.beads) && a!=b,"bond endpoint outside trajectory or self-bond");
    auto pair=std::minmax(static_cast<int>(a),static_cast<int>(b));
    require(seen.emplace(pair.first,pair.second).second,"duplicate bond");
    m.bond_from.push_back(static_cast<int>(a)+1); m.bond_to.push_back(static_cast<int>(b)+1);
  }
}
Attribute compound_member(hid_t d,const std::string& field) {
  H type(H5Dget_type(d),H5Tclose),space(H5Dget_space(d),H5Sclose);
  int i=H5Tget_member_index(type,field.c_str()); require(i>=0,"missing topology/atoms field: "+field);
  H mt(H5Tget_member_type(type,static_cast<unsigned>(i)),H5Tclose);
  return values(mt,space,[&](hid_t read_type,void* out){
    H compound(H5Tcreate(H5T_COMPOUND,H5Tget_size(read_type)),H5Tclose);
    require(H5Tinsert(compound,field.c_str(),0,read_type)>=0,"cannot map topology field");
    return H5Dread(d,compound,H5S_ALL,H5S_ALL,H5P_DEFAULT,out);
  });
}
void embedded(hid_t f,const std::string& base,Metadata& m) {
  const auto top=base+"topology/";
  m.topology_source="embedded";
  if(exists(f,top+"atoms")) {
    H d(H5Dopen2(f,(top+"atoms").c_str(),H5P_DEFAULT),H5Dclose);
    auto names=compound_member(d,"name"),res=compound_member(d,"res_idx"),chains=compound_member(d,"chain_idx");
    require(names.shape==std::vector<std::size_t>{m.beads} && res.shape==names.shape && chains.shape==names.shape,"topology/atoms must have one record per bead");
    auto ids=dataset(f,top+"chain_ids"),resnames=dataset(f,top+"res_names");
    for(std::size_t i=0;i<m.beads;++i) {
      auto r=std::stoll(res.values[i]),c=std::stoll(chains.values[i]);
      require(r>=0 && r<static_cast<long long>(resnames.values.size()) && r<std::numeric_limits<int>::max() && c>=0 && c<static_cast<long long>(ids.values.size()),"topology residue/chain index out of range");
      m.atoms[i]={names.values[i],resnames.values[static_cast<std::size_t>(r)],ids.values[static_cast<std::size_t>(c)],static_cast<int>(r)+1};
    }
  }
  if(exists(f,top+"bonds")) {
    auto b=dataset(f,top+"bonds");
    require(b.kind=="integer" && b.shape.size()==2 && b.shape[1]==2,"topology/bonds must be an integer (B,2) array");
    std::vector<std::int64_t> endpoints; for(const auto& v:b.values) endpoints.push_back(std::stoll(v));
    validate_bonds(m,endpoints);
  }
}
void psf(const std::string& path,Metadata& m) {
  std::ifstream in(std::filesystem::u8path(path)); require(in.good(),"cannot open PSF: "+path);
  std::string line; require(static_cast<bool>(std::getline(in,line)) && line.rfind("PSF",0)==0,"expected a PSF header");
  bool atoms=false,bonds=false; std::vector<std::int64_t> endpoints;
  while(std::getline(in,line)) {
    if(line.find("!NATOM")!=std::string::npos) {
      std::istringstream h(line); long long n; require(static_cast<bool>(h>>n) && n==static_cast<long long>(m.beads),"PSF atom count differs from CNDB");
      require(!atoms,"duplicate PSF atom block"); atoms=true;
      for(std::size_t i=0;i<m.beads;++i) {
        require(static_cast<bool>(std::getline(in,line)),"truncated PSF atoms");
        std::istringstream row(line); long long id; std::string type; double charge,mass;
        auto& a=m.atoms[i];
        require(static_cast<bool>(row>>id>>a.chain>>a.resid>>a.resname>>a.name>>type>>charge>>mass) && id==static_cast<long long>(i+1),"PSF atoms must have sequential one-based IDs and integer residue IDs");
      }
    } else if(line.find("!NBOND")!=std::string::npos) {
      std::istringstream h(line); long long n; require(static_cast<bool>(h>>n) && n>=0 && n<=std::numeric_limits<int>::max(),"invalid PSF bond count");
      require(!bonds,"duplicate PSF bond block"); bonds=true;
      for(long long i=0;i<n*2;++i) { long long e; require(static_cast<bool>(in>>e) && e>0,"truncated/invalid PSF bonds"); endpoints.push_back(e-1); }
    }
  }
  require(atoms && bonds,"PSF must contain !NATOM and !NBOND sections");
  validate_bonds(m,endpoints); m.topology_source="psf: "+path;
}
void cell(FrameMeta& fm,const std::vector<double>& box) {
  require(box.size()==9,"box must contain nine row-major vector components");
  for(double v:box) require(std::isfinite(v),"box must be finite");
  using V=std::array<double,3>;
  auto dot=[](const V& a,const V& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
  V a{box[0],box[1],box[2]},b{box[3],box[4],box[5]},c{box[6],box[7],box[8]};
  const double A=std::sqrt(dot(a,a)),B=std::sqrt(dot(b,b)),C=std::sqrt(dot(c,c));
  require(A>0 && B>0 && C>0,"box vectors must have positive lengths");
  V e1=a; for(auto& x:e1) x/=A;
  V e2=b; double projection=dot(b,e1); for(int i=0;i<3;++i) e2[i]-=projection*e1[i];
  double norm=std::sqrt(dot(e2,e2)); require(norm>1e-12*B,"box vectors a and b are collinear");
  for(auto& x:e2) x/=norm;
  V e3{e1[1]*e2[2]-e1[2]*e2[1],e1[2]*e2[0]-e1[0]*e2[2],e1[0]*e2[1]-e1[1]*e2[0]};
  require(dot(c,e3)>1e-12*C,"box must be nondegenerate and right handed");
  auto angle=[&](const V& u,const V& v,double lengths){return std::acos(std::clamp(dot(u,v)/lengths,-1.0,1.0))*180.0/std::acos(-1.0);};
  std::array<double,6> result{A,B,C,angle(b,c,B*C),angle(a,c,A*C),angle(a,b,A*B)};
  for(std::size_t i=0;i<result.size();++i) {
    require(std::isfinite(result[i]) && result[i]<=std::numeric_limits<float>::max(),"box exceeds VMD precision/range");
    fm.cell[i]=static_cast<float>(result[i]);
  }
  fm.rotation={e1[0],e1[1],e1[2],e2[0],e2[1],e2[2],e3[0],e3[1],e3[2]};
  fm.box_vectors=box;
}
} // namespace

std::vector<std::string> aliases(const std::vector<std::string>& labels,std::size_t width) {
  std::set<std::string> reserved;
  auto safe=[&](const std::string& s){return !s.empty() && s.size()<=width && s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+.-")==std::string::npos;};
  for(const auto& s:labels) if(safe(s)) reserved.insert(s);
  std::map<std::string,std::string> table; std::vector<std::string> out; std::size_t next=0;
  for(const auto& s:labels) {
    require(valid_utf8(s),"invalid UTF-8 label");
    if(!table.count(s)) {
      std::string token=s;
      if(!safe(s)) do { token="T"+std::to_string(next++); require(token.size()<=width,"too many aliases for VMD field width"); } while(reserved.count(token));
      table[s]=token; reserved.insert(token);
    }
    out.push_back(table.at(s));
  }
  return out;
}
struct Reader::Impl { H file; explicit Impl(H&& f):file(std::move(f)) {} };
Reader::~Reader() = default;
void validate_radius(double radius) {
  require(std::isfinite(radius) && radius>0 && radius<=std::numeric_limits<float>::max() && static_cast<float>(radius)>0,"radius must be finite, positive, and fit VMD float precision");
}
Reader::Reader(const std::string& path,const Options& o) {
  Quiet quiet;
  validate_radius(o.radius);
  meta_.default_radius=o.radius;
  impl_=std::make_unique<Impl>(file(path)); auto f=static_cast<hid_t>(impl_->file);
  auto choices=trajectories(f); const Trajectory* selected=nullptr;
  if(o.trajectory.empty()) { require(choices.size()==1,"multiple trajectories: select -trajectory using inspect"); selected=&choices[0]; }
  else for(const auto& t:choices) if(t.name==o.trajectory || t.name=="/"+o.trajectory) selected=&t;
  require(selected!=nullptr,"trajectory not found: "+o.trajectory);
  const auto& t=*selected;
  H coords(H5Gopen2(f,t.coords.c_str(),H5P_DEFAULT),H5Gclose);
  auto frames=frame_names(coords); require(!frames.empty(),"selected trajectory contains no frames");
  meta_.path=path; meta_.trajectory=t.name; meta_.layout=t.name=="/"?"root_numeric_frames":"spatial_position";
  meta_.beads=beads(f,t.coords+frames[0].second); meta_.total_frames=frames.size();
  if(is_group(f,"Header")) {
    H h(H5Gopen2(f,"Header",H5P_DEFAULT),H5Gclose); meta_.header=attributes(h);
    auto version=scalar(meta_.header,"format_version");
    require(version.empty() || version=="2.0","unsupported CNDB format_version: "+version);
    auto name=scalar(meta_.header,"format_name");
    require(name.empty() || name=="OpenMiChroM-CNDB","unrecognized Header format_name: "+name);
  }
  // Unit metadata describes the source. It never triggers a length conversion:
  // coordinates, box, and display radii all use the trajectory's native unit.
  meta_.units=scalar(meta_.header,"coordinate_units","unspecified");
  auto types=dataset(f,t.base+"types");
  require(types.shape==std::vector<std::size_t>{meta_.beads} && (types.kind=="string" || types.kind=="integer" || types.kind=="float"),"types must be a one-dimensional string or numeric array of length N");
  if(types.kind=="float") for(const auto& x:types.values) require(std::isfinite(std::stod(x)),"type values must be finite");
  meta_.raw_types=types.values; meta_.type_kind=types.kind;
  meta_.types=o.labels.empty()?types.values:read_labels(o.labels,meta_.beads);
  meta_.type_source=o.labels.empty()?t.base+"types":"labels: "+o.labels;
  meta_.aliases=cndb::aliases(meta_.types,15);
  for(std::size_t i=0;i<meta_.beads;++i) meta_.type_aliases[meta_.types[i]]=meta_.aliases[i];
  meta_.atoms.resize(meta_.beads);
  for(std::size_t i=0;i<meta_.beads;++i) meta_.atoms[i].resid=static_cast<int>(i+1);
  if(!o.psf.empty()) psf(o.psf,meta_);
  else if(is_group(f,t.base+"topology")) embedded(f,t.base,meta_);
  else {
    meta_.topology_source="linear_chain";
    for(std::size_t i=1;i<meta_.beads;++i) { meta_.bond_from.push_back(static_cast<int>(i));meta_.bond_to.push_back(static_cast<int>(i+1)); }
  }
  if(exists(f,t.base+"genomic_position")) {
    auto g=dataset(f,t.base+"genomic_position");
    require(g.kind=="integer" && g.shape==std::vector<std::size_t>{meta_.beads,2},"genomic_position must be an integer (N,2) array");
    for(std::size_t i=0;i<meta_.beads;++i) meta_.genomic.push_back({std::stoll(g.values[2*i]),std::stoll(g.values[2*i+1])});
  }
  require(o.first>=0 && o.last>=-1 && o.stride>0,"frame range requires first>=0, last>=-1 and stride>0");
  const auto last=o.last==-1?static_cast<std::int64_t>(frames.size()-1):o.last;
  require(o.first<=last && last<static_cast<std::int64_t>(frames.size()),"frame range outside trajectory (first/last are zero-based sorted positions)");
  for(auto i=o.first;i<=last;) {
    const auto& name=frames[static_cast<std::size_t>(i)].second;
    FrameMeta fm; fm.id=name; fm.path=t.coords+name;
    require(beads(f,fm.path)==meta_.beads,"particle count changes at frame "+name);
    H d(H5Dopen2(f,fm.path.c_str(),H5P_DEFAULT),H5Dclose); fm.attributes=attributes(d);
    std::vector<double> box=o.box;
    if(box.empty() && fm.attributes.count("box")) {
      const auto& b=fm.attributes.at("box");
      require(b.shape==std::vector<std::size_t>{3,3} && b.kind!="string","frame box must have numeric shape (3,3)");
      for(const auto& v:b.values) box.push_back(std::stod(v));
    }
    if(!box.empty()) cell(fm,box);
    auto time=scalar(fm.attributes,"time_ps");
    if(!time.empty()) fm.time_ps=std::stod(time);
    else if(fm.attributes.count("time")) {
      const auto tu=scalar(fm.attributes,"time_units",scalar(meta_.header,"time_units"));
      if(tu=="ps" || tu=="picosecond") fm.time_ps=std::stod(scalar(fm.attributes,"time"));
      else if(tu=="ns" || tu=="nanosecond") fm.time_ps=1000*std::stod(scalar(fm.attributes,"time"));
    }
    require(std::isfinite(fm.time_ps),"physical time must be finite");
    meta_.frames.push_back(std::move(fm));
    if(o.stride>last-i) break;
    i+=o.stride;
  }
}
Frame Reader::read(std::size_t index) const {
  Quiet quiet; require(index<meta_.frames.size(),"selected frame index outside range");
  const auto& fm=meta_.frames[index]; auto f=static_cast<hid_t>(impl_->file);
  H d(H5Dopen2(f,fm.path.c_str(),H5P_DEFAULT),H5Dclose);
  // Preserve native lengths; rotate only to VMD's canonical cell orientation.
  std::vector<double> xyz(meta_.beads*3);
  require(H5Dread(d,H5T_NATIVE_DOUBLE,H5S_ALL,H5S_ALL,H5P_DEFAULT,xyz.data())>=0,"cannot decode frame "+fm.id+" (damaged dataset or unavailable HDF5 filter)");
  Frame out; out.xyz.resize(xyz.size());
  for(std::size_t i=0;i<meta_.beads;++i) for(std::size_t k=0;k<3;++k) {
    double value=0; for(std::size_t j=0;j<3;++j) value+=fm.rotation[3*k+j]*xyz[3*i+j];
    require(std::isfinite(value) && std::abs(value)<=std::numeric_limits<float>::max(),"nonfinite or out-of-range coordinate in frame "+fm.id+"; v1 requires complete simulation coordinates");
    out.xyz[3*i+k]=static_cast<float>(value);
  }
  return out;
}
std::vector<Summary> inspect(const std::string& path) {
  Quiet quiet; auto f=file(path); std::vector<Summary> out;
  for(const auto& t:trajectories(f)) {
    H g(H5Gopen2(f,t.coords.c_str(),H5P_DEFAULT),H5Gclose); auto names=frame_names(g);
    require(!names.empty(),"trajectory contains no frames: "+t.name);
    out.push_back({t.name,beads(f,t.coords+names[0].second),names.size()});
  }
  return out;
}
std::int64_t open_hdf5_objects() { return H5Fget_obj_count(H5F_OBJ_ALL,H5F_OBJ_ALL); }
} // namespace cndb
