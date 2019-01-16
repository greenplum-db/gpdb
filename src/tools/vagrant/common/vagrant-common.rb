# frozen_string_literal: true

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

