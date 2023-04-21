project('libdlb_lip', 'c')

inc = include_directories('include')
src = ['src/dlb_lip_cache.c', 'src/dlb_lip_cmd_builder.c', 'src/dlb_lip_osa.c', 'src/dlb_lip.c']

libdlb_lip = static_library('dlb_lip',
  src,
  include_directories : inc,
  install : true)

libdlb_lip_dep = declare_dependency(include_directories : inc,
  link_with : libdlb_lip)