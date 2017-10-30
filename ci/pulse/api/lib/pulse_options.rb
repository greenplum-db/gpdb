
class PulseOptions
  ONE_HOUR_IN_SECONDS = 3600
  DEFAULT_TIMEOUT_IN_HOURS = 10
  DEFAULT_POLLING_TIME_IN_SECONDS = 60
  DEFAULT_TOKEN_REFRESH_TIME_IN_SECONDS = 1200
  DEFAULT_BUILD_REQUEST_TIME_IN_SECONDS = 60 * 20
  CONCOURSE_URL = "https://gpdb.data.pivotal.ci/"

  attr_reader :url, :project_name, :username, :password, :input_dir, :output_dir, :build_artifact_url,
              :build_src_code_url, :qautil_url, :gpdb_src_behave_url, :start_time

  def self.forTriggerJob
    PulseOptions.new(needs_output_dir: true)
  end

  def self.forMonitorJob
    PulseOptions.new(needs_input_dir: true)
  end

  def initialize(options = {})
    @input_required = options[:needs_input_dir]
    @output_required = options[:needs_output_dir]
    @start_time = Time.now
  end

  def read_from_environment
    @url = ENV['PULSE_URL']
    @project_name = ENV['PULSE_PROJECT_NAME']
    @username = ENV['PULSE_USERNAME']
    @password = ENV['PULSE_PASSWORD']
    @input_dir = ENV['INPUT_DIR'] if @input_required
    @output_dir = ENV['OUTPUT_DIR'] if @output_required
    ["PULSE_URL", "PULSE_PROJECT_NAME", "PULSE_USERNAME", "PULSE_PASSWORD"].each do |required_var|
      raise "#{required_var} environment variable required" if ENV[required_var].nil?
    end
    raise "INPUT_DIR environment variable required" if @input_dir.nil? && @input_required
    raise "OUTPUT_DIR environment variable required" if @output_dir.nil? && @output_required
  end

  # read the s3 signed URLs from Concourse
  def read_from_concourse_urls(artifact_url, src_code_url, qa_util_url, behave_url)
    @build_artifact_url = File.read(artifact_url).strip
    @build_src_code_url = File.read(src_code_url).strip
    @qautil_url = File.read(qa_util_url).strip
    @gpdb_src_behave_url = File.read(behave_url).strip
  end

  def print_environment_settings
    puts "PULSE_URL is #{url}"
    puts "PULSE_PROJECT_NAME is #{project_name}"
    puts "BUILD_ARTIFACT_URL:\n#{build_artifact_url}\n" unless build_artifact_url.nil?
    puts "BUILD_SRC_CODE_URL:\n#{build_src_code_url}\n" unless build_src_code_url.nil?
    puts "QAUTIL_URL:\n#{qautil_url}\n" unless qautil_url.nil?
    puts "GPDB_SRC_BEHAVE_URL:\n#{gpdb_src_behave_url}\n" unless gpdb_src_behave_url.nil?
  end

  def validate_options
    directory = @input_required ? input_dir : output_dir
    if !File.directory?(directory)
      puts "#{directory} must be a valid directory"
      exit 1
    end
  end

  def build_id
    return @build_id if !@build_id.nil?
    build_id_file = input_dir + "/build_id"
    @build_id = File.read(build_id_file).strip.to_i
    puts "Build ID input: #{@build_id} (read from #{build_id_file})"
    @build_id
  end

  def build_url
    base_url = URI.parse(url + URI.escape("browse/projects/#{project_name}")).to_s
    return "#{base_url}/builds/#{build_id}/"
  end

  def trigger_options
    {
        force: true,
        properties: {
            BUILD_ARTIFACT_URL: build_artifact_url,
            BUILD_SRC_CODE_URL: build_src_code_url,
            QAUTIL_URL: qautil_url,
            GPDB_SRC_BEHAVE_URL: gpdb_src_behave_url,
        },
        reason: "Triggered via Concourse #{CONCOURSE_URL}"
    }
  end

  # Pulse builds can take between 20 minutes to 6-7 hours to complete
  def end_time
    @start_time + (DEFAULT_TIMEOUT_IN_HOURS * ONE_HOUR_IN_SECONDS)
  end

  def timeout
    DEFAULT_TIMEOUT_IN_HOURS
  end

  def polling_time
    DEFAULT_POLLING_TIME_IN_SECONDS + rand(10)
  end

  def build_request_time
    DEFAULT_BUILD_REQUEST_TIME_IN_SECONDS
  end

  # Pulse login tokens expire every 30 minutes; get a new one every 20 minutes
  def new_token_time(refresh = false)
    if refresh
      Time.now + DEFAULT_TOKEN_REFRESH_TIME_IN_SECONDS
    else
      start_time + DEFAULT_TOKEN_REFRESH_TIME_IN_SECONDS
    end
  end

end
