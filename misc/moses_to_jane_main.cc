#include "util/fake_ofstream.hh"
#include "util/file_piece.hh"
#include "util/tokenize_piece.hh"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>

#include <math.h>
#include <stdlib.h>

void ParseSurface(StringPiece from, std::vector<StringPiece> &out) {
  out.clear();
  util::TokenIter<util::SingleCharacter, true> it(from, ' ');
  std::copy(it, util::TokenIter<util::SingleCharacter, true>::end(), std::back_inserter(out));
  out.pop_back();
}

void ParseAlign(StringPiece in, std::vector<unsigned int> &src, std::vector<unsigned int> &tgt) {
  src.clear();
  tgt.clear();
  unsigned int idx = 0;
  for (util::TokenIter<util::SingleCharacter, true> it(in, ' '); it; ++it, ++idx) {
    char *end;
    unsigned long int src_idx = strtoul(it->data(), &end, 10);
    UTIL_THROW_IF(it->data() == end, util::Exception, "Bad alignment " << *it);
    UTIL_THROW_IF(*end != '-', util::Exception, "Missing hyphen " << *it); 
    ++end;
    char *out;
    unsigned long int tgt_idx = strtoul(end, &out, 10);
    UTIL_THROW_IF(end == out, util::Exception, "Bad alignment " << *it);
    if (src_idx >= src.size()) src.resize(src_idx + 1);
    if (tgt_idx >= tgt.size()) tgt.resize(tgt_idx + 1);
    src[src_idx] = idx;
    tgt[tgt_idx] = idx;
  }
}

void WriteSurface(const std::vector<StringPiece> &text, const std::vector<unsigned int> &indices, util::FakeOFStream &out) {
  for (std::vector<StringPiece>::const_iterator i(text.begin()); i != text.end(); ++i) {
    if (*i->data() == '[' && i->data()[i->size() - 1] == ']') {
      out << StringPiece(i->data() + 1, i->size() - 2) << '~' << indices[i - text.begin()];
    } else {
      out << *i;
    }
    out << ' ';
  }
}

int main() {
  util::FilePiece f(0, "stdin", &std::cerr);
  util::FakeOFStream out(1);
  out << 
    "0 0 0 0 0 0 1 0 # X # <unknown-word> # <unknown-word> # 1 1 1 1 1\n"
    "0 0 0 0 0 0 0 0 # S # X~0 # X~0 # 1 1 1 1 1\n"
    "0 0 0 0 0 0 0 1 # S # S~0 X~1 # S~0 X~1 # 1 1 1 1 1\n";
  std::vector<StringPiece> source, target;
  std::vector<unsigned int> src_idx, tgt_idx;
  try { while(true) {
    util::TokenIter<util::MultiCharacter> pipes(f.ReadLine(), "|||");

    ParseSurface(*pipes, source);
    ParseSurface(*++pipes, target);

    util::TokenIter<util::SingleCharacter, true> scores(*++pipes, ' ');
    ParseAlign(*++pipes, src_idx, tgt_idx);

    for (; scores; ++scores) {
      out << std::min(100.0, -log(atof(scores->data()))) << ' ';
    }
    // Word penalty with stupid multiplier.  It's -.434295 but Jane uses costs so just .434295.
    out << static_cast<unsigned int>(target.size() * .434295) << " 0 0 # X # ";
    WriteSurface(source, src_idx, out);
    out << "# ";
    WriteSurface(target, tgt_idx, out);
    out << "# 1 1 1 1 1 \n";
  } } catch (const util::EndOfFileException &e) {}
}
