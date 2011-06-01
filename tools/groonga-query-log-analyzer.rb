#!/usr/bin/env ruby

require 'English'
require 'optparse'
require 'cgi'
require 'thread'

class GroongaQueryLogAnaylzer
  def initialize
    setup_options
  end

  def run(argv=nil)
    log_paths = @option_parser.parse!(argv || ARGV)

    parser = QueryLogParser.new
    threads = []
    log_paths.each do |log_path|
      threads << Thread.new do
        File.open(log_path) do |log|
          parser.parse(log)
        end
      end
    end
    threads.each do |thread|
      thread.join
    end

    reporter = ConsoleQueryLogReporter.new(parser.statistics)
    reporter.apply_options(@options)
    reporter.report
  end

  private
  def setup_options
    @options = {}
    @options[:n_entries] = 10
    @options[:order] = "-elapsed"

    @option_parser = OptionParser.new do |parser|
      parser.banner += " LOG1 ..."

      parser.on("-n", "--n-entries=N",
                Integer,
                "Show top N entries",
                "(#{@options[:n_entries]})") do |n|
        @options[:n_entries] = n
      end

      available_orders = ["elapsed", "-elapsed", "start-time", "-start-time"]
      parser.on("--order=ORDER",
                available_orders,
                "Sort by ORDER",
                "available values: [#{available_orders.join(', ')}]",
                "(#{@options[:order]})") do |order|
        @options[:order] = order
      end
    end
  end

  class Command
    class << self
      @@registered_commands = {}
      def register(name, klass)
        @@registered_commands[name] = klass
      end

      def parse(command_path)
        name, parameters_string = command_path.split(/\?/, 2)
        parameters = {}
        parameters_string.split(/&/).each do |parameter_string|
          key, value = parameter_string.split(/\=/, 2)
          parameters[key] = CGI.unescape(value)
        end
        name = name.gsub(/\A\/d\//, '')
        name, output_type = name.split(/\./, 2)
        parameters["output_type"] = output_type if output_type
        command_class = @@registered_commands[name] || self
        command_class.new(name, parameters)
      end
    end

    attr_reader :name, :parameters
    def initialize(name, parameters)
      @name = name
      @parameters = parameters
    end

    def ==(other)
      other.is_a?(self.class) and
        @name == other.name and
        @parameters == other.parameters
    end
  end

  class SelectCommand < Command
    register("select", self)

    def sortby
      @parameters["sortby"]
    end

    def conditions
      @parameters["filter"].split(/(?:&&|&!|\|\|)/).collect do |condition|
        condition = condition.strip
        condition = condition.gsub(/\A[\s\(]*/, '')
        condition = condition.gsub(/[\s\)]*\z/, '') unless /\(/ =~ condition
        condition
      end
    end

    def output_columns
      @parameters["output_columns"]
    end
  end

  class Statistic
    attr_reader :context_id, :start_time, :raw_command
    attr_reader :trace, :elapsed, :return_code
    def initialize(context_id)
      @context_id = context_id
      @start_time = nil
      @command = nil
      @raw_command = nil
      @trace = []
      @elapsed = nil
      @return_code = 0
    end

    def start(start_time, command)
      @start_time = start_time
      @raw_command = command
    end

    def finish(elapsed, return_code)
      @elapsed = elapsed
      @return_code = return_code
    end

    def command
      @command ||= Command.parse(@raw_command)
    end

    def end_time
      @start_time + nano_seconds_to_seconds(@elapsed)
    end

    def label
      "[%s-%s (%8.8f)](%d): %s" % [format_time(start_time),
                                   format_time(end_time),
                                   nano_seconds_to_seconds(elapsed),
                                   return_code,
                                   raw_command]
    end

    def each_trace_report
      previous_elapsed = 0
      ensure_parse_command
      @trace.each_with_index do |(trace_elapsed, trace_label), i|
        relative_elapsed = trace_elapsed - previous_elapsed
        previous_elapsed = trace_elapsed
        trace_label = format_trace_label(trace_label, i) if select_command?
        yield " %2d) %8.8f: %s" % [i + 1,
                                   nano_seconds_to_seconds(relative_elapsed),
                                   trace_label]
      end
    end

    def select_command?
      command.name == "select"
    end

    private
    def format_time(time)
      time.strftime("%Y-%m-%d %H:%M:%S.%u")
    end

    def nano_seconds_to_seconds(nano_seconds)
      nano_seconds / 1000.0 / 1000.0 / 1000.0
    end

    def format_trace_label(label, i)
      case label
      when /\Afilter\(/
        "#{label} <#{@select_command.conditions[i]}>"
      when /\Asort\(/
        "#{label} <#{@select_command.sortby}>"
      when /\Aoutput\(/
        "#{label} <#{@select_command.output_columns}>"
      else
        label
      end
    end

    def ensure_parse_command
      return unless select_command?
      @select_command = SelectCommand.parse(@raw_command)
    end
  end

  class SizedStatistics < Array
    def initialize(size)
      @size = size
    end
  end

  class QueryLogParser
    attr_reader :statistics
    def initialize
      @mutex = Mutex.new
      @statistics = []
    end

    def parse(input)
      statistics = []
      current_statistics = {}
      input.each_line do |line|
        case line
        when /\A(\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)\.(\d+)\|(.+?)\|([>:<])/
          year, month, day, hour, minutes, seconds, micro_seconds =
            $1, $2, $3, $4, $5, $6, $7
          context_id = $8
          type = $9
          rest = $POSTMATCH.strip
          time_stamp = Time.local(year, month, day, hour, minutes, seconds,
                                  micro_seconds)
          parse_line(statistics, current_statistics,
                time_stamp, context_id, type, rest)
        end
      end
      @mutex.synchronize do
        @statistics.concat(statistics)
      end
    end

    private
    def parse_line(statistics, current_statistics,
                   time_stamp, context_id, type, rest)
      case type
      when ">"
        statistic = Statistic.new(context_id)
        statistic.start(time_stamp, rest)
        current_statistics[context_id] = statistic
      when ":"
        return unless /\A(\d+) / =~ rest
        elapsed = $1
        label = $POSTMATCH.strip
        statistic = current_statistics[context_id]
        return if statistic.nil?
        statistic.trace << [elapsed.to_i, label]
      when "<"
        return unless /\A(\d+) rc=(\d+)/ =~ rest
        elapsed = $1
        return_code = $2
        statistic = current_statistics.delete(context_id)
        return if statistic.nil?
        statistic.finish(elapsed.to_i, return_code.to_i)
        statistics << statistic
      end
    end
  end

  class QueryLogReporter
    include Enumerable

    attr_accessor :n_entries
    def initialize(statistics)
      @statistics = statistics
      @order = "-elapsed"
      @n_entries = 10
      @sorted_statistics = nil
    end

    def apply_options(options)
      self.order = options[:order]
      self.n_entries = options[:n_entries]
    end

    def order=(order)
      return if @order == order
      @order = order
      @sorted_statistics = nil
    end

    def sorted_statistics
      @sorted_statistics ||= @statistics.sort_by(&sorter)
    end

    def each
      sorted_statistics.each_with_index do |statistic, i|
        break if i >= @n_entries
        yield statistic
      end
    end

    private
    def sorter
      case @order
      when "elapsed"
        lambda do |statistic|
          -statistic.elapsed
        end
      when "-elapsed"
        lambda do |statistic|
          -statistic.elapsed
        end
      when "-start-time"
        lambda do |statistic|
          -statistic.start_time
        end
      else
        lambda do |statistic|
          statistic.start_time
        end
      end
    end
  end

  class ConsoleQueryLogReporter < QueryLogReporter
    def report
      digit = Math.log10(n_entries).truncate + 1
      each_with_index do |statistic, i|
        puts "%*d) %s" % [digit, i + 1, statistic.label]
        command = statistic.command
        puts "  name: <#{command.name}>"
        puts "  parameters:"
        command.parameters.each do |key, value|
          puts "    <#{key}>: <#{value}>"
        end
        statistic.each_trace_report do |report|
          puts report
        end
        puts
      end
    end
  end
end

if __FILE__ == $0
  analyzer = GroongaQueryLogAnaylzer.new
  analyzer.run
end