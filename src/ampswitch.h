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

#ifndef AMPSWITCH_H
#define AMPSWITCH_H

#include <jack/jack.h>

class CAmpSwitch
{
  public:
    CAmpSwitch(int argc, char *argv[]);
    ~CAmpSwitch();

    bool Setup();
    void Process();
    void Cleanup();

  private:
    void        Connect();
    bool        JackConnect();
    void        JackDisconnect();

    static int  SJackProcessCallback(jack_nframes_t nframes, void *arg);
    int         PJackProcessCallback(jack_nframes_t nframes);

    static int  SJackSamplerateCallback(jack_nframes_t nframes, void *arg);
    int         PJackSamplerateCallback(jack_nframes_t nframes);

    static void SJackInfoShutdownCallback(jack_status_t code, const char *reason, void *arg);
    void        PJackInfoShutdownCallback(jack_status_t code, const char *reason);

    static void SignalHandler(int signum);

    bool           m_connected;
    jack_client_t* m_client;
    jack_port_t*   m_port;
    int            m_pipe[2];
    bool           m_switchedon;
    float          m_triggerlevel;
    int            m_samplerate;
    float          m_switchtime;
    int            m_samplecounter;
    bool           m_jackshutdown;
};

#endif //AMPSWITCH_H
