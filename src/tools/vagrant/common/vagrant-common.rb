# frozen_string_literal: true

require 'yaml'

def process_vagrant_local(config)
  return unless File.file? 'vagrant-local.yml'
  local_config = YAML.load_file 'vagrant-local.yml'
  local_config['synced_folder'].each do |folder|
    next if folder['local'].nil? || folder['shared'].nil?
    args = {}
    folder.each do |k, v|
      next if %w[folder local shared].include? k
      args[k.to_sym] = v
    end
    if args[:type] == 'nfs'
      config.vm.network :private_network, ip: '192.168.10.201'
    end
    config.vm.synced_folder folder['local'], folder['shared'], **args
  end
end

@args = %w[--enable-debug --with-python --with-perl --with-libxml]
@ld_lib_path = ['LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH']

def gpdb_build_args(with_gporca: true)
  return @args + ['--disable-orca'] unless with_gporca
  @args + @ld_lib_path
end

def name_vm(config, name)
  config.vm.provider :virtualbox do |vb|
    vb.name = name
  end
end

def all_remotes
  remotes = []
  `git remote -v | grep '\(fetch\)$'`.split(/\n/).each do |remote|
    remote_arr = remote.split(/\t/)
    remotes << { name: remote_arr[0], path: remote_arr[1].split(/ /)[0] }
  end
  remotes
end

# returns a hash with { ENV_NAME: value }
# can be loaded into vagrant-shell's env
def current_git_data
  current = `git branch -vv | grep '^*' | grep -o '\\[.*\\]'`.chomp "]\n"
  current[0] = ''
  r_and_b = current.split(%r{[:/]})
  env_vars = {
    CURRENT_GIT_REMOTE: r_and_b[0],
    CURRENT_GIT_BRANCH: r_and_b[1],
    CURRENT_GIT_USER_NAME: `git config user.name`.strip,
    CURRENT_GIT_USER_EMAIL: `git config user.email`.strip
  }
  all_remotes.each_with_index do |remote, index|
    if remote[:name] == 'origin'
      env_vars[:GIT_ORIGIN_PATH] = remote[:path]
      next
    end
    remote_name_var = 'GIT_REMOTE_NAME_' + index.to_s
    remote_path_var = 'GIT_REMOTE_PATH_' + index.to_s
    env_vars[remote_name_var.to_sym] = remote[:name]
    env_vars[remote_path_var.to_sym] = remote[:path]
  end
  env_vars
end
