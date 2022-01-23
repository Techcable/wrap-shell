project(
  'wrap-shell', 'c',
  license: ['UNLICENSE'],
  default_options : ['warning_level=3']
)

plainlibs = dependency(
  'plainlibs',
  # Builds as a subproject if not found a system
  #
  # Fetches the dependency object from the `plainlib_dep` varaible
  fallback: ['plainlibs', 'plainlib_dep']
)

executable(
  'wrap-shell',
  'src/wrap-shell.c',
  dependencies: [plainlibs]
)

