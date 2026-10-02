{
  pkgs,
  sources,
  dependencies,
  target ? "host",
  configuration ? "release",
  toolchain ? { },
  schemaVersion ? 1,
}:
assert toolchain == { };
assert schemaVersion == 1 && target == "host";
pkgs.stdenv.mkDerivation {
  pname = "helios-virglrenderer";
  version = "1.3.0";
  src = sources.virglrenderer;
  nativeBuildInputs = [
    pkgs.meson
    pkgs.ninja
    pkgs.pkg-config
    (pkgs.python3.withPackages (p: [ p.pyyaml ]))
  ];
  buildInputs = [
    pkgs.libepoxy
    pkgs.libdrm
    pkgs.libgbm
    pkgs.libGLU
    pkgs.libx11
    pkgs.vulkan-headers
    pkgs.vulkan-loader
    dependencies.protocol
    pkgs.check
  ];
  mesonBuildType = if configuration == "debug" then "debug" else "debugoptimized";
  mesonWrapMode = "nodownload";
  mesonFlags = [
    "-Dvenus=true"
    "-Dvulkan-dload=false"
    "-Dtests=true"
    "-Dvideo=false"
    "-Dtracing=none"
    "-Ddrm-renderers=[]"
  ];
  separateDebugInfo = true;
  doCheck = true;
  checkPhase = ''
    runHook preCheck
    meson test venus_queue_sync venus_fault_trace venus_fault_dispatch test_virgl_strbuf test_fuzzer_formats --print-errorlogs
    runHook postCheck
  '';
  postInstall = ''
    mkdir -p $out/share/licenses/virglrenderer
    cp ../COPYING $out/share/licenses/virglrenderer/
  '';
}
