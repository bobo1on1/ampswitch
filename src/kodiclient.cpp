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
#include <cstdint>

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

      //Read data from the TCP socket, and split into separate JSON objects.
      ResetSplit();
      for(;;)
      {
        boost::asio::streambuf receive_buffer;
        size_t bytesread = boost::asio::read(socket, receive_buffer,
                                             boost::asio::transfer_at_least(1));

        const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());

        Split(data, bytesread);
      }
    }
    catch(boost::system::system_error& error)
    {
      m_ampswitch->SetPlayingState(false);
      printf("ERROR: unable to connect to Kodi JSONRPC: %s\n", error.what());
      printf("Retrying in 10 seconds\n");
      sleep(10);
    }
  }
}

void CKodiClient::ResetSplit()
{
  m_bracketlevel = 0;
  m_instring     = false;
  m_escaped      = false;
  m_parsebuf.clear();
}

/*! Splits JSON objects from Kodi's JSON RPC and passes them to Parse().*/
void CKodiClient::Split(const char* data, uint32_t len)
{
  //Parse the JSON data into separate JSON objects by finding the text from
  //the starting { to ending }, while ignoring curly brackets in strings.
  for (uint32_t i = 0; i < len; i++)
  {
    m_parsebuf.push_back(data[i]);

    if (!m_instring)
    {
      if (data[i] == '"')
      {
        m_instring = true;
      }
      else if (data[i] == '{')
      {
        m_bracketlevel++;
      }
      else if (data[i] == '}')
      {
        if (m_bracketlevel > 0)
        {
          m_bracketlevel--;
          if (m_bracketlevel == 0)
          { //Parse the full received JSON object.
            Parse(m_parsebuf);
            m_parsebuf.clear();
          }
        }
        else
        {
          m_parsebuf.clear(); //Shouldn't happen.
        }
      }
    }
    else
    {
      if (!m_escaped)
      {
        if (data[i] == '\\')
          m_escaped = true;
        else if (data[i] == '"')
          m_instring = false;
      }
      else
      {
        m_escaped = false;
      }
    }
  }
}

void CKodiClient::Parse(const std::string& jsonstr)
{
  json jsondata = json::parse(jsonstr);

  if (jsondata.contains("method"))
  {
    std::string method = jsondata["method"];

    if (method == "Player.OnPlay")
    {
      printf("Player started\n");
      m_ampswitch->SetPlayingState(true);
    }
    else if (method == "Player.OnStop")
    {
      printf("Player stopped\n");
      m_ampswitch->SetPlayingState(false);
    }
  }
}
