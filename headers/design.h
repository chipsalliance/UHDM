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
 * File:   design.h
 * Author: 
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef DESIGN_H
#define DESIGN_H

namespace UHDM {

  class design : public base_class {
  public:
    design(){}
    virtual ~design(){}
    
    const VectorOfmodulePtr get_allModules() { return m_allModules; }

    void set_allModules(VectorOfmodulePtr data) { m_allModules = data; }

    const VectorOfmodulePtr get_topModules() { return m_topModules; }

    void set_topModules(VectorOfmodulePtr data) { m_topModules = data; }

  private:
    
    VectorOfmodulePtr m_allModules;

    VectorOfmodulePtr m_topModules;

  };

};

#endif

