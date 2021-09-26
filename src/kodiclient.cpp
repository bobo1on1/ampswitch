/*
 * ampswitch
 * Copyright (C) Bob 2021
 * 
 * ampswitch is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ampswitch is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kodiclient.h"
#include "ampswitch.h"

#include <cstdio>
#include <unistd.h>

#include <iostream>
#include <boost/asio.hpp>

#include <nlohmann/json.hpp>

using namespace boost::asio;
using ip::tcp;
using namespace nlohmann;

void CKodiClient::Start(CAmpSwitch* ampswitch)
{
  m_ampswitch = ampswitch;
  m_thread = std::thread(SProcess, this);
}

void CKodiClient::SProcess(CKodiClient* kodiclient)
{
  kodiclient->Process();
}

void CKodiClient::Process()
{
  printf("Kodi client started\n");

  for(;;)
  {
    try
    {
      //TODO: make "localhost" work instead of "127.0.0.1"
      boost::asio::io_service  io_service;
      tcp::socket              socket(io_service);
      boost::asio::ip::address address = boost::asio::ip::make_address("127.0.0.1");

      //Connect to Kodi's JSONRPC, this will thrown an exception on failure.
      socket.connect(tcp::endpoint(address, 9090));

      printf("Connected to Kodi\n");

      //Keep reading data from Kodi, when the tcp socket is closed an exception is thrown.
      for(;;)
      {
        boost::asio::streambuf receive_buffer;
        size_t readbytes = boost::asio::read(socket, receive_buffer, boost::asio::transfer_at_least(1));
        const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());

        //TODO: because of TCP, one read does not exactly equal one JSON object.
        json jsondata = json::parse(data);

        //If Kodi signals a Player.OnPlay notification, the amplifier should be turned on.
        if (jsondata.contains("method"))
        {
          if (jsondata["method"] == "Player.OnPlay")
          {
            printf("Player started\n");
            m_ampswitch->SignalPlayStart();
          }
        }
      }
    }
    catch(boost::system::system_error& error)
    {
      printf("ERROR: unable to connect to Kodi JSONRPC: %s\n", error.what());
      printf("Retrying in 10 seconds\n");
      sleep(10);
    }
  }
}
