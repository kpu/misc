#include "util/fake_ofstream.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/tokenize_piece.hh"
#include "util/probing_hash_table.hh"
#include "util/pool.hh"

#include <boost/unordered_set.hpp>

#include <iostream>

struct Entry {
  typedef uint64_t Key;
  Key key;
  uint64_t GetKey() const { return key; }
  void SetKey(uint64_t to) { key = to; }

  const char *id;
};

struct MutablePiece {
  mutable StringPiece behind;
  bool operator==(const MutablePiece &other) const {
    return behind == other.behind;
  }
};
std::size_t hash_value(const MutablePiece &m) {
  return util::MurmurHashNative(m.behind.data(), m.behind.size());
}
class InternString {
  public:
    const char *Add(StringPiece str) {
      MutablePiece mut;
      mut.behind = str;
      std::pair<boost::unordered_set<MutablePiece>::iterator, bool> res(strs_.insert(mut));
      if (res.second) {
        void *mem = backing_.Allocate(str.size() + 1);
        memcpy(mem, str.data(), str.size());
        static_cast<char*>(mem)[str.size()] = 0;
        res.first->behind = StringPiece(static_cast<char*>(mem), str.size());
      }
      return res.first->behind.data();
    }

  private:
    util::Pool backing_;
    boost::unordered_set<MutablePiece> strs_;
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " class_file <text >text.class" << std::endl;
    return 1;
  }
  InternString intern;
  typedef util::AutoProbing<Entry, util::IdentityHash> Table;
  Table table;
  {
    StringPiece line;
    for (util::FilePiece classes(argv[1], &std::cerr); classes.ReadLineOrEOF(line);) {
      util::TokenIter<util::SingleCharacter, false> tabs(line, '\t');
      Entry entry;
      entry.key = util::MurmurHashNative(tabs->data(), tabs->size());
      entry.id = intern.Add(*++tabs);
      Table::MutableIterator already;
      if (entry.key) table.FindOrInsert(entry, already);
    }
  }

  StringPiece word;
  util::FakeOFStream out(1);
  for (util::FilePiece text(0, NULL, &std::cerr); ;) {
    while (text.ReadWordSameLine(word)) {
      Table::ConstIterator it;
      if (table.Find(util::MurmurHashNative(word.data(), word.size()), it)) {
        out << it->id << ' ';
      } else {
        out << "<unk> ";
      }
    }
    try { text.ReadLine(); } catch (const util::EndOfFileException &e) {break;}
    out << '\n';
  }
}
