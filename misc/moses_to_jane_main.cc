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

void PrintUnknown(StringPiece str, util::FakeOFStream &out) {
  out << "0 0 0 0 0 0.434295 -1 0 # X # " << str << " # " << str << " # 1 1 1 1 1\n";
}

// Hallucinate unknown word entries for words that appear in phrases but not as singletons.
class PassthroughMaker {
  public:
    void Singleton(StringPiece word) {
      map_[util::MurmurHashNative(word.data(), word.size())].clear();
    }

    void InPhrase(StringPiece word) {
      std::pair<uint64_t, std::string> to_ins;
      to_ins.first = util::MurmurHashNative(word.data(), word.size());
      std::pair<Map::iterator, bool> res(map_.insert(to_ins));
      if (res.second) {
        res.first->second.assign(word.data(), word.size());
      }
    }

    void Print(util::FakeOFStream &to) {
      for (boost::unordered_map<uint64_t, std::string>::const_iterator i = map_.begin(); i != map_.end(); ++i) {
        if (i->second.size())
          PrintUnknown(i->second, to);
      }
    }

  private:
    // key is murumurhash of word.  value is string if no vocab.
    typedef boost::unordered_map<uint64_t, std::string> Map;
    Map map_;
};

void ParseSurface(StringPiece from, std::vector<StringPiece> &out) {
  out.clear();
  util::TokenIter<util::SingleCharacter, true> it(from, ' ');
  std::copy(it, util::TokenIter<util::SingleCharacter, true>::end(), std::back_inserter(out));
  out.pop_back();
}

unsigned int ParseAlign(StringPiece in, std::vector<unsigned int> &src, std::vector<unsigned int> &tgt) {
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
  return idx;
}

bool MosesNonTerminal(const StringPiece &str) {
  return *str.data() == '[' && str.data()[str.size() - 1] == ']';
}

unsigned int WriteSurface(const std::vector<StringPiece> &text, const std::vector<unsigned int> &indices, util::FakeOFStream &out) {
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

int main() {
  util::FilePiece f(0, "stdin", &std::cerr);
  util::FakeOFStream out(1);
  PrintUnknown("<unknown-word>", out);
  out <<
    // passthrough gets a word penalty too.  Penultimate feature is passthrough, last feature is glue
    "0 0 0 0 0 0 0 0 # S # X~0 # X~0 # 1 1 1 1 1\n"
    "0 0 0 0 0 0 0 -1 # S # S~0 X~1 # S~0 X~1 # 1 1 1 1 1\n";
  std::vector<StringPiece> source, target;
  std::vector<unsigned int> src_idx, tgt_idx;
  PassthroughMaker pass;
  try { while(true) {
    util::TokenIter<util::MultiCharacter> pipes(f.ReadLine(), "|||");

    ParseSurface(*pipes, source);
    ParseSurface(*++pipes, target);

    if (source.size() == 1) {
      if (!MosesNonTerminal(source[0])) {
        pass.Singleton(source[0]);
      }
    } else {
      for (std::vector<StringPiece>::const_iterator i = source.begin(); i != source.end(); ++i) {
        if (!MosesNonTerminal(*i))
          pass.InPhrase(*i);
      }
    }

    util::TokenIter<util::SingleCharacter, true> scores(*++pipes, ' ');
    unsigned int non_terminals = ParseAlign(*++pipes, src_idx, tgt_idx);

    for (; scores; ++scores) {
      out << std::min(100.0, -log(atof(scores->data()))) << ' ';
    }
    // Word penalty with stupid multiplier.  It's -.434295 but Jane uses costs so just .434295.
    out << (static_cast<float>(target.size() - non_terminals) * .434295) << " 0 0 # X # ";
    UTIL_THROW_IF(non_terminals != WriteSurface(source, src_idx, out), util::Exception, "Non-terminal count mismatch");
    out << "# ";
    UTIL_THROW_IF(non_terminals != WriteSurface(target, tgt_idx, out), util::Exception, "Non-terminal count mismatch");
    out << "# 1 1 1 1 1 \n";
  } } catch (const util::EndOfFileException &e) {}
  pass.Print(out);
}
