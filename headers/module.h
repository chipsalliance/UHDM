/*
 Copyright 2019 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   module.h
 * Author: alain
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef MODULE_H
#define MODULE_H

namespace UHDM {

  class module {
  public:
    module(){};
    virtual ~module();
    
    bool vpiTopModule() { return m_vpiTopModule; }

    void set_vpiTopModule(bool data) { m_vpiTopModule = data; }

    int vpiDefDecayTime() { return m_vpiDefDecayTime; }

    void set_vpiDefDecayTime(int data) { m_vpiDefDecayTime = data; }

  private:
    
    bool m_vpiTopModule;

    int m_vpiDefDecayTime;

  };

};

#endif

