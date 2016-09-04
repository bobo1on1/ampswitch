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
#include <getopt.h>

volatile bool g_stop = false;

CAmpSwitch::CAmpSwitch(int argc, char *argv[])
{
  m_connected     = false;
  m_client        = NULL;
  m_port          = NULL;
  m_switchedon    = false;
  m_triggerlevel  = 0.5f;
  m_pipe[0]       = -1;
  m_pipe[1]       = -1;
  m_switchtime    = 10.0f;
  m_samplecounter = 0;
  m_samplerate    = 0;
  m_jackshutdown  = false;
  m_oncommand     = NULL;
  m_offcommand    = NULL;

  struct option longoptions[] =
  {
   {"on-command",    required_argument, NULL, 'n'},
   {"off-command",   required_argument, NULL, 'f'},
   {"switch-time",   required_argument, NULL, 's'},
   {"trigger-level", required_argument, NULL, 't'},
   {"help",          no_argument,       NULL, 'h'},
   {0, 0, 0, 0}
  };

  const char* shortoptions = "n:f:s:t:h";
  int c;
  int optionindex = 0;
  while ((c = getopt_long(argc, argv, shortoptions, longoptions, &optionindex)) != -1)
  {
    if (c == 'n')
    {
      m_oncommand = optarg;
    }
    else if (c == 'f')
    {
      m_offcommand = optarg;
    }
    else if (c == 'h')
    {
      PrintHelpMessage();
      exit(1);
    }
    else if (c == 's')
    {
      float switchtime;
      if (sscanf(optarg, "%f", &switchtime) != 1)
      {
        printf("ERROR: Wrong value for switch-time: \"%s\"\n", optarg);
        exit(1);
      }

      m_switchtime = switchtime;
    }
    else if (c == 't')
    {
      float triggerlevel;
      if (sscanf(optarg, "%f", &triggerlevel) != 1)
      {
        printf("ERROR: Wrong value for trigger-level: \"%s\"\n", optarg);
        exit(1);
      }

      m_triggerlevel = triggerlevel;
    }
    else if (c == '?')
    {
      exit(1);
    }
  }
}

CAmpSwitch::~CAmpSwitch()
{
}

bool CAmpSwitch::Setup()
{
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  if (pipe2(m_pipe, O_NONBLOCK) == -1)
  {
    printf("ERROR: Creating pipe: %s\n", strerror(errno));
    return false;
  }

  if (m_oncommand)
    printf("on command: \"%s\"\n", m_oncommand);
  if (m_offcommand)
    printf("off command: \"%s\"\n", m_offcommand);

  return true;
}

void CAmpSwitch::Process()
{
  if (m_offcommand)
  {
    printf("switching off, executing \"%s\"\n", m_offcommand);
    system(m_offcommand);
  }

  while (!g_stop)
  {
    if (m_jackshutdown)
      JackDisconnect();

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
    {
      if (m_switchedon && m_oncommand)
      {
        printf("switching on, executing \"%s\"\n", m_oncommand);
        system(m_oncommand);
      }
      else if (!m_switchedon && m_offcommand)
      {
        printf("switching off, executing \"%s\"\n", m_offcommand);
        system(m_offcommand);
      }

      uint8_t byte;
      while (read(m_pipe[0], &byte, 1) == 1);
    }
  }
}

void CAmpSwitch::Cleanup()
{
  JackDisconnect();
}

void CAmpSwitch::PrintHelpMessage()
{
  printf(
         "\n"
         "usage: ampswitch [OPTION]\n"
         "\n"
         "  options:\n"
         "\n"
         "    -n, --on-command    command to execute when switching on\n"
         "    -f, --off-command   command to execute when switching off\n"
         "    -s, --switch-time   minimum number of seconds between switches\n"
         "    -t, --trigger-level absolute value of trigger level\n"
         "    -h, --help          print this message\n"
         "\n"
         );
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

  jack_on_info_shutdown(m_client, SJackInfoShutdownCallback, this);

  returnv = jack_set_process_callback(m_client, SJackProcessCallback, this);
  if (returnv != 0)
  {
    printf("ERROR: Unable to set process callback\n");
    return false;
  }

  returnv = jack_set_sample_rate_callback(m_client, SJackSamplerateCallback, this);
  if (returnv != 0)
    printf("ERROR: Unable to set sample rate callback\n");

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

    jack_client_close(m_client);
    m_port = NULL;
    m_client = NULL;
  }
  m_jackshutdown = false;
  m_connected = false;
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

int CAmpSwitch::SJackSamplerateCallback(jack_nframes_t nframes, void *arg)
{
  return ((CAmpSwitch*)arg)->PJackSamplerateCallback(nframes);
}

int CAmpSwitch::PJackSamplerateCallback(jack_nframes_t nframes)
{
  m_samplecounter = (double)m_samplecounter / (double)m_samplerate * (double)nframes;
  m_samplerate = nframes;

  return 0;
}

void CAmpSwitch::SJackInfoShutdownCallback(jack_status_t code, const char *reason, void *arg)
{
  ((CAmpSwitch*)arg)->PJackInfoShutdownCallback(code, reason);
}

void CAmpSwitch::PJackInfoShutdownCallback(jack_status_t code, const char *reason)
{
  uint8_t msgbyte = 0;
  write(m_pipe[1], &msgbyte, 1);

  m_jackshutdown = true;
}

void CAmpSwitch::SignalHandler(int signum)
{
  if (signum == SIGINT || signum == SIGTERM)
    g_stop = true;
}

