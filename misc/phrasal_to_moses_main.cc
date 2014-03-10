#include "util/fake_ofstream.hh"
#include "util/file_piece.hh"
#include "util/tokenize_piece.hh"

#include <iostream>

namespace {
static const double_conversion::StringToDoubleConverter kConverter(
    double_conversion::StringToDoubleConverter::ALLOW_TRAILING_JUNK | double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES,
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN(),
    "inf",
    "NaN");
} // namespace

int main() {
  util::FilePiece in(0, NULL, &std::cerr);
  util::FakeOFStream out(1);
  StringPiece line;
  while (in.ReadLineOrEOF(line)) {
    util::TokenIter<util::MultiCharacter, false> pipes(line, "|||");
    out << *pipes; // source
    out << "|||";
    out << *(++pipes); //target
    ++pipes; // source alignment?
    ++pipes; // target alignment?
    out << "|||";
    for (util::TokenIter<util::SingleCharacter, true> feats(*++pipes, ' '); feats; ++feats) {
      int chars;
      double value = kConverter.StringToDouble(feats->data(), feats->size(), &chars);
      UTIL_THROW_IF2(isnan(value), "Found a NaN: " << *feats);
      out << ' ' << exp(value);
    }
    out << '\n';
  }
}
