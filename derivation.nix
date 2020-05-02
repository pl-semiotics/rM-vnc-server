{ stdenv, lib, libvncserver, linuxHeaders, linuxPackages }:

stdenv.mkDerivation {
  pname = "rM-vnc-server";
  version = "0.0.1";
  src = lib.cleanSource ./.;
  buildInputs = [ libvncserver
                  linuxHeaders
                  linuxPackages.mxc_epdc_fb_damage.dev ];
  buildPhase = ''
    $CC -g vncs.c -o rM-vnc-server -lvncserver -lz -lpthread
  '';
  installPhase = ''
    mkdir -p $out/bin
    cp rM-vnc-server $out/bin
  '';
}
