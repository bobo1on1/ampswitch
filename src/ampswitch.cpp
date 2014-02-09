/*
 * ampswitch
 * Copyright (C) Bob 2014
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

#include "ampswitch.h"

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE //for pipe2
#endif //_GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>

volatile bool g_stop = false;

CAmpSwitch::CAmpSwitch(int argc, char *argv[])
{
  m_connected = false;
  m_client = NULL;
  m_port = NULL;
  m_switchedon = false;
  m_triggerlevel = 0.5f;
  m_pipe[0] = -1;
  m_pipe[1] = -1;
  m_switchtime = 10.0f;
  m_samplecounter = 0;
  m_samplerate = 0;
}

CAmpSwitch::~CAmpSwitch()
{
}

bool CAmpSwitch::Setup()
{
  signal(SIGINT, SignalHandler);
  signal(SIGINT, SignalHandler);

  if (pipe2(m_pipe, O_NONBLOCK) == -1)
  {
    printf("ERROR: Creating pipe: %s\n", strerror(errno));
    return false;
  }

  return true;
}

void CAmpSwitch::Process()
{
  while (!g_stop)
  {
    if (!m_connected)
      Connect();

    fd_set pipeset;
    FD_ZERO(&pipeset);
    FD_SET(m_pipe[0], &pipeset);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    select(m_pipe[0] + 1, &pipeset, NULL, NULL, &tv);

    if (FD_ISSET(m_pipe[0], &pipeset))
      printf("Current state: %s\n", m_switchedon ? "on" : "off");

    uint8_t byte;
    while (read(m_pipe[0], &byte, 1) == 1);

    sleep(1);
  }
}

void CAmpSwitch::Cleanup()
{
  JackDisconnect();
}

void CAmpSwitch::Connect()
{
  m_connected = JackConnect();
  if (!m_connected)
  {
    JackDisconnect();

    printf("Waiting 10 seconds before trying again\n");
    sleep(10);
  }
}

bool CAmpSwitch::JackConnect()
{
  m_client = jack_client_open("Ampswitch", JackNoStartServer, NULL);

  if (m_client == NULL)
  {
    printf("ERROR: Unable to connect to jack\n");
    return false;
  }

  int returnv;

  m_samplerate = jack_get_sample_rate(m_client);

  returnv = jack_set_process_callback(m_client, SJackProcessCallback, this);
  if (returnv != 0)
  {
    printf("ERROR: Unable to set process callback\n");
    return false;
  }

  m_port = jack_port_register(m_client, "Input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  if (m_port == NULL)
  {
    printf("ERROR: Unable to register jack port\n");
    return false;
  }

  returnv = jack_activate(m_client);
  if (returnv != 0)
  {
    printf("ERROR: Unable to activate jack client\n");
    return false;
  }

  printf("Connected to jack\n");

  return true;
}

void CAmpSwitch::JackDisconnect()
{
  if (m_client)
  {
    printf("Disconnecting from jack\n");

    jack_port_unregister(m_client, m_port);
    m_port = NULL;

    jack_deactivate(m_client);
    jack_client_close(m_client);
    m_client = NULL;
  }
}

int CAmpSwitch::SJackProcessCallback(jack_nframes_t nframes, void *arg)
{
  return ((CAmpSwitch*)arg)->PJackProcessCallback(nframes);
}

int CAmpSwitch::PJackProcessCallback(jack_nframes_t nframes)
{
  float* jackptr = (float*)jack_port_get_buffer(m_port, nframes);

  float* in = jackptr;
  float* inend = in + nframes;
  while (in != inend)
  {
    if (m_samplecounter == 0)
    {
      bool trigger = fabsf(*(in++)) > m_triggerlevel;
      bool change = (m_switchedon && !trigger) || (!m_switchedon && trigger);
      if (change)
      {
        m_switchedon = !m_switchedon;
        m_samplecounter = m_switchtime * m_samplerate;

        uint8_t msgbyte = 0;
        write(m_pipe[1], &msgbyte, 1);
      }
    }
    else
    {
      m_samplecounter--;
    }
  }

  return 0;
}

void CAmpSwitch::SignalHandler(int signum)
{
  if (signum == SIGINT || signum == SIGKILL)
    g_stop = true;
}

