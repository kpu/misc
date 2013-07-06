#!/usr/bin/env ruby
require 'pathname'
class OnDisk
  def initialize(dir)
    @dir = dir
  end
  def cat(lm)
    ["Misc", "Source", "TargetColl", "TargetInd", "Vocab"].map do |n|
      @dir + "_" + lm + "/" + n + ".dat"
    end
  end
  def ini(lm)
    "2 0 0 5 " + @dir + "_" + lm
  end
end

class InMemory
  def initialize(file, features)
    @file = file
    @features = features
  end
  def cat(lm)
    @file + "_" + lm
  end
  def ini(lm)
    "6 0 0 " + @features.to_s + " " + @file + "_" + lm
  end
end

class CubeSearch
  def initialize(additional="")
    @add = additional
  end
  def moses_cmd
    @add
  end
  def moses_num
    3
  end
  def cdec_cut_score
    "cut -d \\| -f 10"
  end
  def cdec_cmd(lm, glue)
    "--feature_function \"KLanguageModel -n KLanguageModel #{glue.arg} #{lm}\" --feature_function WordPenalty " + @add
  end
end

class LazySearch
  def moses_cmd
    ""
  end
  def moses_num
    5
  end
  def cdec_cut_score
    "cut -d \\| -f 4"
  end
  def cdec_cmd(lm, glue)
    throw "Incremental only supports fixed bos eos" unless @glue.arg == "-x"
    "--incremental_search " + lm
  end
end

class Decoder
  attr_reader :name, :short
  def initialize(name)
    @name = name
    @short = name.split('-').map do |n| n[0..0] end.join
  end
  
  def configure(scenario)
  end

  def input
    "input"
  end

  def setup
    ""
  end
end

class Moses < Decoder
  def initialize(name, phrase, search)
    super("moses" + (name.empty? ? "" : "-") + name)
    @phrase = phrase
    @search = search
  end

  def cat(lm)
    ["moses.ini", input] + @phrase.map do |p| p.cat(lm) end + [lm]
  end

  def cut_text
    "cut -d \" \" -f 2-"
  end

  def cut_score
    "cut -d \" \" -f 1"
  end

  def line(threading, pop, lm, write_to)
    "~/mosesdecoder/" + if lm == "sri" then
      "82cf7a2/bin/moses_chart -f moses.ini -lmodel-file \"0 0 5 sri\" "
    else
      "bin/moses_chart -f moses.ini -lmodel-file \"8 0 5 #{lm}\""
    end + " -cube-pruning-pop-limit #{pop} -search-algorithm #{@search.moses_num} -output-hypo-score true #{threading.moses_args} -ttable-file " +
    @phrase.map do |p|
      "\"" + p.ini(lm) + "\""
    end.join(' ') + ' ' + @search.moses_cmd() + ' >' + write_to
  end
end

class SentenceGlue
  attr_reader :cname, :suffix, :file, :cut_text, :arg
  def initialize(cname, suffix, file, cut_text, arg)
    @cname = cname
    @suffix = suffix
    @file = file
    @cut_text = cut_text
    @arg = arg
  end
end

BOSFIX = SentenceGlue.new("cdec", ".boseos", "glue-grammar.cdec.boseos", "sed 's/^.*<s> //g;s/ <\\/s>.*$//'", "-x")
BOSCUSTOMBAD = SentenceGlue.new("cbadglue", "", "glue-grammar.cdec", "cat", "")
BOSDEFAULT = SentenceGlue.new("cdefglue", "", nil, "cat", "")

class CDec < Decoder
  def initialize(name, search, glue)
    super(glue.cname + (name.empty? ? "" : "-") + name)
    @search = search
    @glue = glue
  end

  def sentence_level_name
    "cdec_sentence_level" + @glue.suffix
  end

  def configure(scenario)
    sent_level = scenario + '/' + sentence_level_name
    if File.exist?(sent_level)
      @sentence_level = File.new(sent_level).readlines.map do |l|
        matched = l.match /grammar="([^"]*)"/ #/"/
        matched[1].split('/')[0..-2].join('/')
      end.sort.uniq
    end
  end

  def cat(lm)
    if @sentence_level
      @sentence_level
    else
      ["cdec_pruned_" + lm]
    end +
    [input, "weights", "cdec.ini"] + if @glue.file != nil
      [@glue.file]
    else
      []
    end + [lm]
  end

  def input
    if @sentence_level
      "cdec_sentence_level"
    else
      "input"
    end + @glue.suffix
  end

  def cut_text
    @glue.cut_text
  end
  def cut_score
    @search.cdec_cut_score
  end

  def setup
    if @sentence_level
      'sed -i ' + "'" + @sentence_level.map do |s|
        "s|" + s.split('/')[0..-2].join('/') + "|'$SCRATCH'|"
      end.join(';') + "' " + sentence_level_name + "\n"
    else
      ""
    end
  end

  def line(threading, pop, lm, write_to)
    throw "cdec is single threaded" unless threading.name == "single"
    grammar = @sentence_level ? "" : "-g cdec_pruned"
    "~/cdec/decoder/cdec --formalism scfg -c cdec.ini #{grammar} -k 1 --show_weights --cubepruning_pop_limit #{pop} -w weights --add_pass_through_rules " + if @glue.file
      "-n --scfg_extra_glue_grammar #{@glue.file}"
    else
      ""
    end + " " + @search.cdec_cmd(lm,@glue) + " >" + write_to
  end
end

class Joshua < Decoder
  def initialize
    super("joshua")
  end
  def cat(lm)
    throw "TODO: remake packed"
    throw "Joshua only supports probing" unless lm == "probing"
    ["joshua.grammar.packed", input, "glue-grammar.joshua", "joshua.config",lm]
  end
  def cut_text
    "cut -d \\| -f 4"
  end
  def cut_score
    "cut -d \\| -f 10"
  end
  def setup
    "export JOSHUA=$HOME/joshua\n"
  end
  def line(threading, pop, lm, write_to)
    throw "bother to configure threading" unless threading.name == "single"
    "$JOSHUA/joshua-decoder -m 40g -c joshua.config -pop-limit #{pop} >" + write_to
  end
end

class Jane < Decoder
  def initialize
    super("jane")
  end
  def cat(lm)
    throw "Jane can only do probing" unless lm == "probing"
    [input, "jane.opt.config", "jane_poundescape_probing", "jane_pruned_probing"]
  end
  
end

class Threading
  attr_reader :name, :moses_args
  def initialize(name, moses_args)
    @name = name
    @moses_args = moses_args
  end
end

class Trestles
  def logdir
    "/home/zngu/incr"
  end
  def preamble
"#PBS -q normal
#PBS -A cmu126
#PBS -l nodes=1:ppn=32
SCRATCH=/scratch/$USER/$PBS_JOBID
"
  end
  def tempdir(permanent)
    "$SCRATCH"
  end
  def copy_in(files)
"if ps aux |egrep -v \"(xfs|root|zngu|rpc|dbus|68|ntp|PID)\"; then
  echo Somebody else running.
  sleep 600
  exit
fi
" + files.map do |f|
    f.copy("$SCRATCH") + " || echo copy failed.\n"
  end.join +
    "cd $SCRATCH\n"
  end
  def copy_out(files, to)
    "cp " + files.join(' ') + ' ' + to + "\n"
  end
end

class Workhorse
  def logdir
    "/home/kheafiel/incr"
  end
  def preamble
    "#PBS -l nodes=1:ppn=8 -l mem=31500mb\n"
  end
  def tempdir(permanent)
    permanent
  end
  def copy_in
    throw "Broken"
    ""
  end
  def copy_out
    ""
  end
end

class AlreadyExists < Exception
end

class FileSpec
  def initialize(scenario, name)
    @name = name
    @base = @name.split('/')[-1]
    @location = Pathname.new(name).relative? ? (scenario + '/' + name) : name
    if Dir.exists?(@location)
      @directory = true
      @compression = nil
    elsif File.exists?(@location)
      @directory = false
      @compression = nil
    elsif File.exists?(@location + ".bz2")
      @directory = false
      @compression = :bzip
    else
      throw "Missing #{c}"
    end
  end

  def copy(to)
    if @compression == :bzip
      "pbzip2 -d <" +  @name + ".bz2 >" + to + '/' + @base
    elsif @compression == nil
      "cp -Lr " + @name + " " + to
    end
  end

  def cat
    @base + if @directory
      "/*"
    else
      ""
    end
  end
end

def prep_run(cluster, scenario, decoder, threading, lm, pop, permanent, name, input, time)
  raise AlreadyExists, permanent if File.exists?(permanent)
  files = (["ref"] + decoder.cat(lm)).map do |c|
    FileSpec.new(scenario, c)
  end
"#PBS -N #{name}
#PBS -l walltime=#{time}
#PBS -S /bin/bash
#PBS -o #{scenario}/out -e #{scenario}/err
" + cluster.preamble +
"cd #{scenario}
pwd
out=#{cluster.tempdir(permanent)}
if [ -d #{permanent} ]; then echo already exists; exit 1; fi
mkdir -p #{permanent}
ps aux >#{permanent}/ps
mkdir -p $out
" + cluster.copy_in(files) +
  "cat " + files.map do |c|
    c.cat
  end.join(' ') + " >/dev/null\n" +
  decoder.setup +
  "(time " + decoder.line(threading, pop, lm, "$out/output") + " <" + input + ") 2>$out/stderr\n" +
  "tail -n 3 $out/stderr > $out/timing\n"
end


def make_shell(cluster, scenario, decoder, threading, lm, pop)
  permanent = "#{scenario}/logs/#{decoder.name}/#{threading.name}-#{lm}-#{pop}-1"
  prep_run(
      cluster, scenario, decoder, threading, lm, pop,
      permanent, [decoder.short, pop, lm[0..1]].join('-'), decoder.input, "9:00:00") +
  decoder.cut_text + " $out/output >$out/text\n" +
  "~/mosesdecoder/scripts/tokenizer/detokenizer.perl <$out/text >$out/text.detok\n" +
  "score.rb --hyp $out/text --ref ref &\n" +
  "score.rb --hyp $out/text.detok --ref ref &\n" +
  "wait\n" +
  decoder.cut_score + " $out/output |average >$out/score\n" +
  "mkdir -p #{permanent}\n" +
  cluster.copy_out(["text", "text.scores", "text.detok", "text.detok.scores", "stderr", "output", "timing", "score"], permanent)
end

def loading(cluster, scenario, decoder, threading, lm)
  permanent = scenario + "/logs/" + decoder.name + '/' + threading.name + "-" + lm + "-loading"
  prep_run(cluster, scenario, decoder, threading, lm, 100, permanent, decoder.short + "-load", "/dev/null", "0:30:00") +
  "mkdir -p #{permanent}\n" +
  cluster.copy_out(["stderr", "timing"], permanent)
end

def select_decoders(arg)
  moses_memory = [InMemory.new("moses_pruned", 5), InMemory.new("glue-grammar", 1)]
  decoders = [
    Moses.new("", moses_memory, CubeSearch.new),
    Moses.new("phil", moses_memory, CubeSearch.new("-cube-pruning-lazy-scoring")),
    Moses.new("lazy-fillin", moses_memory, LazySearch.new)
  ]
  [BOSFIX, BOSCUSTOMBAD, BOSDEFAULT].each do |glue|
    decoders += [
      CDec.new("", CubeSearch.new, glue),
      CDec.new("iwslt1", CubeSearch.new("--intersection_strategy fast_cube_pruning"), glue),
      CDec.new("iwslt2", CubeSearch.new("--intersection_strategy fast_cube_pruning_2"), glue)
    ]
  end
  #lazy only works with fixed bos
  decoders << CDec.new("lazy-fillin", LazySearch.new, BOSFIX)
  decoders << Joshua.new
  decoder_map = {}
  decoders.each do |d|
    decoder_map[d.name] = d
  end
  if arg == "all" then
    decoders
  else
    get = decoder_map[arg]
    throw "Unknown decoder #{arg}" unless get
    [get]
  end
end

def get_scenario(arg, cluster, decoders)
  scenario = cluster.logdir + "/" + arg
  throw "Missing #{scenario}" unless File.exists?(scenario)
  decoders.each do |d|
    d.configure(scenario)
  end
#    d.cat("probing")
#  end.flatten.uniq.each do |name|
#    name = scenario + '/' + name unless name.start_with?('/')
#    throw "Missing #{name}" unless File.exists?(name)
#  end
  scenario
end

unless ARGV[2] then
  $stderr.puts "scenario decoders pops lms"
  exit 1
end


pops = if ARGV[2] == "all" then
  [5, 1000, 750, 500, 400, 300, 200, 150, 100, 75, 50, 25, 20, 15, 10]
else
  [ARGV[2]]
end

lms = if ARGV[3] and ARGV[3] != "all" then
  ARGV[3].split
else
  ["lower", "probing"]
end

def submit(str)
  file = IO.popen('qsub', 'w')
  file.write(str)
  file.close
end

cluster = case File.new(ENV["HOME"] + "/.cluster").read.strip
when "Trestles"
  Trestles.new
when "Workhorse"
  Workhorse.new
else
  throw "Unfamilar with this cluster."
end
decoders = select_decoders(ARGV[1])
scenario = get_scenario(ARGV[0], cluster, decoders)

#threading = Threading.new("multi", "-threads all")
threading = Threading.new("single", "")
decoders.each do |d|
  pops.each do |p|
    lms.each do |l|
      begin
        ret = if p == "loading"
          loading(cluster, scenario, d, threading, l)
        else
          throw "Bad pop limit #{p}" if p.to_i == 0
          make_shell(cluster, scenario, d, threading, l, p.to_i)
        end
        puts ret
#        submit(ret)
      rescue AlreadyExists
        puts $!.to_s + " already exists"
      end
    end
  end
end

