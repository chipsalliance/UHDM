/*

 Copyright 2019-2021 Alain Dargelas

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
#ifndef UHDM_VPI_USER_H
#define UHDM_VPI_USER_H

// Missing defines from vpi_user.h, sv_vpi_user.h
#define vpiDesign            3000
#define vpiInterfaceTypespec 3001
#define vpiNets              3002
#define vpiSimpleExpr        3003
#define vpiParameters        3004
#define vpiSequenceExpr      3005
#define vpiSoftDisable       3006
#define vpiIsModPort         3007

// These define where orinally aliased in sv_vpi_user.h
// Aliasing makes it hard to distinguish in automatically generated code, assigning unique values.
#define vpiVarBit            3008
#define vpiLogicVar          3009
#define vpiArrayVar          3010

#define vpiWaits             3011
#define vpiDisables          3012
#define vpiStructMember      3013

// Used to mark imported parameters
#define  vpiImported         3014

// Extra location information 
#define  vpiColumnNo         3015
#define  vpiEndLineNo        3016
#define  vpiEndColumnNo      3017

// Tags used to model unsupported nodes
#define vpiUnsupportedStmt   4000
#define vpiUnsupportedExpr   4001
#define vpiUnsupportedTypespec 4002

// Objects/properties not in the Standard
#define vpiHierPath         5000 // Represents a hierarchical path
#define vpiReordered        5001 // Boolean for operations (pattern assign, concat) that has been reordered
#define vpiElaborated       5002 // Boolean indicating UHDM has been elaborated/uniquified
#define vpiRefVar           5003 // "variables" type reference object required for late binding during elaboration
#define vpiOverriden        5004 // Boolean indicating a param_assign is overriden (not default value)

#endif  // UHDM_VPI_USER_H
