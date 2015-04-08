Jasmine.configure do |config|
  project_dir = File.expand_path('../../../../misc/..', File.dirname(__FILE__))
  config.spec_dir = project_dir
  config.spec_files = lambda { Dir["#{project_dir}/misc/spec/javascripts/helpers/**/*.js"] + Dir["#{project_dir}/misc/js/jquery.min.js"] + Dir["#{project_dir}/misc/**/*[sS]pec.js"] }
  js_tmp_dir = File.expand_path('pushstream/js', Dir.tmpdir)
  config.src_dir = js_tmp_dir
  config.src_files = lambda { Dir["#{js_tmp_dir}/**/*.js"] }
end
