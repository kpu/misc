#include "util/fake_ofstream.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/tokenize_piece.hh"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>

#include <math.h>
#include <stdlib.h>

#include <boost/unordered_map.hpp>

void ParseSurface(StringPiece from, std::vector<StringPiece> &out) {
  out.clear();
  util::TokenIter<util::SingleCharacter, true> it(from, ' ');
  std::copy(it, util::TokenIter<util::SingleCharacter, true>::end(), std::back_inserter(out));
  out.pop_back();
}

class ParseAlign {
  public:
    unsigned int Parse(StringPiece in, std::vector<unsigned int> &tgt) {
      temp_.clear();

      for (util::TokenIter<util::SingleCharacter, true> it(in, ' '); it; ++it) {
        char *end;
        unsigned long int src_idx = strtoul(it->data(), &end, 10);
        UTIL_THROW_IF(it->data() == end, util::Exception, "Bad alignment " << *it);
        UTIL_THROW_IF(*end != '-', util::Exception, "Missing hyphen " << *it);
        ++end;
        char *out;
        unsigned long int tgt_idx = strtoul(end, &out, 10);
        UTIL_THROW_IF(end == out, util::Exception, "Bad alignment " << *it);
        temp_.push_back(std::pair<unsigned int, unsigned int>(src_idx, tgt_idx));
      }
      std::sort(temp_.begin(), temp_.end());
      tgt.clear();
      // These are the indices that Jane will see
      for (unsigned int idx = 0; idx < temp_.size(); ++idx) {
        unsigned int target_position = temp_[idx].second;
        if (target_position >= tgt.size()) {
          tgt.resize(target_position + 1);
        }
        tgt[target_position] = idx;
      }
      return temp_.size();
    }
  private:
    std::vector<std::pair<unsigned int, unsigned int> > temp_;
};

bool MosesNonTerminal(const StringPiece &str) {
  return *str.data() == '[' && str.data()[str.size() - 1] == ']';
}

unsigned int WriteTarget(const std::vector<StringPiece> &text, const std::vector<unsigned int> &indices, util::FakeOFStream &out) {
  unsigned int non_terminals = 0;
  for (std::vector<StringPiece>::const_iterator i(text.begin()); i != text.end(); ++i) {
    if (MosesNonTerminal(*i)) {
      out << "X~" << indices[i - text.begin()];
      ++non_terminals;
    } else {
      UTIL_THROW_IF(std::find(i->data(), i->data() + i->size(), '#') != i->data() + i->size(), util::Exception, "# detected in input data");
      UTIL_THROW_IF(std::find(i->data(), i->data() + i->size(), '~') != i->data() + i->size(), util::Exception, "~ detected in input data");
      out << *i;
    }
    out << ' ';
  }
  return non_terminals;
}

unsigned int WriteSource(const std::vector<StringPiece> &text, util::FakeOFStream &out) {
  unsigned int non_terminals = 0;
  for (std::vector<StringPiece>::const_iterator i(text.begin()); i != text.end(); ++i) {
    if (MosesNonTerminal(*i)) {
      out << "X~" << non_terminals;
      ++non_terminals;
    } else {
      UTIL_THROW_IF(std::find(i->data(), i->data() + i->size(), '#') != i->data() + i->size(), util::Exception, "# detected in input data");
      UTIL_THROW_IF(std::find(i->data(), i->data() + i->size(), '~') != i->data() + i->size(), util::Exception, "~ detected in input data");
      out << *i;
    }
    out << ' ';
  }
  return non_terminals;
}

int main() {
  util::FilePiece f(0, "stdin", &std::cerr);
  util::FakeOFStream out(1);
  out <<
    // Penultimate feature is passthrough, last feature is glue
    "0 0 0 0 0 0.434295 -1 0 # X # <unknown-word> # <unknown-word> # 1 1 1 1 1\n"
    "0 0 0 0 0 0.434295 0 0 # S # <s> # <s> # 1 1 1 1 1\n"
    "0 0 0 0 0 0.434295 0 0 # S # S~0 </s> # S~0 </s> # 1 1 1 1 1\n"
    "0 0 0 0 0 0 0 -1 # S # S~0 X~1 # S~0 X~1 # 1 1 1 1 1\n";
  std::vector<StringPiece> source, target;
  ParseAlign align;
  std::vector<unsigned int> tgt_indices;
  try { while(true) {
    util::TokenIter<util::MultiCharacter> pipes(f.ReadLine(), "|||");

    ParseSurface(*pipes, source);
    ParseSurface(*++pipes, target);

    util::TokenIter<util::SingleCharacter, true> scores(*++pipes, ' ');

    unsigned int non_terminals = align.Parse(*++pipes, tgt_indices);

    for (; scores; ++scores) {
      out << std::min(100.0, -log(atof(scores->data()))) << ' ';
    }
    // Word penalty with stupid multiplier.  It's -.434295 but Jane uses costs so just .434295.
    out << (static_cast<float>(target.size() - non_terminals) * .434295) << " 0 0 # X # ";
    UTIL_THROW_IF(non_terminals != WriteSource(source, out), util::Exception, "Non-terminal count mismatch");
    out << "# ";
    UTIL_THROW_IF(non_terminals != WriteTarget(target, tgt_indices, out), util::Exception, "Non-terminal count mismatch");
    out << "# 1 1 1 1 1 \n";
  } } catch (const util::EndOfFileException &e) {}
}
