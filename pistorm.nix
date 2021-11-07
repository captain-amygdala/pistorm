{ stdenv, libraspberrypi, alsaLib, openocd, makeWrapper }:

stdenv.mkDerivation {
  name = "pistorm";
  src = ./.;
  enableParallelBuilding = false;
  buildInputs = [ libraspberrypi alsaLib makeWrapper ];
  installPhase = ''
    find | grep --color buptest
    pwd
    ls -ltrh
    mkdir -pv $out/bin $out/share/pistorm
    cp -vi emulator buptest $out/bin/
    cp -vr rtl $out/share/pistorm
    cp -vr nprog $out/share/pistorm
    cp -v *.cfg $out/share/pistorm
    cp -vr data $out/share/pistorm
    mkdir -pv $out/share/pistorm/platforms/amiga/piscsi
    cp -v platforms/amiga/piscsi/piscsi.rom $out/share/pistorm/platforms/amiga/piscsi/
    cp -v platforms/amiga/pistorm.hdf $out/share/pistorm/platforms/amiga/

    mkdir -pv $out/share/pistorm/platforms/amiga/rtg
    cp -v platforms/amiga/rtg/*.shader $out/share/pistorm/platforms/amiga/rtg/

    cat <<EOF > $out/bin/pistorm
    #!${stdenv.shell}
    cd $out/share/pistorm
    $out/bin/emulator "\''${@}"
    EOF
    chmod +x $out/bin/pistorm

    cp -v *.sh $out/bin/
    #wrapProgram $out/bin/nprog.sh --prefix PATH : {openocd}/bin
  '';
}
