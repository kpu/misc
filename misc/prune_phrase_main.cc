#include "lm/left.hh"
#include "lm/model.hh"
#include "util/tokenize_piece.hh"

#include <functional>
#include <string>
#include <vector>

class Pair {
  public:
    std::string &MutableLine() { return line_; }
    const std::string &Line() const { return line_; }

    void Parse(const lm::ngram::RestProbingModel &model, const std::vector<float> &weights);

    StringPiece Source() const { return source_; }

    float EGivenF() const { return e_given_f_; }

    float Score() const { return score_; }

  private:
    std::string line_;
    StringPiece source_;
    float e_given_f_;
    float score_;
};

float PieceFloat(StringPiece str) {
  char *end;
  float ret = strtof(str.data(), &end);
  UTIL_THROW_IF(end != str.data() + str.size(), util::Exception, "Bad number " << str);
  return ret;
}

void Pair::Parse(const lm::ngram::RestProbingModel &model, const std::vector<float> &weights) {
  util::TokenIter<util::MultiCharacter> pipes(line_, "|||");
  source_ = *++pipes;
  lm::ngram::ChartState state;
  lm::ngram::RuleScore<lm::ngram::RestProbingModel> scorer(model, state);
  float lm_score = 0.0;
  unsigned int word_count = 0;
  for (util::TokenIter<util::SingleCharacter, true> i(*++pipes, ' '); i; ++i) {
    if (i->size() > 3 && !memcmp(i->data(), "[X,", 3)) {
      lm_score += scorer.Finish();
      scorer.Reset();
    }
    scorer.Terminal(model.GetVocabulary().Index(*i));
    ++word_count;
  }
  lm_score += scorer.Finish();

  score_ = lm_score * weights[0] + static_cast<float>(word_count) * weights[1];
  util::TokenIter<util::SingleCharacter, true> i(*++pipes, ' ');
  for (unsigned int feature = 2; feature < weights.size(); ++i, ++feature) {
    float value = PieceFloat(*i);
    score_ += value * weights[feature];
//    if (feature == 4) e_given_f_ = value;
  }
  UTIL_THROW_IF(i, util::Exception, "Too many features for the number of weights.");
}

struct EGivenFGreater : public std::binary_function<const Pair*, const Pair *, bool> {
  bool operator()(const Pair *first, const Pair *second) const {
    return first->EGivenF() > second->EGivenF();
  }
};

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
/*  const size_t passed_ondisk = std::min<size_t>(sorting.size(), 100);
  std::partial_sort(sorting.begin(), sorting.begin() + passed_ondisk, sorting.end(), EGivenFGreater());*/
  const size_t passed_ondisk = sorting.size();
  const size_t passed_mem = std::min<size_t>(sorting.size(), 20);
  std::partial_sort(sorting.begin(), sorting.begin() + passed_mem, sorting.begin() + passed_ondisk, ScoreGreater());
  for (std::vector<const Pair*>::const_iterator i(sorting.begin()); i != sorting.begin() + passed_mem; ++i) {
    std::cout << (*i)->Line() << '\n';
  }
  pool.clear();
}

int main() {
  std::vector<Pair> pool;
  StringPiece source("");
  lm::ngram::RestProbingModel rest("lm");
  std::vector<float> weights;
  weights.push_back(0.203029);
  weights.push_back(-0.319567);
  weights.push_back(0.0434571);
  weights.push_back(0.0427877);
  weights.push_back(0.0378033);
  weights.push_back(0.0167567);
  weights.push_back(-0.233854);
  while (true) {
    Pair got;
    if (!getline(std::cin, got.MutableLine())) {
      Flush(pool);
      break;
    }
    got.Parse(rest, weights);
    if (got.Source() != source) {
      Flush(pool);
      source = got.Source();
    }
    pool.push_back(got);
  }
}
