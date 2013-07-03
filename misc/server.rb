#!/usr/bin/env ruby
require 'socket'
require 'thread'
require 'shellwords'
serv = UNIXServer.new("/tmp/socket")
moses = IO.popen(Shellwords.join(ARGV), "w+")
while c = serv.accept
  c.send_io(moses)
  while l = c.gets
    moses.puts l
  end
  c.close
end
