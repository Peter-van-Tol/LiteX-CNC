#!/bin/bash
#
#    Simple bash script for compiling all LitexCNC components
#
#    Copyright (C) 2022 Peter van Tol
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#

clean=''

print_usage() {
  printf "Usage: Flag -c for cleaning directory after use."
}

while getopts 'c' flag; do
  case "${flag}" in
    c) clean='true' ;;
    *) print_usage
       exit 1 ;;
  esac
done

# Make the components
sudo make install -f base.Makefile
sudo make install -f debug.Makefile
sudo make install -f eth.Makefile

# Clean the directory of all files (they are copied to LinuxCNC anyway)
if $clean
then
  rm -f -v *.o *.so *.sym *.tmp *.ver
fi
