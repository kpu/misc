#include "util/file_piece.hh"
#include "util/tokenize_piece.hh"
#include "util/murmur_hash.hh"
#include "util/probing_hash_table.hh"

#include <boost/circular_buffer.hpp>
#include <boost/unordered_map.hpp>

#include <iostream>

struct Entry {
  typedef std::pair<uint32_t, uint32_t> Key;
  Key GetKey() const { return key; }
  void SetKey(Key to) { key = to; }

  Key key;
  uint64_t value;
};

struct HashPair {
  uint64_t operator()(const Entry::Key &key) {
    return util::MurmurHashNative(&key, sizeof(Entry::Key), 0);
  }
};

std::pair<uint64_t, uint64_t> SortPair(uint64_t first, uint64_t second) {
  return first < second ? std::make_pair(first, second) : std::make_pair(second, first);
}

int main() {
  util::FilePiece in(0, "stdin", &std::cerr);
  StringPiece line;
  const unsigned int radius = 5;
  boost::circular_buffer<uint32_t> history(radius);
  typedef util::AutoProbing<Entry, HashPair> Table;
  Table table;
  Entry entry;
  entry.value = 0;
  try { while (true) {
    if (in.SkipSpacesIsNewline()) history.clear();
    uint32_t word = in.ReadULong();
    for (boost::circular_buffer<uint32_t>::const_iterator i = history.begin(); i != history.end(); ++i) {  
      entry.key = SortPair(word, *i);
      Table::MutableIterator it;
      table.FindOrInsert(entry, it);
      ++it->value;
    }
    history.push_back(word);
  } } catch (const util::EndOfFileException &e) {}
/*  for (boost::unordered_map<std::pair<uint64_t, uint64_t>, uint64_t>::const_iterator i = counts.begin(); i != counts.end(); ++i) {
    std::cout << i->first.first << ' ' << i->first.second << ' ' << i->second << '\n';
  }*/
}
