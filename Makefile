# m9312 - Decode DEC boot PROMs as used on M9312 Bootstrap/Terminator
#         module.
# Makefile
# Copyright 2004, 2005, 2019 Eric Smith <spacewar@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.  Note that permission is
# not granted to redistribute this program under the terms of any
# other version of the General Public License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111  USA

CFLAGS = -g -Wall -Wextra

all: m9312

m9312: m9312.o
