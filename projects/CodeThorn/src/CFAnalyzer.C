/*************************************************************
 * Copyright: (C) 2012 Markus Schordan                       *
 * Author   : Markus Schordan                                *
 * License  : see file LICENSE in the CodeThorn distribution *
 *************************************************************/

#include "sage3basic.h"

#include "CFAnalyzer.h"
#include "Labeler.h"
#include "AstTerm.h"
#include <boost/foreach.hpp>

using namespace CodeThorn;

CFAnalyzer::CFAnalyzer(Labeler* l):labeler(l){
}

size_t CFAnalyzer::deleteFunctioncCallLocalEdges(Flow& flow) {
  return flow.deleteEdges(EDGE_LOCAL);
}

// MS: TODO: refactor the following two functions 
LabelSet CFAnalyzer::functionCallLabels(Flow& flow) {
  LabelSet resultSet;
  LabelSet nodeLabels;
  nodeLabels=flow.nodeLabels();
  for(LabelSet::iterator i=nodeLabels.begin();i!=nodeLabels.end();++i) {
    if(labeler->isFunctionCallLabel(*i))
      resultSet.insert(*i);
  }
  return resultSet;
}

LabelSet CFAnalyzer::conditionLabels(Flow& flow) {
  LabelSet resultSet;
  LabelSet nodeLabels;
  nodeLabels=flow.nodeLabels();
  for(LabelSet::iterator i=nodeLabels.begin();i!=nodeLabels.end();++i) {
    if(labeler->isConditionLabel(*i))
      resultSet.insert(*i);
  }
  return resultSet;
}

LabelSet CFAnalyzer::functionEntryLabels(Flow& flow) {
  LabelSet resultSet;
  LabelSet nodeLabels;
  nodeLabels=flow.nodeLabels();
  for(LabelSet::iterator i=nodeLabels.begin();i!=nodeLabels.end();++i) {
    if(labeler->isFunctionEntryLabel(*i))
      resultSet.insert(*i);
  }
  return resultSet;
}

Label CFAnalyzer::correspondingFunctionExitLabel(Label entryLabel) {
  ROSE_ASSERT(getLabeler()->isFunctionEntryLabel(entryLabel));
  SgNode* fdefnode=getNode(entryLabel);
  ROSE_ASSERT(fdefnode);
  return getLabeler()->functionExitLabel(fdefnode);
}

int CFAnalyzer::numberOfFunctionParameters(Label entryLabel) {
  ROSE_ASSERT(getLabeler()->isFunctionEntryLabel(entryLabel));
  SgNode* fdefnode=getNode(entryLabel);
  ROSE_ASSERT(fdefnode);
  SgInitializedNamePtrList& params=SgNodeHelper::getFunctionDefinitionFormalParameterList(fdefnode);
  return params.size();
}

bool CFAnalyzer::isVoidFunction(Label entryLabel) {
  ROSE_ASSERT(getLabeler()->isFunctionEntryLabel(entryLabel));
  SgNode* fdefnode=getNode(entryLabel);
  ROSE_ASSERT(fdefnode);
  return isSgTypeVoid(SgNodeHelper::getFunctionReturnType(fdefnode));
}

LabelSetSet CFAnalyzer::functionLabelSetSets(Flow& flow) {
  LabelSetSet result;
  LabelSet feLabels=functionEntryLabels(flow);
  for(LabelSet::iterator i=feLabels.begin();i!=feLabels.end();++i) {
    Label entryLabel=*i;
    LabelSet fLabels=functionLabelSet(entryLabel, flow);
    result.insert(fLabels);
  }
  return result;
}

LabelSet CFAnalyzer::functionLabelSet(Label entryLabel, Flow& flow) {
    Label exitLabel=correspondingFunctionExitLabel(entryLabel);
    LabelSet fLabels=flow.reachableNodesButNotBeyondTargetNode(entryLabel,exitLabel);
    return fLabels;
}


InterFlow CFAnalyzer::interFlow(Flow& flow) {
  // 1) for each call use AST information to find its corresponding called function
  // 2) create a set of <call,entry,exit,callreturn> edges
  InterFlow interFlow;
  LabelSet callLabs=functionCallLabels(flow);
  //cout << "calllabs: "<<callLabs.size()<<endl;
  for(LabelSet::iterator i=callLabs.begin();i!=callLabs.end();++i) {
    SgNode* callNode=getNode(*i);
    //info: callNode->get_args()
    SgFunctionCallExp *funCall=SgNodeHelper::Pattern::matchFunctionCall(callNode);
    if(!funCall) 
      throw "Error: interFlow: unkown call exp (not a SgFunctionCallExp).";
    SgFunctionDefinition* funDef=SgNodeHelper::determineFunctionDefinition(funCall);
    Label callLabel,entryLabel,exitLabel,callReturnLabel;
    if(funDef==0) {
      // we were not able to find the funDef in the AST
      cout << "STATUS: External function ";
      if(SgFunctionDeclaration* funDecl=funCall->getAssociatedFunctionDeclaration())
        cout << SgNodeHelper::getFunctionName(funDecl)<<"."<<endl;
      else
        cout << "cannot be determined."<<endl;
      callLabel=*i;
      entryLabel=Labeler::NO_LABEL;
      exitLabel=Labeler::NO_LABEL;
      callReturnLabel=labeler->functionCallReturnLabel(callNode);
    } else {
      callLabel=*i;
      entryLabel=labeler->functionEntryLabel(funDef);
      exitLabel=labeler->functionExitLabel(funDef);
      callReturnLabel=labeler->functionCallReturnLabel(callNode);
    }
    interFlow.insert(InterEdge(callLabel,entryLabel,exitLabel,callReturnLabel));
      
  }
  return interFlow;
}

Label CFAnalyzer::getLabel(SgNode* node) {
  assert(labeler);
  return labeler->getLabel(node);
}

SgNode* CFAnalyzer::getNode(Label label) {
  assert(labeler);
  return labeler->getNode(label);
}

Labeler* CFAnalyzer::getLabeler() {
  return labeler;
}

Label CFAnalyzer::initialLabel(SgNode* node) {
  assert(node);

  // special case of incExpr in SgForStatement
  if(SgNodeHelper::isForIncExpr(node))
    return labeler->getLabel(node);

  // special case of function call
  if(SgNodeHelper::Pattern::matchFunctionCall(node))
    return labeler->getLabel(node);

  if(!labeler->isLabelRelevantNode(node)) {
    cerr << "Error: not label relevant node "<<node->sage_class_name()<<endl;
  }
  assert(labeler->isLabelRelevantNode(node));
  switch (node->variantT()) {
  case V_SgFunctionDeclaration:
  case V_SgNullStatement:
  case V_SgPragmaDeclaration:
  case V_SgLabelStatement:
    return labeler->getLabel(node);
  case V_SgFunctionDefinition:
    return labeler->getLabel(node);
  case V_SgBreakStmt:
  case V_SgReturnStmt:
  case V_SgVariableDeclaration:
      return labeler->getLabel(node);
  case V_SgExprStatement: {
    return labeler->getLabel(node);
  }
  case V_SgInitializedName:
    return labeler->getLabel(node);
  case V_SgIfStmt:
  case V_SgWhileStmt:
    node=SgNodeHelper::getCond(node);
    return labeler->getLabel(node);
  case V_SgDoWhileStmt:
    node=SgNodeHelper::getLoopBody(node);
    return labeler->getLabel(node);
  case V_SgBasicBlock:
   return labeler->getLabel(node);
   // TODO: for(emptyInitList;S;S) {}
  case V_SgForStatement: {
    SgStatementPtrList& stmtPtrList=SgNodeHelper::getForInitList(node);
    if(stmtPtrList.size()==0) {
      // empty initializer list (hence, an initialization stmt cannot be initial stmt of for)
      cout << "INFO: for-stmt: initializer-list is empty."<<endl;
      cerr << "ERROR: we are bailing out. This case is not implemented yet."<<endl;
      exit(1);
    }
    assert(stmtPtrList.size()>0);
    node=*stmtPtrList.begin();
    return labeler->getLabel(node);
  }
  default:
    cerr << "Error: Unknown node in CodeThorn::CFAnalyzer::initialLabel: "<<node->sage_class_name()<<endl; exit(1);
  }
}
 
LabelSet CFAnalyzer::finalLabels(SgNode* node) {
  assert(node);
  assert(labeler->isLabelRelevantNode(node));
  LabelSet finalSet;

  // special case of incExpr in SgForStatement
  if(SgNodeHelper::isForIncExpr(node)) {
    finalSet.insert(labeler->getLabel(node));
    return finalSet;
  }

  // special case of function call
  if(SgNodeHelper::Pattern::matchFunctionCall(node)) {
    if(SgNodeHelper::Pattern::matchReturnStmtFunctionCallExp(node)) {
      finalSet.insert(labeler->functionCallReturnLabel(node)+1); // special case of function call in return-stmt
    } else {
      finalSet.insert(labeler->functionCallReturnLabel(node));
    }
    return finalSet;
  }

  switch (node->variantT()) {
  // function declarations inside basic block
  case V_SgFunctionDeclaration:
      finalSet.insert(labeler->getLabel(node));
      return finalSet;
  case V_SgFunctionDefinition: {
    SgBasicBlock* body=isSgFunctionDefinition(node)->get_body();
    return finalLabels(body);
  }
  case V_SgBreakStmt:
    return finalSet;
  case V_SgReturnStmt:
    return finalSet;
  case V_SgNullStatement:
  case V_SgPragmaDeclaration:
  case V_SgLabelStatement:
  case V_SgInitializedName:
  case V_SgVariableDeclaration:
      finalSet.insert(labeler->getLabel(node));
      return finalSet;
  case V_SgExprStatement: {
    finalSet.insert(labeler->getLabel(node));
    return finalSet;
  }

  case V_SgIfStmt: {
    SgNode* nodeTB=SgNodeHelper::getTrueBranch(node);
    LabelSet finalSetTB=finalLabels(nodeTB);
    finalSet+=finalSetTB;
    if(SgNode* nodeFB=SgNodeHelper::getFalseBranch(node)) {
      LabelSet finalSetFB=finalLabels(nodeFB);
      finalSet+=finalSetFB;
    } else {
      // in case of an empty else branch the cond node becomes the final node
      SgNode* condNode=SgNodeHelper::getCond(node);
      finalSet.insert(labeler->getLabel(condNode));
    }
    return finalSet;
  }
  case V_SgForStatement:
  case V_SgDoWhileStmt:
  case V_SgWhileStmt: {
    SgNode* condNode=SgNodeHelper::getCond(node);
    finalSet.insert(labeler->getLabel(condNode));
    set<SgNode*> breakNodes=SgNodeHelper::LoopRelevantBreakStmtNodes(node);
    LabelSet lset=labeler->getLabelSet(breakNodes);
    finalSet+=lset;
    //cout << finalSet.toString() << endl;
    return finalSet;
  }
  case V_SgBasicBlock: {
    if(SgNodeHelper::numChildren(node)>0) {
      SgNode* lastNode=SgNodeHelper::getLastOfBlock(node);
      LabelSet s=finalLabels(lastNode);
      finalSet+=s;
    } else {
      // empty basic block
      finalSet.insert(initialLabel(node));
    }
    return finalSet;
  }
  case V_SgFunctionCallExp:
    finalSet.insert(labeler->functionCallReturnLabel(node));
    return finalSet;

  default:
    cerr << "Error: Unknown node in CodeThorn::CFAnalyzer::finalLabels: "<<node->sage_class_name()<<endl; exit(1);
   }
}


Flow CFAnalyzer::flow(SgNode* s1, SgNode* s2) {
  assert(s1);
  assert(s2);
  Flow flow12;
  Flow flow1=flow(s1);
  Flow flow2=flow(s2);
  flow12+=flow1;
  flow12+=flow2;
  LabelSet finalSets1=finalLabels(s1);
  Label initLabel2=initialLabel(s2);
  for(LabelSet::iterator i=finalSets1.begin();i!=finalSets1.end();++i) {
    Edge e(*i,initLabel2);
    if(SgNodeHelper::isCond(labeler->getNode(*i))) { // special case (all TRUE edges are created explicitly)
      e.addType(EDGE_FALSE);
    }
    e.addType(EDGE_FORWARD);
    flow12.insert(e);
  }
  return flow12;
}

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
int CFAnalyzer::inlineTrivialFunctions(Flow& flow) {
  // 1) compute all functions that are called exactly once (i.e. number of pred in ICFG is 1)
  //    AND have the number of formal parameters is 0 AND have void return type.
  // 2) inline function
  // more advanced version will also clone function-CFGs, but this makes the mapping label<->code loose the 1-1 mapping property.
  int numInlined=0;
  LabelSet lnLabs=functionEntryLabels(flow);
  for(LabelSet::iterator i=lnLabs.begin();i!=lnLabs.end();++i) {
    LabelSet pred=flow.pred(*i);
    if(pred.size()==1) {
      Label lc=*pred.begin();
      ROSE_ASSERT(getLabeler()->isFunctionCallLabel(lc));
      // check the number of formal parameters of ln
      if(numberOfFunctionParameters(*i)==0 && isVoidFunction(*i)) {

        // reduce all four nodes: lc,ln,lx,lr (this also reduces a possibly existing local edge)
        Label ln=*i;
        Label lx=correspondingFunctionExitLabel(ln);
        LabelSet succ=flow.succ(lx);
        // since we have exactly one call there must be exactly one return edge
        ROSE_ASSERT(succ.size()==1);
        Label lr=*succ.begin();
        // reduce all four nodes now
        reduceNode(flow,lc);
        reduceNode(flow,ln);
        reduceNode(flow,lx);
        reduceNode(flow,lr);
        numInlined++;
      }
    }
  }
  return numInlined;
}

int CFAnalyzer::reduceEmptyConditionNodes(Flow& flow) {
  LabelSet labs=conditionLabels(flow);
  int cnt=0;
  for(LabelSet::iterator i=labs.begin();i!=labs.end();++i) {
    if(flow.succ(*i).size()==1) {
      cnt+=reduceNode(flow,*i);
    }
  }
  return cnt;
}

int CFAnalyzer::reduceNode(Flow& flow, Label lab) {
  Flow inFlow=flow.inEdges(lab);
  Flow outFlow=flow.outEdges(lab);
  /* description of essential operations:
   *   inedges: (n_i,b)
   *   outedge: (b,n2) 
   *   remove(n_i,b)
   *   remove(b,n2)
   *   insert(n1,n2)
   */
  if(inFlow.size()==0 && outFlow.size()==0)
    return 0;

  if(inFlow.size()==0 || outFlow.size()==0) {
    Flow edges=inFlow+outFlow;
    flow.deleteEdges(edges);
    return 1;
  }

  for(Flow::iterator initer=inFlow.begin();initer!=inFlow.end();++initer) {
    for(Flow::iterator outiter=outFlow.begin();outiter!=outFlow.end();++outiter) {
      Edge e1=*initer;
      Edge e2=*outiter;
      Edge newEdge=Edge(e1.source,e1.types(),e2.target);
      flow.erase(e1);
      flow.erase(e2);
      flow.insert(newEdge);
    }
  }
  return 1;
}

int CFAnalyzer::reduceBlockBeginNodes(Flow& flow) {
  LabelSet labs=flow.nodeLabels();
  int cnt=0;
  for(LabelSet::iterator i=labs.begin();i!=labs.end();++i) {
    if(isSgBasicBlock(getNode(*i))) {
#if 1
      cnt+=reduceNode(flow,*i);
#else
      cnt++;
      Flow inFlow=flow.inEdges(*i);
      Flow outFlow=flow.outEdges(*i);

      // multiple out-edges not supported yet
      assert(outFlow.size()<=1); 

      /* description of essential operations:
       *   inedges: (n_i,b)
       *   outedge: (b,n2) 
       *   remove(n_i,b)
       *   remove(b,n2)
       *   insert(n1,n2)
       */
      for(Flow::iterator initer=inFlow.begin();initer!=inFlow.end();++initer) {
        Edge e1=*initer;
        Edge e2=*outFlow.begin();
        Edge newEdge=Edge(e1.source,e1.types(),e2.target);
        flow.erase(e1);
        flow.erase(e2);
        flow.insert(newEdge);
      }
#endif
    }
  }
  return cnt;
}

void CFAnalyzer::intraInterFlow(Flow& flow, InterFlow& interFlow) {
  for(InterFlow::iterator i=interFlow.begin();i!=interFlow.end();++i) {
    if((*i).entry==Labeler::NO_LABEL && (*i).exit==Labeler::NO_LABEL) {
      Edge externalEdge=Edge((*i).call,EDGE_EXTERNAL,(*i).callReturn);      
      flow.insert(externalEdge);
    } else {
      Edge callEdge=Edge((*i).call,EDGE_CALL,(*i).entry);
      Edge callReturnEdge=Edge((*i).exit,EDGE_CALLRETURN,(*i).callReturn);
      Edge localEdge=Edge((*i).call,EDGE_LOCAL,(*i).callReturn);
      flow.insert(callEdge);
      flow.insert(callReturnEdge);
      flow.insert(localEdge);
    }
  }
}

LabelSet CFAnalyzer::setOfInitialLabelsOfStmtsInBlock(SgNode* node) {
  LabelSet ls;
  if(node==0)
    return ls;
  if(!isSgStatement(node)) {
    cerr<<"ERROR: "<<node->class_name()<<endl;
  }
  size_t len=node->get_numberOfTraversalSuccessors();
  for(size_t i=0;i<len;++i) {
    SgNode* childNode=node->get_traversalSuccessorByIndex(i);
    ls.insert(initialLabel(childNode));
  }
  return ls;
}

Flow CFAnalyzer::controlDependenceGraph(Flow& controlFlow) {
  LabelSet condLabels=conditionLabels(controlFlow);
  LabelSet targetLabels;
  Flow controlDependenceEdges;
  for(LabelSet::iterator i=condLabels.begin();i!=condLabels.end();++i) {
    SgNode* condition=getLabeler()->getNode(*i);
    cerr<<"DEBUG: cond:"<<condition->class_name()<<endl;
    SgNode* stmt=SgNodeHelper::getParent(condition);
    cerr<<"DEBUG: stmt:"<<stmt->class_name()<<endl;
    // while/dowhile/for
    if(SgNodeHelper::isLoopCond(condition)) {
      SgNode* loopBody=SgNodeHelper::getLoopBody(stmt);
      cerr<<"DEBUG: loopBody:"<<loopBody->class_name()<<endl;
      LabelSet loopBodyInitLabels=setOfInitialLabelsOfStmtsInBlock(loopBody);
      targetLabels=loopBodyInitLabels;
    }
    // if
    if(isSgIfStmt(stmt)) {
      SgNode* trueBranch=SgNodeHelper::getTrueBranch(stmt);
      LabelSet trueBranchInitLabels=setOfInitialLabelsOfStmtsInBlock(trueBranch);
      SgNode* falseBranch=SgNodeHelper::getFalseBranch(stmt);
      LabelSet falseBranchInitLabels=setOfInitialLabelsOfStmtsInBlock(falseBranch);
      targetLabels=trueBranchInitLabels+falseBranchInitLabels;
    }
    for(LabelSet::iterator j=targetLabels.begin();j!=targetLabels.end();++j) {
      controlDependenceEdges.insert(Edge(*i,EDGE_FORWARD,*j));
    }
  }
  return controlDependenceEdges;
}

Flow CFAnalyzer::WhileAndDoWhileLoopFlow(SgNode* node, 
                                         Flow edgeSet,
                                         EdgeType edgeTypeParam1,
                                         EdgeType edgeTypeParam2) {
  if(!(isSgWhileStmt(node) || isSgDoWhileStmt(node))) {
    throw "Error: WhileAndDoWhileLoopFlow: unsupported loop construct.";
  }
  SgNode* condNode=SgNodeHelper::getCond(node);
  Label condLabel=getLabel(condNode);
  SgNode* bodyNode=SgNodeHelper::getLoopBody(node);
  assert(bodyNode);
  Edge edge=Edge(condLabel,EDGE_TRUE,initialLabel(bodyNode));
  edge.addType(edgeTypeParam1);
  Flow flowB=flow(bodyNode);
  LabelSet finalSetB=finalLabels(bodyNode);
  edgeSet+=flowB;
  edgeSet.insert(edge);
  // back edges in while (forward edges in do-while)
  for(LabelSet::iterator i=finalSetB.begin();i!=finalSetB.end();++i) {
    Edge e;
    if(SgNodeHelper::isCond(labeler->getNode(*i))) {
      e=Edge(*i,EDGE_FALSE,condLabel);
      e.addType(edgeTypeParam2);
    } else {
      e=Edge(*i,edgeTypeParam2,condLabel);
    }
    edgeSet.insert(e);
  }
  return edgeSet;
}

LabelSet Flow::reachableNodes(Label start) {
  return reachableNodesButNotBeyondTargetNode(start,Labeler::NO_LABEL);
}

// MS: will possibly be replaced with an implementation from the BOOST graph library
LabelSet Flow::reachableNodesButNotBeyondTargetNode(Label start, Label target) {
  LabelSet reachableNodes;
  LabelSet toVisitSet=succ(start);
  size_t oldSize=0;
  size_t newSize=0;
  do {
    LabelSet newToVisitSet;
    for(LabelSet::iterator i=toVisitSet.begin();i!=toVisitSet.end();++i) {
      LabelSet succSet=succ(*i);
      for(LabelSet::iterator j=succSet.begin();j!=succSet.end();++j) {
        if(reachableNodes.find(*j)==reachableNodes.end())
          newToVisitSet.insert(*j);
      }
    }
    toVisitSet=newToVisitSet;
    oldSize=reachableNodes.size();
    reachableNodes+=toVisitSet;
    newSize=reachableNodes.size();
  } while(oldSize!=newSize);
  return reachableNodes;
}


Flow CFAnalyzer::flow(SgNode* node) {
  assert(node);

  Flow edgeSet;
  if(node==0)
    return edgeSet;
  if(isSgFunctionDeclaration(node)) {
    return edgeSet;
  }

  if(isSgProject(node)||isSgFileList(node)||isSgGlobal(node)||isSgSourceFile(node)) {
    RoseAst ast(node);
    Flow tmpEdgeSet;
    // search for all functions and union flow for all functions
    for(RoseAst::iterator i=ast.begin();i!=ast.end();++i) {
      if(isSgFunctionDefinition(*i)) {
        i.skipChildrenOnForward();
        cout << "STATUS: Generating flow for function "<<SgNodeHelper::getFunctionName(*i)<<endl;
        tmpEdgeSet=flow(*i);
        edgeSet+=tmpEdgeSet;
      }
    }
    return edgeSet;
  }
  
  // special case of function call pattern
  if(SgNodeHelper::Pattern::matchFunctionCall(node)) {
    // we add the 'local' edge when intraInter flow is computed (it may also be an 'external' edge)
#if 0
    Label callLabel=labeler->functionCallLabel(node);
    Label callReturnLabel=labeler->functionCallReturnLabel(node);
    edgeSet.insert(Edge(callLabel,EDGE_LOCAL,callReturnLabel));
#endif
    // add special case edge for callReturn to returnNode SgReturnStmt(SgFunctionCallExp) 
    // edge: SgFunctionCallExp.callReturn->init(SgReturnStmt)
    if(SgNodeHelper::Pattern::matchReturnStmtFunctionCallExp(node)) {
      Label callReturnLabel=labeler->functionCallReturnLabel(node);
      Label returnStmtLabel=labeler->functionCallReturnLabel(node)+1;
      edgeSet.insert(Edge(callReturnLabel,EDGE_FORWARD,returnStmtLabel));
    }
    return edgeSet;
  }

  switch (node->variantT()) {
  case V_SgFunctionDefinition: {
    SgBasicBlock* body=isSgFunctionDefinition(node)->get_body();
    Edge edge=Edge(labeler->functionEntryLabel(node),EDGE_FORWARD,initialLabel(body));
    edgeSet.insert(edge);
    Flow bodyFlow=flow(body);
    edgeSet+=bodyFlow;
    /* add explicitly edge from last stmt of function to exit-node
       not that a return at the end of a function is *not* represented in a ROSE AST.
       if return does not exist, we need to add an explicit edge from end of function to exit
       since we create this edge with the computed final node a branch with returns in all branches is
       maintained properly.
       this edge is identical if we have some other branches. Because we maintain the edges as an edge-set
       this operation is always OK.
     */
    LabelSet funFinalLabels=finalLabels(node);
    for(LabelSet::iterator i=funFinalLabels.begin();i!=funFinalLabels.end();++i) {
      Edge explicitEdge=Edge(*i,EDGE_FORWARD,labeler->functionExitLabel(node));
      if(SgNodeHelper::isLoopCond(labeler->getNode(*i))) {
        explicitEdge.addType(EDGE_FALSE);
      }
      edgeSet.insert(explicitEdge);
    }
    return edgeSet;
  }
  case V_SgReturnStmt: {
    SgNode* funcDef=SgNodeHelper::correspondingSgFunctionDefinition(node);
    if(!funcDef)
      cerr << "Error: No corresponding function for ReturnStmt found."<<endl;
    assert(isSgFunctionDefinition(funcDef));
    Edge edge=Edge(getLabel(node),EDGE_FORWARD,labeler->functionExitLabel(funcDef));
    edgeSet.insert(edge);
    return edgeSet;
  }
  case V_SgBreakStmt:
  case V_SgInitializedName:
  case V_SgVariableDeclaration:
  case V_SgNullStatement:
  case V_SgPragmaDeclaration:
  case V_SgLabelStatement:
  case V_SgExprStatement:
    return edgeSet;
  case V_SgIfStmt: {
    SgNode* nodeC=SgNodeHelper::getCond(node);
    Label condLabel=getLabel(nodeC);
    SgNode* nodeTB=SgNodeHelper::getTrueBranch(node);
    Edge edgeTB=Edge(condLabel,EDGE_TRUE,initialLabel(nodeTB));
    edgeTB.addType(EDGE_FORWARD);
    edgeSet.insert(edgeTB);
    Flow flowTB=flow(nodeTB);
    edgeSet+=flowTB;
    if(SgNode* nodeFB=SgNodeHelper::getFalseBranch(node)) {
      Flow flowFB=flow(nodeFB);
      Edge edgeFB=Edge(condLabel,EDGE_FALSE,initialLabel(nodeFB));
      edgeFB.addType(EDGE_FORWARD);
      edgeSet.insert(edgeFB);
      edgeSet+=flowFB;
    }
    return edgeSet;
  }
  case V_SgWhileStmt: 
    return WhileAndDoWhileLoopFlow(node,edgeSet,EDGE_FORWARD,EDGE_BACKWARD);
  case V_SgDoWhileStmt: 
    return WhileAndDoWhileLoopFlow(node,edgeSet,EDGE_BACKWARD,EDGE_FORWARD);
  case V_SgBasicBlock: {
    size_t len=node->get_numberOfTraversalSuccessors();
    if(len==0) {
      return edgeSet;
    } else {
      if(len==1) {
        SgNode* onlyStmt=node->get_traversalSuccessorByIndex(0);
        Flow onlyFlow=flow(onlyStmt);
        edgeSet+=onlyFlow;
      } else {
        for(size_t i=0;i<len-1;++i) {
          SgNode* childNode1=node->get_traversalSuccessorByIndex(i);
          SgNode* childNode2=node->get_traversalSuccessorByIndex(i+1);
          Flow flow12=flow(childNode1,childNode2);
          edgeSet+=flow12;
        }
      }
    }
    SgNode* firstStmt=node->get_traversalSuccessorByIndex(0);
    Edge edge=Edge(getLabel(node),EDGE_FORWARD,initialLabel(firstStmt));
    edgeSet.insert(edge);
    return edgeSet;
  }

  case V_SgForStatement: {
    SgStatementPtrList& stmtPtrList=SgNodeHelper::getForInitList(node);
    int len=stmtPtrList.size();
    if(len==0) {
      // empty initializer list (hence, an initialization stmt cannot be initial stmt of for)
      cout << "INFO: for-stmt: initializer-list is empty."<<endl;
      cerr << "Error: empty for-stmt initializer not supported yet."<<endl;
      exit(1);
    }
    assert(len>0);
    SgNode* lastNode=0;
    if(len==1) {
      SgNode* onlyStmt=*stmtPtrList.begin();
      Flow onlyFlow=flow(onlyStmt);
      edgeSet+=onlyFlow;
      lastNode=onlyStmt;
    } else {
      assert(stmtPtrList.size()>=2);
      for(SgStatementPtrList::iterator i=stmtPtrList.begin();
          i!=stmtPtrList.end();
          ++i) {
        SgNode* node1=*i;
        SgStatementPtrList::iterator i2=i;
        i2++;
        SgNode* node2=*i2;
        Flow flow12=flow(node1,node2);
        edgeSet+=flow12;
        i2++;
        if(i2==stmtPtrList.end()) {
          lastNode=node2;
          break;
        }        
      }
    }
    SgNode* condNode=SgNodeHelper::getCond(node);
    if(!condNode)
      throw "Error: for-loop: empty condition not supported yet.";
    Flow flowInitToCond=flow(lastNode,condNode);
    edgeSet+=flowInitToCond;
    Label condLabel=getLabel(condNode);
    SgNode* bodyNode=SgNodeHelper::getLoopBody(node);
    assert(bodyNode);
    Edge edge=Edge(condLabel,EDGE_TRUE,initialLabel(bodyNode));
    edge.addType(EDGE_FORWARD);
    Flow flowB=flow(bodyNode);
    LabelSet finalSetB=finalLabels(bodyNode);
    edgeSet+=flowB;
    edgeSet.insert(edge);
    // back edges
    for(LabelSet::iterator i=finalSetB.begin();i!=finalSetB.end();++i) {
      SgExpression* incExp=SgNodeHelper::getForIncExpr(node);
      if(!incExp)
        throw "Error: for-loop: empty incExpr not supported yet.";
      assert(incExp);
      Label incExpLabel=getLabel(incExp);
      assert(incExpLabel!=Labeler::NO_LABEL);
      Edge e1,e2;
      if(SgNodeHelper::isCond(labeler->getNode(*i))) {
        e1=Edge(*i,EDGE_FALSE,incExpLabel);
        e1.addType(EDGE_FORWARD);
        e2=Edge(incExpLabel,EDGE_BACKWARD,condLabel);
      } else {
        e1=Edge(*i,EDGE_FORWARD,incExpLabel);
        e2=Edge(incExpLabel,EDGE_BACKWARD,condLabel);
      }
      edgeSet.insert(e1);
      edgeSet.insert(e2);
    }
    return edgeSet;
  }
  default:
    cerr << "Error: Unknown node in CFAnalyzer::flow: "<<node->sage_class_name()<<endl; 
    cerr << "Problemnode: "<<node->unparseToString()<<endl;
    exit(1);
  }
}
