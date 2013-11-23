#include "util/file_piece.hh"
#include "util/tokenize_piece.hh"
#include "util/murmur_hash.hh"

#include <boost/circular_buffer.hpp>
#include <boost/unordered_map.hpp>

#include <iostream>

std::pair<uint64_t, uint64_t> SortPair(uint64_t first, uint64_t second) {
  return first < second ? std::make_pair(first, second) : std::make_pair(second, first);
}

int main() {
  util::FilePiece in(0, "stdin", &std::cerr);
  StringPiece line;
  const unsigned int radius = 5;
  const uint64_t kBOS = util::MurmurHashNative("<s>", 3);
  const uint64_t kEOS = util::MurmurHashNative("</s>", 4);
  boost::circular_buffer<uint64_t> history(radius);
  boost::unordered_map<std::pair<uint64_t, uint64_t>, uint64_t> counts;
  while (true) {
    try {
      line = in.ReadLine();
    } catch (const util::EndOfFileException &e) { break; }
    history.clear();
    history.push_back(kBOS);
    for (util::TokenIter<util::AnyCharacter, true> tok(line, " \t"); tok; ++tok) {
      uint64_t word = util::MurmurHashNative(tok->data(), tok->size());
      for (boost::circular_buffer<uint64_t>::const_iterator i = history.begin(); i != history.end(); ++i) {
        ++counts[SortPair(word, *i)]; 
      }
      history.push_back(word);
    }
    for (boost::circular_buffer<uint64_t>::const_iterator i = history.begin(); i != history.end(); ++i) {
      ++counts[SortPair(kEOS, *i)]; 
    }
  }
  for (boost::unordered_map<std::pair<uint64_t, uint64_t>, uint64_t>::const_iterator i = counts.begin(); i != counts.end(); ++i) {
    std::cout << i->first.first << ' ' << i->first.second << ' ' << i->second << '\n';
  }
}
