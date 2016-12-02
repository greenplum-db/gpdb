require 'xmlrpc/client'
require 'uri'

# http://confluence.zutubi.com/display/pulse0206/Remote+API+Reference
class PulseProxy
  MAX_RETRY_ATTEMPTS = 3

  def initialize(url, username, password)
    @url = url
    @username = username
    @password = password
  end

  def refresh
    @token = nil
  end

  def request_triggerBuild(project_name, trigger_options)
    #triggerBuild returns an array of request IDs (strings),
    #but only if we pass trigger_options
    retry_call { proxy.triggerBuild(token, project_name, trigger_options) }
  end

  def waitForBuildRequestToBeActivated(request_id, timeout_seconds)
    end_time = Time.now() + timeout_seconds
    puts 'waiting'
    while Time.now < end_time
      build_request_status = request_getBuildRequestStatus(request_id)
      status = build_request_status['status']
      if status == 'QUEUED' || status == 'UNHANDLED'
        sleep 10
        print '.'
      else
        return build_request_status
      end
    end

    # if it's still queued after the timeout, return a queued
    # build_request_status and let the caller handle it
    request_getBuildRequestStatus(request_id)
  end

  def request_getBuildRequestStatus(request_id)
    retry_call { proxy.getBuildRequestStatus(token, request_id) }
  end

  def request_getBuild(project_name, build_id)
    retry_call { proxy.getBuild(token, project_name, build_id) }
  end

  def request_getMessagesInBuild(project_name, build_id)
    retry_call { proxy.getMessagesInBuild(token, project_name, build_id) }
  end

  def request_getArtifactsInBuild(project_name, build_id)
    retry_call { proxy.getArtifactsInBuild(token, project_name, build_id) }
  end

  # The token will be retrieved one time for this Pulse API connection and
  # reused throughout API calls.
  # Note that the token is only valid for 30 minutes.
  def token
    @token ||= retry_call { proxy.login(@username, @password) }
  end

  def proxy
    @proxy ||= server.proxy('RemoteApi')
  end

  def server
    @server ||= XMLRPC::Client.new2("#{@url}/xmlrpc")
  end

  def retry_call(&block)
    attempts = 0
    begin
      yield
    rescue Net::ReadTimeout, Net::OpenTimeout, SocketError => error
      raise error if attempts > MAX_RETRY_ATTEMPTS
      puts "Got error: #{error} - retry ##{attempts} of #{MAX_RETRY_ATTEMPTS}"
      attempts += 1
      sleep (10 * attempts)
      retry
    end
  end
end
