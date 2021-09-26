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

#ifndef KODICLIENT_H
#define KODICLIENT_H

#include <thread>
#include <string>

class CAmpSwitch;

class CKodiClient
{
  public:
    void        Start(CAmpSwitch* ampswitch);

  private:
    static void SProcess(CKodiClient* kodiclient);
    void        Process();
    void        Parse(const std::string& jsonstr);

    std::thread m_thread;
    CAmpSwitch* m_ampswitch;
};

#endif //KODICLIENT_H
