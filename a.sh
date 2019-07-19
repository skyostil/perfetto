tools/gen_amalgamated --out amalgamated --keep --gn_args 'cc_wrapper="ccache" target_os="linux" target_cpu="x64" is_debug=false enable_perfetto_watchdog=false'
cp out/amalgamated/perfetto.{cc,h} ~/proj/galaxy2d/third_party/perfetto-sdk
