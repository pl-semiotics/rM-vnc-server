{ stdenv, lib, targetPlatform
, libvncserver, linuxHeaders, linuxPackages
, libqsgepaper-snoop, rM-input-devices
}:

stdenv.mkDerivation {
  pname = "rM-vnc-server";
  version = "0.0.2";
  src = lib.cleanSource ./.;
  buildInputs = [
    libvncserver
    linuxHeaders
    linuxPackages.mxc_epdc_fb_damage.dev
    libqsgepaper-snoop
    rM-input-devices.dev
  ];
  REMARKABLE_VERSION = targetPlatform.rmVersion;
  MXC_EPDC_FB_DAMAGE_KO = if targetPlatform.rmVersion == 1
                          then "${linuxPackages.mxc_epdc_fb_damage}/lib/modules/${linuxPackages.kernel.modDirVersion}/drivers/fb/mxc_epdc_fb_damage.ko"
                          else "";
  installPhase = ''
    mkdir -p $out/bin
    cp build/rM*-vnc-server* $out/bin
  '';
}
