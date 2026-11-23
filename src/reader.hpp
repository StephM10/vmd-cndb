#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cndb {
struct Attribute {
  std::string kind;
  std::vector<std::size_t> shape;
  std::vector<std::string> values;
};
using Attributes = std::map<std::string, Attribute>;
struct Options {
  std::string trajectory, labels, psf, units = "auto";
  std::int64_t first = 0, last = -1, stride = 1;
  std::vector<double> box; // Nine row-major vectors, in input coordinate units.
};
struct Summary { std::string name; std::size_t beads, frames; };
struct Atom {
  std::string name = "BEAD", resname = "BEAD", chain = "A";
  int resid = 1;
};
struct FrameMeta {
  std::string id, path;
  Attributes attributes;
  std::vector<double> box_angstrom; // Original orientation, for provenance.
  std::array<double, 9> rotation{1,0,0, 0,1,0, 0,0,1};
  std::array<float, 6> cell{0,0,0,90,90,90};
  double time_ps = 0;
};
struct Metadata {
  std::string path, trajectory, layout, units, type_kind, type_source, topology_source;
  double scale = 10;
  std::size_t beads = 0, total_frames = 0;
  Attributes header;
  std::vector<std::string> raw_types, types, aliases;
  std::map<std::string, std::string> type_aliases;
  std::vector<Atom> atoms;
  std::vector<int> bond_from, bond_to; // VMD convention: one-based.
  std::vector<std::array<std::int64_t, 2>> genomic;
  std::vector<FrameMeta> frames;
};
struct Frame { std::vector<float> xyz; };
class Reader {
public:
  explicit Reader(const std::string& path, const Options& options = {});
  ~Reader();
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;
  Frame read(std::size_t selected_index) const;
  const Metadata& metadata() const { return meta_; }
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  Metadata meta_;
};
std::vector<Summary> inspect(const std::string& path);
std::vector<std::string> aliases(const std::vector<std::string>& labels, std::size_t width);
std::int64_t open_hdf5_objects();
} // namespace cndb
