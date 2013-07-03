#!/usr/bin/env ruby
require 'thread'

queue = Queue.new
number = ARGV[0] ? ARGV[0].to_i : 8

threads = []
number.times do |i|
	threads << Thread.new do
		while (command = queue.pop)
			puts "Thread #{i} running #{command.chomp}.  #{queue.size} remaining."
			unless system(command) then
                          $stderr.puts("ERROR: Could not run " + command)
                          exit 1
                        end
		end
	end
end

while (str = $stdin.gets) do
	queue << str
	puts "#{queue.size} remaining."
end

threads.each { queue << nil }
threads.each { |t| t.join }
