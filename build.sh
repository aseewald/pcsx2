#!/bin/sh

# PCSX2 - PS2 Emulator for PCs
# Copyright (C) 2002-2011  PCSX2 Dev Team
#
# PCSX2 is free software: you can redistribute it and/or modify it under the terms
# of the GNU Lesser General Public License as published by the Free Software Found-
# ation, either version 3 of the License, or (at your option) any later version.
#
# PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with PCSX2.
# If not, see <http://www.gnu.org/licenses/>.

flags=""
clean_build=false

for f in $*
do
	case $f in
		--dev|--devel)
			flags="$flags -DCMAKE_BUILD_TYPE=Devel"
			;;
		--debug)
			flags="$flags -DCMAKE_BUILD_TYPE=Debug"
			;;
		--release)
			flags="$flags -DCMAKE_BUILD_TYPE=Release"
			;;
        --glsl)
			flags="$flags -DGLSL_API=TRUE"
            ;;
        --egl)
            echo "Warning EGL needs at lesat MESA 9.0"
			flags="$flags -DEGL_API=TRUE"
            ;;
		--clean)
			clean_build=true
			;;
		*)
			# unknown option
			echo "Valid options are:"
			echo "--dev / --devel - Build PCSX2 as a Development build."
			echo "--debug         - Build PCSX2 as a Debug build."
			echo "--release       - Build PCSX2 as a Release build."
			echo "--clean         - Do a clean build."
            echo "--glsl          - Replace CG backend of ZZogl by GLSL"
            echo "--egl           - Replace GLX by EGL (ZZogl plugins only)"
			exit 1;;
  	esac
done

rm install_log.txt

if [ "$flags" != "" ]; then
	echo "Building pcsx2 with $flags"
	echo "Building pcsx2 with $flags" > install_log.txt
fi

if [ "$clean_build" = true ]; then
	echo "Doing a clean build."
    rm -fr build
	# make clean 2>&1 | tee -a ../install_log.txt 
fi


mkdir -p build
cd build

cmake $flags .. 2>&1 | tee -a ../install_log.txt 

CORE=`grep -w -c processor /proc/cpuinfo`
make -j $CORE 2>&1 | tee -a ../install_log.txt 
make install 2>&1 | tee -a ../install_log.txt 

cd ..
