// -*- c++ -*-

/*

 Copyright 2019-2020 Alain Dargelas

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
 * File:   uhdm_forward_decl.h
 * Author:
 *
 * Created on May 06, 2020, 10:03 PM
 */

#ifndef UHDM_FORWARD_DECL_H
#define UHDM_FORWARD_DECL_H

#include <vector>

#include <uhdm/BaseClass.h>

namespace UHDM {
class BaseClass;
typedef BaseClass any;

<UHDM_CLASSES_FORWARD_DECL>

<UHDM_FACTORIES_FORWARD_DECL>

typedef FactoryT<std::vector<BaseClass*>> VectorOfanyFactory;
<UHDM_CONTAINER_FACTORIES_FORWARD_DECL>
};


#endif
