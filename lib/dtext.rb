# frozen_string_literal: true

require "dtext/dtext"
require "dtext/version"
require "dtext/ruby"

begin
  require "zeitwerk"

  loader = Zeitwerk::Loader.for_gem
  loader.enable_reloading
  loader.inflector.inflect("dtext" => "DText")
  #loader.logger = Logger.new(STDERR)
  loader.setup
rescue LoadError
end

class DText
  class Error < StandardError; end

  def self.parse(str, inline: false, disable_mentions: false, allow_color: false, max_thumbs: 25, base_url: nil, domain: nil, internal_domains: [])
    c_parse(str, base_url, domain, internal_domains, inline, disable_mentions, allow_color, max_thumbs)
  end
end
