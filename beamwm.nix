{ lib, stdenv, libX11, libXft, libxinerama, xorg, freetype, fontconfig, pkg-config, gnumake }:

stdenv.mkDerivation rec {
  pname = "beamwm";
  version = "0.1.0";
  src = ./. + "";
  nativeBuildInputs = [ pkg-config gnumake ];
  buildInputs = [ 
    libX11 
    libXft 
    libxinerama
    fontconfig 
    freetype 
  ];

  patchPhase = ''
    substituteInPlace Makefile \
      --replace "-I/usr/include/freetype2" "$(pkg-config --cflags freetype2)"
  '';
  makeFlags = [ "PREFIX=$(out)" ];
  passthru.providedSessions = [ "beamwm" ];
  meta = with lib; {
    description = "Minimalist wm";
    platforms = platforms.linux;
  };
}
