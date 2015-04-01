/*
 * Copyright (C) 2015 Nicolas Bouillot (http://www.nicolasbouillot.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

#include <cassert>
#include "shmdata/sysv-shm.hpp"

int main () {
  using namespace shmdata;
  {
    sysVShm shm(1234, 1024, IPC_CREAT | IPC_EXCL | 0666);
    assert(shm);
  }
  {
    sysVShm shm(1234, 1024, IPC_CREAT | IPC_EXCL | 0666);
    assert(shm);
  }
  return 0;
}

