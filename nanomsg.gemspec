# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name = 'nanomsg'
  s.version = '0.1.0'

  s.authors = ['Kaspar Schiess']
  s.email = 'kaspar.schiess@absurd.li'
  s.extra_rdoc_files = ['README']
  s.files = %w(HISTORY LICENSE README) + Dir.glob("{lib,ext,examples,spec}/**/*")
  s.homepage = 'https://bitbucket.org/kschiess/nanomsg'
  s.rdoc_options = ['--main', 'README']
  s.require_paths = ['lib', 'ext']
  s.summary = 'Ruby binding for nanomsg. nanomsg library is a high-performance implementation of several "scalability protocols".'
  s.description = <<-EOD
    nanomsg provides idiomatic Ruby bindings to the C library nanomsg (nanomsg.org)

    nanomsg library is a high-performance implementation of several "scalability 
    protocols". Scalability protocol's job is to define how multiple applications 
    communicate to form a single distributed application. Implementation of 
    following scalability protocols is available at the moment:

      PAIR - simple one-to-one communication
      BUS - simple many-to-many communication
      REQREP - allows to build clusters of stateless services to process user requests
      PUBSUB - distributes messages to large sets of interested subscribers
      FANIN - aggregates messages from multiple sources
      FANOUT - load balances messages among many destinations
      SURVEY - allows to query state of multiple applications in a single go

    WARNING: nanomsg is still in alpha stage!
  EOD

  s.extensions << 'ext/extconf.rb'
  end