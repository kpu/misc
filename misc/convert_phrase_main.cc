#include "util/file_piece.hh"
#include "util/tokenize_piece.hh"

#include <algorithm>
#include <iostream>
#include <vector>

#include <math.h>
#include <stdlib.h>

unsigned long int PieceInt(StringPiece str) {
  char *end;
  unsigned long int ret = strtoul(str.data(), &end, 10);
  UTIL_THROW_IF(end != str.data() + str.size(), util::Exception, "Bad number " << str);
  return ret;
}

float PieceFloat(StringPiece str) {
  char *end;
  float ret = strtof(str.data(), &end);
  UTIL_THROW_IF(end != str.data() + str.size(), util::Exception, "Bad number " << str);
  return ret;
}

void ParseAlignment(StringPiece from, std::vector<unsigned int> &to) {
  std::vector<std::pair<unsigned int, unsigned int> > pairs;
  unsigned int max_target = 0;
  for (util::TokenIter<util::SingleCharacter, true> i(from, ' '); i; ++i) {
    util::TokenIter<util::SingleCharacter> hyphen(*i, '-');
    std::pair<unsigned int, unsigned int> to_add;
    to_add.first = PieceInt(*hyphen);
    to_add.second = PieceInt(*++hyphen);
    max_target = std::max(max_target, to_add.second);
    pairs.push_back(to_add);
  }
  std::sort(pairs.begin(), pairs.end());
  to.clear();
  to.resize(max_target + 1);
  for (std::vector<std::pair<unsigned int, unsigned int> >::const_iterator i(pairs.begin()); i != pairs.end(); ++i) {
    to[i->second] = i - pairs.begin() + 1;
  }
}

int main() {
  util::FilePiece f(0, "stdin", &std::cerr);
  StringPiece l;
  std::vector<unsigned int> alignment;
  try { while (true) {
    util::TokenIter<util::MultiCharacter> pipes(f.ReadLine(), "|||");
    util::TokenIter<util::SingleCharacter, true> source(*pipes, ' ');
    util::TokenIter<util::SingleCharacter, true> target(*++pipes, ' ');
    util::TokenIter<util::SingleCharacter, true> features(*++pipes, ' ');
    ParseAlignment(*++pipes, alignment);
    std::cout << "[X] ||| ";
    unsigned int source_index = 0;
    for (; source; ++source) {
      if (*source == "[X][X]") {
        std::cout << "[X," << ++source_index << "] ";
      } else if (*source == "[X]") {
      } else {
        std::cout << *source << ' ';
      }
    }
    std::cout << "||| ";
    unsigned int target_index = 0;
    for (; target; ++target, ++target_index) {
      if (*target == "[X][X]") {
        std::cout << "[X," << alignment[target_index] << "] ";
      } else if (*target == "[X]") {
      } else {
        std::cout << *target << ' ';
      }
    }
    std::cout << "|||";
    for (; features; ++features) { 
      float value = PieceFloat(*features);
      value = (value <= 0.0) ? -100.0 : logf(value);
      std::cout << ' ' << value;
    }
    std::cout << '\n';
  } } catch(const util::EndOfFileException &e) {}
}
