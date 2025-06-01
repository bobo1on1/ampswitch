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
#include <algorithm>

volatile bool g_stop = false;

CAmpSwitch::CAmpSwitch(int argc, char *argv[])
{
  m_triggerlevel  = 0.1f;
  m_switchtime    = 10.0f;
  m_oncommand     = NULL;
  m_offcommand    = NULL;
  m_pipe[0]       = -1;
  m_pipe[1]       = -1;
  m_connected     = false;
  m_jackname      = "Ampswitch";
  m_client        = NULL;
  m_port          = NULL;
  m_samplerate    = 0;
  m_jackshutdown  = false;
  m_switchedon    = false;
  m_samplecounter = 0;
  m_usekodi       = false;
  m_playing       = false;

  struct option longoptions[] =
  {
   {"jack-name",     required_argument, NULL, 'j'},
   {"on-command",    required_argument, NULL, 'n'},
   {"off-command",   required_argument, NULL, 'f'},
   {"switch-time",   required_argument, NULL, 's'},
   {"trigger-level", required_argument, NULL, 't'},
   {"kodi",          no_argument,       NULL, 'k'},
   {"help",          no_argument,       NULL, 'h'},
   {0, 0, 0, 0}
  };

  const char* shortoptions = "j:n:f:s:t:kh";
  int c;
  int optionindex = 0;
  while ((c = getopt_long(argc, argv, shortoptions, longoptions, &optionindex)) != -1)
  {
    if (c == 'j')
    {
      m_jackname = optarg;
    }
    else if (c == 'n')
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
    else if (c == 'k')
    {
      m_usekodi = true;
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
  //install signal handlers for exiting
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  //create a non blocking pipe which the jack thread will use to communicate with the main thread
  if (pipe2(m_pipe, O_NONBLOCK) == -1)
  {
    printf("ERROR: Creating pipe: %s\n", strerror(errno));
    return false;
  }

  if (m_oncommand)
    printf("on command: \"%s\"\n", m_oncommand);
  if (m_offcommand)
    printf("off command: \"%s\"\n", m_offcommand);

  if (m_usekodi)
    m_kodiclient.Start(this); //start a thread that connects to Kodi's JSONRPC

  return true;
}

void CAmpSwitch::Process()
{
  //if the off command is passed on the command line, execute that first
  if (m_offcommand)
  {
    printf("switching off, executing \"%s\"\n", m_offcommand);
    system(m_offcommand);
  }

  //local switch state
  bool switchedon = false;

  while (!g_stop)
  {
    //if the jack daemon has shut down, clean up the jack client
    if (m_jackshutdown)
      JackDisconnect();

    //try to connect to jackd if not connected yet
    if (!m_connected)
      Connect();

    //wait for one second on the readable end of the pipe
    fd_set pipeset;
    FD_ZERO(&pipeset);
    FD_SET(m_pipe[0], &pipeset);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    select(m_pipe[0] + 1, &pipeset, NULL, NULL, &tv);

    //clear the readable end of the pipe
    uint8_t byte;
    read(m_pipe[0], &byte, 1);

    //if the jack thread has written a byte to the pipe, the switch state has changed
    //execute the switch on or switch off command if necessary
    if (switchedon != m_switchedon)
    {
      switchedon = m_switchedon;
      if (switchedon && m_oncommand)
      {
        printf("switching on, executing \"%s\"\n", m_oncommand);
        system(m_oncommand);
      }
      else if (!switchedon && m_offcommand)
      {
        printf("switching off, executing \"%s\"\n", m_offcommand);
        system(m_offcommand);
      }
    }
  }

  //if the off command is passed on the command line, execute that before exiting
  if (m_offcommand)
  {
    printf("switching off, executing \"%s\"\n", m_offcommand);
    system(m_offcommand);
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
         "    -j, --jack-name     name of the jack client\n"
         "    -n, --on-command    command to execute when switching on\n"
         "    -f, --off-command   command to execute when switching off\n"
         "    -s, --switch-time   minimum number of seconds between switches\n"
         "    -t, --trigger-level absolute value of trigger level\n"
         "    -k, --kodi          use Kodi's JSONRPC to switch on when playback starts\n"
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
  //try to connect to jackd
  m_client = jack_client_open(m_jackname, JackNoStartServer, NULL);
  if (m_client == NULL)
  {
    printf("ERROR: Unable to connect to jack\n");
    return false;
  }

  int returnv;

  //get the sample rate for timing calculations
  m_samplerate = jack_get_sample_rate(m_client);

  //install a callback which gets called when jackd shuts down
  jack_on_info_shutdown(m_client, SJackInfoShutdownCallback, this);

  //install the process callback, this will be called when a new audio frame is passed
  returnv = jack_set_process_callback(m_client, SJackProcessCallback, this);
  if (returnv != 0)
  {
    printf("ERROR: Unable to set process callback\n");
    return false;
  }

  //install a callback for when the sample rate changes
  returnv = jack_set_sample_rate_callback(m_client, SJackSamplerateCallback, this);
  if (returnv != 0)
    printf("ERROR: Unable to set sample rate callback\n");

  //register a jack audio port
  m_port = jack_port_register(m_client, "Input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  if (m_port == NULL)
  {
    printf("ERROR: Unable to register jack port\n");
    return false;
  }

  //activate the jack client
  returnv = jack_activate(m_client);
  if (returnv != 0)
  {
    printf("ERROR: Unable to activate jack client\n");
    return false;
  }

  printf("Connected to jack with name %s\n", m_jackname);

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

  //iterate over all samples
  float* in = jackptr;
  float* inend = in + nframes;
  while (in != inend)
  {
    //if the absolute sample value is higher than the trigger level, set the switch state to on and reset the sample counter
    bool trigger = fabsf(*(in++)) > m_triggerlevel;

    //Consider kodi playing as a trigger
    if (m_playing)
      trigger = true;

    if (trigger)
    {
      m_samplecounter = std::max((int)lround(m_switchtime * m_samplerate), 1);
      if (!m_switchedon)
      {
        m_switchedon = true;
        uint8_t msgbyte = 0;
        write(m_pipe[1], &msgbyte, 1);
      }
    }
    else if (m_samplecounter > 0)
    {
      //if the sample counter expires, set the switch state to off
      m_samplecounter--;
      if (m_samplecounter == 0)
      {
        m_switchedon = false;
        uint8_t msgbyte = 0;
        write(m_pipe[1], &msgbyte, 1);
      }
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
  //when the sample rate changes, update the sample counter so that it will represent the same amount of time
  m_samplecounter = lround((double)m_samplecounter / (double)m_samplerate * (double)nframes);
  m_samplerate = nframes;

  return 0;
}

void CAmpSwitch::SJackInfoShutdownCallback(jack_status_t code, const char *reason, void *arg)
{
  ((CAmpSwitch*)arg)->PJackInfoShutdownCallback(code, reason);
}

void CAmpSwitch::PJackInfoShutdownCallback(jack_status_t code, const char *reason)
{
  //signal the main thread that the jack server has shut down
  m_jackshutdown = true;
  uint8_t msgbyte = 0;
  write(m_pipe[1], &msgbyte, 1);
}

void CAmpSwitch::SignalHandler(int signum)
{
  //signal the main thread that the process should exit
  if (signum == SIGINT || signum == SIGTERM)
    g_stop = true;
}

void CAmpSwitch::SetPlayingState(bool playing)
{
  //Inform the switch thread about the play state of kodi
  m_playing = playing;
}
