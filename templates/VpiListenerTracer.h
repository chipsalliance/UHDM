// -*- c++ -*-

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
 * File:   VpiListenerTracer.h
 * Author: hs
 *
 * Created on October 11, 2020, 9:00 PM
 */

#ifndef UHDM_VPILISTENERTRACER_H
#define UHDM_VPILISTENERTRACER_H

#include "VpiListener.h"

#include <ostream>
#include <string>

#define TRACE_CONTEXT                 \
  "[" << object->VpiLineNo() <<       \
  "," << object->VpiColumnNo() <<     \
  ":" << object->VpiEndLineNo() <<    \
  "," << object->VpiEndColumnNo() <<  \
  "]"

#define TRACE_ENTER strm                \
  << std::string(++indent * 2, ' ')     \
  << __func__ << ": " << TRACE_CONTEXT  \
  << std::endl
#define TRACE_LEAVE strm                \
  << std::string(2 * indent--, ' ')     \
  << __func__ << ": " << TRACE_CONTEXT  \
  << std::endl

namespace UHDM {

  class VpiListenerTracer : public VpiListener {
  public:
    VpiListenerTracer(std::ostream &strm) : strm(strm) {}

    virtual ~VpiListenerTracer() = default;

<VPI_LISTENER_TRACER_METHODS>

  protected:
   std::ostream &strm;
   int indent = -1;
  };
};

#endif
