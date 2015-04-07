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

#include "./unix-socket-protocol.hpp"

namespace shmdata{
namespace UnixSocketProtocol{

onConnectData::onConnectData(size_t shm_size,
                             const std::string &user_data) :
    shm_size_(shm_size),
    user_data_(user_data){
}
    

onConnectDataMaker::onConnectDataMaker(size_t shm_size,
                                       const std::string &user_data) :
    onConnectData(shm_size, user_data),
    iovec_{
  {&shm_size_, sizeof(size_t)},
  {const_cast<char *>(user_data_.c_str()), user_data_.size()}} {
}

socketMsg_t onConnectDataMaker::get_connect_iov(){
  return {(const struct iovec *)iovec_, iovec_len_};
}

}  // namespace UnixSocketProtocol
}  // namespace shmdata
