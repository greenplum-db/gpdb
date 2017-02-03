require "net/http"
require "uri"

class PulseBuildResult
  MAX_RETRY_ATTEMPTS = 3

  def initialize(pulse_options, build_result, build_messages, build_artifacts)
    @pulse_options = pulse_options
    @build_result = build_result
    @build_messages = build_messages
    @build_artifacts = build_artifacts
  end

  def build_successful?
    @build_result['succeeded']
  end

  def print_build_stats
    puts "Build ID fetched from pulse: #{@build_result['id']}"

    puts "Build " + (build_successful? ? "succeeded" : "failed")
    puts "Revision: #{@build_result['revision']}"
    puts "   Owner: #{@build_result['owner']}"
    puts "  Errors: #{@build_result['errorCount']}"
    puts "Warnings: #{@build_result['warningCount']}"

    seconds = (@build_result['endTimeMillis'].to_i - @build_result['startTimeMillis'].to_i) / 1000
    puts "Duration: " + (@build_result['completed'] ? Time.at(seconds).utc.strftime("%H:%M:%S") : "N/A")

    puts "=== Messages ==="
    @build_messages.each do |message|
      puts "  #{message}"
    end
  end

  def print_build_artifacts
    puts "\n=== Artifacts ===\n"
    @build_artifacts.each do |artifact|
      artifact_name = artifact["name"]
      next unless (artifact_name =~ /\.log/ || artifact_name =~ /\.out/ || artifact_name =~ /output\.txt/)

      # convert "Test Results (results.log) into 'results.log'"
      artifact_name.gsub!(/.*\((.*)\)/) { |match| "#{$1}" }
      uri_string = [@pulse_options.url, artifact["permalink"], URI.escape(artifact_name)].join("")
      puts "\n= #{artifact_name} @ #{uri_string} =\n"
      response = fetch_uri(uri_string)
      response.body.split("\n").each do |line|
        puts colorize(line)
      end
    end
  end

  def fetch_uri(uri_string, limit = 10)
    raise ArgumentError, 'HTTP redirect too deep' if limit == 0
    response = retry_call { Net::HTTP.get_response(URI.parse(uri_string)) }
    case response
    when Net::HTTPSuccess     then response
    when Net::HTTPRedirection then fetch_uri(response['location'], limit - 1)
    else
      response.error!
    end
  end

  def retry_call(&block)
    attempts = 0
    begin
      yield
    rescue Net::HTTPFatalError => error
      raise error if attempts > MAX_RETRY_ATTEMPTS
      puts "Got error: #{error} - retry ##{attempts} of #{MAX_RETRY_ATTEMPTS}"
      attempts += 1
      sleep (10 * attempts)
      retry
    end
  end

  def colorize(message)
    colors = {
      :red => 31,
      :green => 32,
      :yellow => 33,
      :white => 37
    }
    color_code = 37
    case message
    when /\|PASS|Scenario:|ms ... ok/
      color_code = colors[:green]
    when /ms ... skipped|Ran \d+ tests in/
      color_code = colors[:yellow]
    when /FAIL|Traceback|Exception:|, line \d+, in/i
      color_code = colors[:red]
    end
    "\e[#{color_code}m#{message}\e[0m"
  end

end
