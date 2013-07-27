#!/usr/bin/env ruby

class OrderedHash
  attr_reader :arr
  def initialize
    @arr = []
    @table = {}
  end

  def insert(key, default_value)
    if @table[key]
      return @arr[@table[key]][1]
    end
    @table[key] = @arr.size
    @arr << [key, default_value]
    @arr.last[1]
  end

  def find(key)
    @arr[@table[key]][1]
  end
end

def parse_sgm(file)
  sysid = nil
  docid = nil
  ord = OrderedHash.new
  file.each_line do |l|
    l.strip!
    if l.start_with?("<srcset") or l.start_with?("<refset") or l.start_with?("<!DOCTYPE") or l.start_with?("<hl>") or l.start_with?("<p>") or l.start_with?("</p>") or l.start_with?("</hl>") or l.start_with?("</srcset>") or l.start_with?("</refset>")
      next
    elsif l.start_with?("<doc")
      sysid = (l.match /sysid="([^"]*)"/)[1]
      docid = (l.match /docid="([^"]*)"/)[1]
    elsif l.start_with?("<seg")
      throw "Naked seg" unless sysid and docid
      segid = (l.match /id="([^"]*)"/)[1]
      content = (l.match /^<seg id="[^"]*">([^<]*)<\/seg>$/)[1]
      throw "Bad seg " unless content and segid
      ord.insert(sysid, OrderedHash.new).insert(docid, OrderedHash.new).insert(segid, content)
    elsif l.start_with?("</doc>")
      sysid = nil
      docid = nil
    else
      throw "Unrecognized line " + l
    end
  end
  ord
end

unless ARGV[1]
  $stderr.puts "Poorly written program to extract plaintext references from sgm files."
  $stderr.puts "Usage: denistify.rb source.sgm refs.sgm"
  exit 1
end

source = parse_sgm(File.new(ARGV[0]))
refs = parse_sgm(File.new(ARGV[1]))

source.arr[0][1].arr.each do |doc|
  doc[1].arr.each do |seg|
    refs.arr.each do |ref|
      puts ref[1].find(doc[0]).find(seg[0])
    end
  end
end
