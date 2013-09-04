# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name = 'nanomsg'
  s.version = '0.3.1'

  s.authors = ['Kaspar Schiess']
  s.email = 'kaspar.schiess@absurd.li'
  s.extra_rdoc_files = ['README']
  s.files = File.readlines('manifest.txt').map { |l| l.chomp } -
    %w(.gitignore todo nanomsg.gemspec)
  s.homepage = 'https://bitbucket.org/kschiess/nanomsg'
  s.rdoc_options = ['--main', 'README']
  s.require_paths = ['lib', 'ext']
  s.summary = 'Ruby binding for nanomsg. nanomsg library is a high-performance implementation of several "scalability protocols".'
  s.description = <<-EOD
    nanomsg library is a high-performance implementation of several "scalability 
    protocols". Scalability protocol's job is to define how multiple applications 
    communicate to form a single distributed application. WARNING: nanomsg is still in alpha stage!
  EOD

  s.extensions << 'ext/extconf.rb'
  end