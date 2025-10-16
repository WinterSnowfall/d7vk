ddraw_res = wrc_generator.process('version.rc')

ddraw_src = [
  'ddraw_main.cpp',
  'ddraw7_interface.cpp',
  'ddraw7_surface.cpp',
  'd3d7_buffer.cpp',
  'd3d7_interface.cpp',
  'd3d7_device.cpp',
  'd3d7_state_block.cpp'
]

ddraw_ld_args      = []
ddraw_link_depends = []

if platform != 'windows'
  lib_d3d9 = d3d9_dep
  ddraw_ld_args      += [ '-Wl,--version-script', join_paths(meson.current_source_dir(), 'ddraw.sym') ]
  ddraw_link_depends += files('ddraw.sym')
endif

ddraw_dll = shared_library(dxvk_name_prefix+'ddraw', ddraw_src, ddraw_res,
  dependencies        : [ lib_d3d9, util_dep, dxso_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'ddraw'+def_spec_ext,
  link_args           : ddraw_ld_args,
  link_depends        : [ ddraw_link_depends ],
  kwargs              : dxvk_so_version,
)

ddraw_dep = declare_dependency(
  link_with           : [ ddraw_dll ],
  include_directories : [ dxvk_include_path ],
)

if platform != 'windows'
  pkg.generate(ddraw_dll,
    filebase: dxvk_pkg_prefix + 'ddraw',
    subdirs:  'dxvk',
  )
endif
