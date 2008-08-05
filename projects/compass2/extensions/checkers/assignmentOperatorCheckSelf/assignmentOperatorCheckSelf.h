// Assignment Operator Check Self
// Author: pants,,,
// Date: 24-July-2007

#include "compass.h"

#ifndef COMPASS_ASSIGNMENT_OPERATOR_CHECK_SELF_H
#define COMPASS_ASSIGNMENT_OPERATOR_CHECK_SELF_H

namespace CompassAnalyses
   { 
     namespace AssignmentOperatorCheckSelf
        { 
        /*! \brief Assignment Operator Check Self: Add your description here 
         */

          extern const std::string checkerName;
          extern const std::string shortDescription;
          extern const std::string longDescription;

       // Specification of Checker Output Implementation
          class CheckerOutput: public Compass::OutputViolationBase
             { 
               public:
                    CheckerOutput(SgNode* node);
             };

       // Specification of Checker Traversal Implementation

          class Traversal
             : public AstSimpleProcessing, public Compass::TraversalBase
             {
            // Checker specific parameters should be allocated here.

               public:
                    Traversal(Compass::Parameters inputParameters, Compass::OutputObject* output);

                 // The implementation of the run function has to match the traversal being called.
                    void run(SgNode* n){ this->traverse(n, preorder); };

                    void visit(SgNode* n);
             };
        }
   }

// COMPASS_ASSIGNMENT_OPERATOR_CHECK_SELF_H
#endif 

