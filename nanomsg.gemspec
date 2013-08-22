# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name = 'nanomsg'
  s.version = '0.1.0'

  s.authors = ['Kaspar Schiess']
  s.email = 'kaspar.schiess@absurd.li'
  s.extra_rdoc_files = ['README']
  s.files = %w(HISTORY LICENSE README) + Dir.glob("{lib,ext,examples,spec}/**/*")
  # s.homepage = ''
  s.rdoc_options = ['--main', 'README']
  s.require_paths = ['lib', 'ext']
  s.summary = 'Ruby binding for nanomsg. nanomsg library is a high-performance implementation of several "scalability protocols".'

  s.extensions << 'ext/extconf.rb'
  end