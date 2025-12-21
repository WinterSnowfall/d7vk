ddraw_res = wrc_generator.process('version.rc')

ddraw_src = [
  'ddraw_main.cpp',
  'ddraw_gamma.cpp',
  'ddraw_interface.cpp',
  'ddraw_surface.cpp',
  'ddraw2/ddraw2_interface.cpp',
  'ddraw2/ddraw2_surface.cpp',
  'ddraw2/ddraw3_surface.cpp',
  'ddraw4/ddraw4_interface.cpp',
  'ddraw4/ddraw4_surface.cpp',
  'ddraw7/ddraw7_interface.cpp',
  'ddraw7/ddraw7_surface.cpp',
  'd3d6/d3d6_buffer.cpp',
  'd3d6/d3d6_device.cpp',
  'd3d6/d3d6_interface.cpp',
  'd3d6/d3d6_light.cpp',
  'd3d6/d3d6_material.cpp',
  'd3d6/d3d6_multithread.cpp',
  'd3d6/d3d6_texture.cpp',
  'd3d6/d3d6_viewport.cpp',
  'd3d7/d3d7_buffer.cpp',
  'd3d7/d3d7_interface.cpp',
  'd3d7/d3d7_device.cpp',
  'd3d7/d3d7_state_block.cpp',
  'd3d7/d3d7_multithread.cpp'
]

ddraw_ld_args      = []
ddraw_link_depends = []

if platform != 'windows'
  lib_d3d9 = d3d9_dep
  ddraw_ld_args      += [ '-Wl,--version-script', join_paths(meson.current_source_dir(), 'ddraw.sym') ]
  ddraw_link_depends += files('ddraw.sym')
endif

ddraw_dll = shared_library(dxvk_name_prefix+'ddraw', ddraw_src, ddraw_res,
  dependencies        : [ util_dep, dxso_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'ddraw'+def_spec_ext,
  link_args           : ddraw_ld_args,
  link_depends        : [ ddraw_link_depends ],
  kwargs              : dxvk_so_version,
  link_with           : [ d3d9_static ],
)

ddraw_dep = declare_dependency(
  link_with           : [ ddraw_dll, d3d9_static ],
  include_directories : [ dxvk_include_path ],
)

if platform != 'windows'
  pkg.generate(ddraw_dll,
    filebase: dxvk_pkg_prefix + 'ddraw',
    subdirs:  'dxvk',
  )
endif
