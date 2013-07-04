#include "lm/left.hh"
#include "lm/model.hh"
#include "util/tokenize_piece.hh"

#include <functional>
#include <string>
#include <vector>

#include <math.h>

class Pair {
  public:
    std::string &MutableLine() { return line_; }
    const std::string &Line() const { return line_; }

    template <class M> void Parse(const M &model, const std::vector<float> &weights);

    StringPiece Source() const { return source_; }

    float Score() const { return score_; }

  private:
    std::string line_;
    StringPiece source_;
    float score_;
};

float PieceFloat(StringPiece str) {
  char *end;
  float ret = strtof(str.data(), &end);
  UTIL_THROW_IF(end != str.data() + str.size(), util::Exception, "Bad number " << str);
  return ret;
}

template <class M> void Pair::Parse(const M &model, const std::vector<float> &weights) {
  util::TokenIter<util::MultiCharacter> pipes(line_, "|||");
  source_ = *pipes;
  lm::ngram::ChartState state;
  lm::ngram::RuleScore<M> scorer(model, state);
  float lm_score = 0.0;
  unsigned int word_count = 0;
  for (util::TokenIter<util::SingleCharacter, true> i(*++pipes, ' '); i; ++i) {
    if (*i->data() == '[') {
      lm_score += scorer.Finish();
      scorer.Reset();
    } else {
      scorer.Terminal(model.GetVocabulary().Index(*i));
      ++word_count;
    }
  }
  lm_score += scorer.Finish();

  score_ = lm_score * weights[0] + static_cast<float>(word_count) * weights[1];
  util::TokenIter<util::SingleCharacter, true> i(*++pipes, ' ');
  for (unsigned int feature = 2; feature < weights.size(); ++i, ++feature) {
    float value = logf(PieceFloat(*i));
    score_ += value * weights[feature];
  }
  UTIL_THROW_IF(i, util::Exception, "Too many features for the number of weights.");
}

struct ScoreGreater : public std::binary_function<const Pair*, const Pair *, bool> {
  bool operator()(const Pair *first, const Pair *second) const {
    return first->Score() > second->Score();
  }
};

void Flush(std::vector<Pair> &pool) {
  std::vector<const Pair*> sorting;
  for (size_t i = 0; i < pool.size(); ++i) {
    sorting.push_back(&pool[i]);
  }
  const size_t passed_mem = std::min<size_t>(sorting.size(), 20);
  std::partial_sort(sorting.begin(), sorting.begin() + passed_mem, sorting.end(), ScoreGreater());
  for (std::vector<const Pair*>::const_iterator i(sorting.begin()); i != sorting.begin() + passed_mem; ++i) {
    std::cout << (*i)->Line() << '\n';
  }
  pool.clear();
}

template <class M> void Loop(const M &model, std::vector<Pair> &pool, const std::vector<float> &weights) {
  StringPiece source("");
  while (true) {
    Pair got;
    if (!getline(std::cin, got.MutableLine())) {
      Flush(pool);
      break;
    }
    got.Parse(model, weights);
    if (got.Source() != source) {
      Flush(pool);
      source = got.Source();
    }
    pool.push_back(got);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Expected LM" << std::endl;
    return 1;
  }
  std::vector<Pair> pool;
  std::vector<float> weights;
  // LM
  weights.push_back(0.203029);
  // Words
  weights.push_back(-0.319567);
  // TM
  weights.push_back(0.0434571);
  weights.push_back(0.0427877);
  weights.push_back(0.0378033);
  weights.push_back(0.0167567);
  weights.push_back(-0.233854);
  // Note: glue doesn't count becuase it's not in the phrase table.

  // Silly weight for wordpenalty.
  weights[1] *= -.434295;

  lm::ngram::ModelType type;
  if (!lm::ngram::RecognizeBinary(argv[1], type)) {
    std::cerr << "not a binary file: " << argv[1] << std::endl;
    return 1;
  }
  switch (type) {
    case lm::ngram::REST_PROBING:
      {
        lm::ngram::RestProbingModel rest(argv[1]);
        Loop(rest, pool, weights);
      }
      break;
    case lm::ngram::PROBING:
      {
        lm::ngram::Model m(argv[1]);
        Loop(m, pool, weights);
      }
      break;
    default:
      std::cerr << "Unsupported model " << std::endl;
      return 1;
  }
}
