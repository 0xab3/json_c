project('json_impl','c', default_options: ['c_std=c99','warning_level=everything'])
lib=library('json','./json.c')
executable('json_impl','main.c', link_with:lib)
