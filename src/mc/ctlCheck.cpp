


#include "ctlCheck.hh"
#include "visfd.h"

using namespace its;


its::Transition  CTLChecker::getHomomorphism (Ctlp_Formula_t *ctlFormula) const {
  its::Transition result;
  ctl_cache_it it = ctl_cache.find(ctlFormula);
  if ( it == ctl_cache.end() ) {
    // A miss 
    // invoke recursive procedures
    its::Transition leftHom, rightHom;
    // Handle left child
    {
      Ctlp_Formula_t *leftChild = Ctlp_FormulaReadLeftChild(ctlFormula);
      if (leftChild) {
	leftHom = getHomomorphism (leftChild);
      }
    }
    // Handle right child
    {
      Ctlp_Formula_t *rightChild = Ctlp_FormulaReadRightChild(ctlFormula);
      if (rightChild) {
	rightHom = getHomomorphism (rightChild);
      }      
    }
    //    std::cerr << "Translating CTL formula : " ;
    //    Ctlp_FormulaPrint(vis_stderr, ctlFormula);
    //    std::cerr << std::endl;

    // Now switch on the type of formula
    switch (Ctlp_FormulaReadType(ctlFormula)) {
    case Ctlp_ID_c:
      // basic case
      result =  getSelectorAP( Ctlp_FormulaReadVariableName(ctlFormula),Ctlp_FormulaReadValueName(ctlFormula) );
      break;
    case Ctlp_TRUE_c:
      result = Transition::id;
      break;

    case Ctlp_FALSE_c:
      result = Transition::null; 
      break;

    case Ctlp_NOT_c:
      result = Transition(getReachable() - leftHom (getReachable())) ; 
      break;
      
    case Ctlp_AND_c:
      result = Transition( ( leftHom (getReachable())) *  (rightHom(getReachable())) );
      break;

    case Ctlp_EQ_c:
      assert(false);
//      result = mdd_xnor(leftMdd, rightMdd); 
      break;

    case Ctlp_XOR_c:
      assert(false);
//      result = mdd_xor(leftMdd, rightMdd); break;
      break;

    case Ctlp_THEN_c:
      // A => B    <->  not A or B
      result = (! leftHom ) + rightHom ;
      break;

    case Ctlp_OR_c:
      result = Transition( ( leftHom (getReachable())) +  (rightHom(getReachable())) );
      //      result = leftHom + rightHom;
      break;

    case Ctlp_EX_c:
      
      result = getPredRel() & leftHom;
      break;

    case Ctlp_EU_c: 
      // start from states satisfying g, then add predescessors verifying f in a fixpoint.
      // f U g <-> ( f & pred + Id )^* & g
      result = fixpoint ( (leftHom & getPredRel()) + its::Transition::id ) & rightHom ;
      break;
    
    case Ctlp_EG_c: 
      // start with states satisfying f
      // then remove states that are not a predescessor of a state in the set
      // EG f <->  ( f & pred )^* & f
      result = fixpoint ( leftHom & getPredRel() * its::Transition::id ) & leftHom ;
      break;
    
    case Ctlp_Cmp_c: {
      // Forward CTL specific operator, curretly unimplemented
      assert(false);
//       if (Ctlp_FormulaReadCompareValue(ctlFormula) == 0)
// 	result = bdd_is_tautology(leftMdd, 0) ?
// 	  mdd_one(mddMgr) : mdd_zero(mddMgr);
//       else
// 	result = bdd_is_tautology(leftMdd, 0) ?
// 	  mdd_zero(mddMgr) : mdd_one(mddMgr);
      break;
    }
    case Ctlp_Init_c:
      // cast to constant homomorphism
      result = GShom(getInitialState()) ;
      break;
    case Ctlp_FwdU_c:
      /** From Vis source documentation :
       *							    t
       ** E[p U q]      = lfp Z[q V (p ^ EX(Z))]   :   p p ... p q
       ** FwdUntil(p,q) = lfp Z[p V EY(Z ^ q)]     :		    pq q q ... q
       */
      /**
       * In other words, start from states satisfying p, then add successors satisfying q to fixpoint
       * FwdUntil(p,q) =  ( q & Next  + Id)^* & p
       */
      // test for trivial reachability case
      if (Ctlp_FormulaReadLeftChild(ctlFormula) &&
	  Ctlp_FormulaReadType(Ctlp_FormulaReadLeftChild(ctlFormula)) ==
	  Ctlp_Init_c &&
	  Ctlp_FormulaReadRightChild(ctlFormula) &&
	  Ctlp_FormulaReadType(Ctlp_FormulaReadRightChild(ctlFormula)) ==
	  Ctlp_TRUE_c ) {
	// cast to constant hom
	result = GShom(getReachable()) ;
	break;
      }
      // the real case
      result = fixpoint ( (rightHom & getNextRel()) + its::Transition::id ) & leftHom ;
      break;
    case Ctlp_FwdG_c:
      // states reachable by an infinite path of f
      result = fixpoint ( leftHom & getNextRel() ) ; 
      break;
    case Ctlp_EY_c:
      // exists yesterday : states that have a predescessor that verifies f
      // take states verifying f, then compute successors
      // EY f  <->  Next & f (reach)
      result = getNextRel() & leftHom ;
      break;
      
    default: 
      fail("Encountered unknown type of CTL formula\n");
    }

    // std::cerr << "Obtained Homomorphism : " << result << std::endl;
    
  } else {
    result = it->second;
  }  
  return result;
}


// Explain the truth value of formula in given states.
// Formula is true if at least some input states satify it.
// Writes to out a diagnosis.
// Returns states that were actually explained, a subset of the input states.
its::State CTLChecker::explain (its::State sat, Ctlp_Formula_t *ctlFormula, std::ostream & out) const  {
 
  out << "Subformula :";
  Ctlp_FormulaPrint(vis_stdout, ctlFormula);
  // test if formula is true in given states. i.e. some at least satisfy p.
  its::State trueF = getStateVerifying(ctlFormula);
  // Testing with subsets would be stronger, requiring all states satisfy p.
  its::State satF = sat * trueF ; 
  bool formIsTrue = ( sat != State::null ) ;
  if (formIsTrue) {
    out << "is true in "<< sat.nbStates() <<  " state(s) out of "<< sat << " input state(s).";
  } else {
    out << "is false in all " << sat.nbStates() << " input state(s)."  ;
  }
  // Handle left child
  Ctlp_Formula_t *leftChild = Ctlp_FormulaReadLeftChild(ctlFormula);
  // Handle right child
  Ctlp_Formula_t *rightChild = Ctlp_FormulaReadRightChild(ctlFormula);

  
  // Now switch on the type of formula
  switch (Ctlp_FormulaReadType(ctlFormula)) {
  case Ctlp_ID_c:
      {
	if (formIsTrue) {
	  out << "Because atomic predicate " << Ctlp_FormulaReadVariableName(ctlFormula) << Ctlp_FormulaReadValueName(ctlFormula) << " is true in " << satF.nbStates() << " states out of "  << sat.nbStates()  << " input states. \n" <<  std::endl ;
	  out << "For instance predicate is true in this state :\n";
	  model.printSomeStates(satF, out);
	  return satF;
	} else {
	  out << "Because atomic predicate " << Ctlp_FormulaReadVariableName(ctlFormula) << Ctlp_FormulaReadValueName(ctlFormula) << " is false in all " << sat.nbStates()  << " input states. \n" <<  std::endl ;
	  out << "For instance predicate is false in this state :\n";
	  model.printSomeStates(sat, out);
	  return sat;
	}
	break;
      }
  case Ctlp_TRUE_c:
    out << "Because TRUE is true in any state, such as " << std::endl;
    model.printSomeStates(sat, out);
    return sat;
    break;
    
  case Ctlp_FALSE_c:
    out << "Because FALSE is false in any state, such as " << std::endl;
    model.printSomeStates(sat, out);
    return sat;
    break;
    
  case Ctlp_NOT_c:
    if (formIsTrue) {
      out << "Because for some input states all future behaviors satisfy NOT(" ;
      Ctlp_FormulaPrint(vis_stdout, leftChild);
      out << ")" <<  std::endl;
      return explain(satF, leftChild, out);
    } else {
      out << "Because for some input states, there exist some future behaviors that satisfy " ;
      Ctlp_FormulaPrint(vis_stdout, leftChild);
      out <<  std::endl;
      return explain(sat, leftChild, out);
    }
    break;
    
  case Ctlp_AND_c:
    if (formIsTrue) {
      out << "Because both " ;
      Ctlp_FormulaPrint(vis_stdout,leftChild);
      out  << " and " ;
      Ctlp_FormulaPrint(vis_stdout,rightChild);
      out << " are satisfied in some input states. For instance :\n" ;
      its::State lexpl = explain(satF, leftChild, out); 
      out << "And in the same states  \n";
      its::State rexpl = explain(lexpl, rightChild, out); 
      return rexpl;
    } else {
      
      out << "Because no input states satisfy both " ;
      Ctlp_FormulaPrint(vis_stdout,leftChild) ;
      out << " and " ;
      Ctlp_FormulaPrint(vis_stdout,rightChild);
      out<< " simultaneously." ;
      if (leftChild && rightChild) {
	its::State leftStates = getStateVerifying (leftChild) * sat ;
	its::State rightStates = getStateVerifying (rightChild) * sat ;
	
	// cut into cases, since a & b is false in s 
	// either : s*a = 0  or s*b = 0 (explain why) and its no use talking further
	// or both are non-empty but a&b false, so that (s*a) * (s*b) is empty.
	// explain both a and b and conclude in empty intersection.
	if (leftStates == its::State::null && rightStates == its::State::null) {
	  out << "Even formula " ;
	  Ctlp_FormulaPrint(vis_stdout,leftChild);
	  out << " OR " ;
	  Ctlp_FormulaPrint(vis_stdout,rightChild) ;
	  out << " is false in all input states." ;
	  out << " for instance " ;
	  Ctlp_FormulaPrint(vis_stdout,leftChild);
	  out << " is not satisfied in input states.";
	  return explain(sat,leftChild,out);
	} else if (leftStates == its::State::null) {
	  out << "First conjunct  " ;
	  Ctlp_FormulaPrint(vis_stdout,leftChild) ;
	  out << " is not satisfied in any input states. So AND conjunction is false .";
	  return explain(sat,leftChild,out);
	} else if (rightStates == its::State::null) {
	  out << "Second conjunct  " ;
	  Ctlp_FormulaPrint(vis_stdout,rightChild) ;
	  out << " is not satisfied in any input states. So AND conjunction is false .";
	  return explain(sat,rightChild,out);
	} else {
	  out << "Some " << leftStates.nbStates() << " of your " << sat.nbStates() << " input states do satisfy " ;
	  Ctlp_FormulaPrint(vis_stdout,leftChild);
	  out << "AND some " << rightStates.nbStates() << " of your " << sat.nbStates() << " input states do satisfy " ;
	  Ctlp_FormulaPrint(vis_stdout,leftChild);
	  out << "But these sets have an empty intersection.";
	  explain(sat, leftChild, out);
	  explain(sat, rightChild, out);
	  return sat;
	}
      } else {
	std::cerr << "ERROR : Conjunction has no children" << std::endl; 
	assert(false);
      }	
    }
    break;
    
  case Ctlp_EQ_c:
    assert(false);
    //      result = mdd_xnor(leftMdd, rightMdd); 
    break;
    
  case Ctlp_XOR_c:
    assert(false);
    //      result = mdd_xor(leftMdd, rightMdd); break;
    break;

  case Ctlp_THEN_c:
    // A => B    <->  not A or B
    if (formIsTrue) {
      if (leftChild && rightChild) {
	its::State leftStates = getStateVerifying (leftChild) * sat ;
	its::State rightStates = getStateVerifying (rightChild) * sat ;
	if (leftStates != its::State::null) {
	  out << "Because NOT " ;
	  Ctlp_FormulaPrint(vis_stdout,leftChild);
	  out << " is true in " << leftStates.nbStates() << " states of your " << sat.nbStates() << " input states." << std::endl;
	  return explain(leftStates, leftChild, out);
	} else {
	  out << "Because " ;
	  Ctlp_FormulaPrint(vis_stdout,rightChild);
	  out << " is true in " << rightStates.nbStates() << " states of your " << sat.nbStates() << " input states." << std::endl;
	  return explain(rightStates, leftChild, out);
	}
      } else {
	std::cerr << "ERROR : Implication has no children" << std::endl; 
	assert(false);
      }
    } else {
      out << "Because A=>B <==> !A OR B and  both NOT(" ;
      Ctlp_FormulaPrint(vis_stdout,leftChild);
      out  << ") and " ;
      Ctlp_FormulaPrint(vis_stdout,rightChild);
      out << " are unsatisfied in all input states. For instance A is true :\n" ;
      its::State lexpl = explain(sat, leftChild, out); 
      out << "And also (!B) is true \n";
      its::State rexpl = explain(sat, rightChild, out); 
      return sat;
    } 
    break;

  case Ctlp_OR_c:
    if (formIsTrue) {
      if (leftChild && rightChild) {
	its::State leftStates = getStateVerifying (leftChild) * sat ;
	its::State rightStates = getStateVerifying (rightChild) * sat ;
	if (leftStates != its::State::null) {
	  out << "Because " ;
	  Ctlp_FormulaPrint(vis_stdout,leftChild);
	  out << " is true in " << leftStates.nbStates() << " states of your " << sat.nbStates() << " input states." << std::endl;
	  return explain(leftStates, leftChild, out);
	} else {
	  out << "Because " ;
	  Ctlp_FormulaPrint(vis_stdout,rightChild);
	  out << " is true in " << rightStates.nbStates() << " states of your " << sat.nbStates() << " input states." << std::endl;
	  return explain(rightStates, leftChild, out);
	}
      } else {
	std::cerr << "ERROR : Implication has no children" << std::endl; 
	assert(false);
      }
    } else {
      out << "Because both A and B are unsatisfied in all input states. Both NOT(" ;
      Ctlp_FormulaPrint(vis_stdout,leftChild);
      out  << ") and NOT(" ;
      Ctlp_FormulaPrint(vis_stdout,rightChild);
      out << ") are true in all input states. For instance A is false :\n" ;
      its::State lexpl = explain(sat, leftChild, out); 
      out << "And also B is false \n";
      its::State rexpl = explain(sat, rightChild, out); 
      return sat;
    } 
    break;

  case Ctlp_EX_c:
    if (formIsTrue) {
      if (leftChild) {
	its::State leftStates = getStateVerifying (leftChild) ;
	its::path_t path = model.findPath(satF,leftStates, getReachable(), true);
	// EX => should be exactly one transition
	assert(path.getPath().size() == 1);
	out << "EX p is true Because there are immediate successors of input states that satisfy p.\n";
	out << "Firing transition :" << * path.getPath().begin() << " from input states " ;
	model.printSomeStates(path.getInit(),out);
	out << "Leads to states " ;
	model.printSomeStates(path.getFinal(),out);
	out << "That satisfy p."<< std::endl ;
	explain (path.getFinal(),leftChild,out);
	return path.getInit();
      } else {
	std::cerr << "ERROR : EX has no children" << std::endl; 
	assert(false);
      }
    } else {
      out << "EX p is false because all successors of current states satisfy !p.\n" ;
      out << "For instance some input states such as \n" ;
      model.printSomeStates(sat,out);
      out << " have successors such as  \n" ;
      explain ( getNextRel() (sat), leftChild, out);
      return sat;
    } 
    break;
  case Ctlp_EU_c: 
    {
      // start from states satisfying g, then add predescessors verifying f in a fixpoint.
      // f U g <-> ( f & pred + Id )^* & g
      //      result = fixpoint ( (leftStates * getPredRel()) + its::Transition::id )  (rightStates) ;
      break;
    }
  case Ctlp_EG_c: 
    // start with states satisfying f
    // then remove states that are not a predescessor of a state in the set
    // EG f <->  ( f & pred )^* & f      
//       result = fixpoint (  
// 			   ( getPredRel()   
// 			     + ( getReachable() -  (getPredRel() (getReachable())) ) // i.e. add dead states that verify f
// 			     ) * its::Transition::id ) (leftStates) ; 
    break;
			   
  case Ctlp_Cmp_c: {
      // Forward CTL specific : compare a formula to false or true
      // i.e. check whether a set is empty or not. return State::one to indicate truth, and State::null to indicate false.
//       if (Ctlp_FormulaReadCompareValue(ctlFormula) == 0)
//  	result = (leftStates == State::null ? State::one : State::null);
//       else
// 	result = (leftStates == State::null ? State::null : State::one);
    break;
  }
  case Ctlp_Init_c:
      // cast to constant homomorphism
//       result = getInitialState() ;
    break;
  case Ctlp_FwdU_c:
      /** From Vis source documentation :
       *							    t
       ** E[p U q]      = lfp Z[q V (p ^ EX(Z))]   :   p p ... p q
       ** FwdUntil(p,q) = lfp Z[p V EY(Z ^ q)]     :		    pq q q ... q
       */
      /**
       * In other words, start from states satisfying p, then add successors satisfying q to fixpoint
       * FwdUntil(p,q) =  ( q & Next  + Id)^* & p
       */
      // test for trivial reachability case
//       if (Ctlp_FormulaReadLeftChild(ctlFormula) &&
// 	  Ctlp_FormulaReadType(Ctlp_FormulaReadLeftChild(ctlFormula)) ==
// 	  Ctlp_Init_c &&
// 	  Ctlp_FormulaReadRightChild(ctlFormula) &&
// 	  Ctlp_FormulaReadType(Ctlp_FormulaReadRightChild(ctlFormula)) ==
// 	  Ctlp_TRUE_c ) {
// 	// cast to constant hom
// 	result = getReachable() ;
//     break;
//   }
      // the real case
      // FwdUntil(p,q) holds at any state "t", such that there exists a path through "t" from some state at which
      // p holds, and q holds at all states before "t" on the path.
      
//       result = (getNextRel() +Transition::id) (fixpoint ( (rightStates * getNextRel()) + its::Transition::id ) ( leftStates )) ;
    break;
  case Ctlp_FwdG_c:
   {
	// EH (p) is the subset of states verifying "p" that are reachable through a cycle in p
	// EH = fixpoint ( p * getNextRel() ) (getReachable);

	// Reachable (p,q) : states that verify "q" reachable from states verifying "p and q" 
	// (while constantly verifying "q")
	// Reach (p,q) = fixpoint (  (q * next) + id )  (p * q)

	// FwdGlobal(p,q) = EH ( Reachable (p,q) )

	// states reachable by an infinite path of f
// 	result = fixpoint (  getNextRel() 
// 			     * ( fixpoint ( (rightStates * getNextRel()) + Transition::id  ) ( leftStates * rightStates)  )
// 			     ) ( getReachable() );



	// FwdGlobal(p,q) = EH ( Reachable (p,q) )
	// Start from states p, S = p
	// Keep only those satisfying q. S = S * q
	// Add any states satisfying q, q reachable from S. S = fix( Id +  q*Next ) (S)
	// Reduce to states in cycles + suffix thereof. S = fix ( Next * Id ) (S)
     break;
   }
 case Ctlp_EY_c:
      // exists yesterday : states that have a predescessor that verifies f
      // take states verifying f, then compute successors
      // EY f  <->  Next & f (reach)
//       result = getNextRel() ( leftStates ) ;
   break;
   
  default: 
      out << "Encountered unknown type of CTL formula\n";
  }
  return sat;
}

its::State  CTLChecker::getStateVerifying (Ctlp_Formula_t *ctlFormula) const {
  its::State result;
  ctl_statecache_it it = ctl_statecache.find(ctlFormula);
  if ( it == ctl_statecache.end() ) {
    // A miss 
    // invoke recursive procedures
    its::State leftStates, rightStates;
    // Handle left child
    {
      Ctlp_Formula_t *leftChild = Ctlp_FormulaReadLeftChild(ctlFormula);
      if (leftChild) {
	leftStates = getStateVerifying (leftChild);
      }
    }
    // Handle right child
    {
      Ctlp_Formula_t *rightChild = Ctlp_FormulaReadRightChild(ctlFormula);
      if (rightChild) {
	rightStates = getStateVerifying (rightChild);
      }      
    }
    //    std::cerr << "Translating CTL formula : " ;
    //    Ctlp_FormulaPrint(vis_stderr, ctlFormula);
    //    std::cerr << std::endl;

    // Now switch on the type of formula
    switch (Ctlp_FormulaReadType(ctlFormula)) {
    case Ctlp_ID_c:
      {
	// basic case
	Transition sel =  getSelectorAP( Ctlp_FormulaReadVariableName(ctlFormula) , Ctlp_FormulaReadValueName(ctlFormula) );
	result = sel (getReachable());
	break;
      }
    case Ctlp_TRUE_c:
      result = getReachable();
      break;

    case Ctlp_FALSE_c:
      result = State::null; 
      break;

    case Ctlp_NOT_c:
      result = getReachable() - leftStates ; 
      break;
      
    case Ctlp_AND_c:
      result = leftStates *  rightStates;
      break;

    case Ctlp_EQ_c:
      assert(false);
//      result = mdd_xnor(leftMdd, rightMdd); 
      break;

    case Ctlp_XOR_c:
      assert(false);
//      result = mdd_xor(leftMdd, rightMdd); break;
      break;

    case Ctlp_THEN_c:
      // A => B    <->  not A or B
      result = (getReachable() - leftStates) + rightStates ;
      break;

    case Ctlp_OR_c:
      result = leftStates  +  rightStates ;

      break;

    case Ctlp_EX_c:
      
      result = getPredRel() (leftStates);
      break;

    case Ctlp_EU_c: 
      {
      // start from states satisfying g, then add predescessors verifying f in a fixpoint.
      // f U g <-> ( f & pred + Id )^* & g
      result = fixpoint ( (leftStates * getPredRel()) + its::Transition::id )  (rightStates) ;
      break;
      }
    case Ctlp_EG_c: 
      // start with states satisfying f
      // then remove states that are not a predescessor of a state in the set
      // EG f <->  ( f & pred )^* & f      
      result = fixpoint (  
			   ( getPredRel()   
			     + ( getReachable() -  (getPredRel() (getReachable())) ) // i.e. add dead states that verify f
			     ) * its::Transition::id ) (leftStates) ; 
      break;
			   
    case Ctlp_Cmp_c: {
      // Forward CTL specific : compare a formula to false or true
      // i.e. check whether a set is empty or not. return State::one to indicate truth, and State::null to indicate false.
      if (Ctlp_FormulaReadCompareValue(ctlFormula) == 0)
 	result = (leftStates == State::null ? State::one : State::null);
      else
	result = (leftStates == State::null ? State::null : State::one);
      break;
    }
    case Ctlp_Init_c:
      // cast to constant homomorphism
      result = getInitialState() ;
      break;
    case Ctlp_FwdU_c:
      /** From Vis source documentation :
       *							    t
       ** E[p U q]      = lfp Z[q V (p ^ EX(Z))]   :   p p ... p q
       ** FwdUntil(p,q) = lfp Z[p V EY(Z ^ q)]     :		    pq q q ... q
       */
      /**
       * In other words, start from states satisfying p, then add successors satisfying q to fixpoint
       * FwdUntil(p,q) =  ( q & Next  + Id)^* & p
       */
      // test for trivial reachability case
      if (Ctlp_FormulaReadLeftChild(ctlFormula) &&
	  Ctlp_FormulaReadType(Ctlp_FormulaReadLeftChild(ctlFormula)) ==
	  Ctlp_Init_c &&
	  Ctlp_FormulaReadRightChild(ctlFormula) &&
	  Ctlp_FormulaReadType(Ctlp_FormulaReadRightChild(ctlFormula)) ==
	  Ctlp_TRUE_c ) {
	// cast to constant hom
	result = getReachable() ;
	break;
      }
      // the real case
      // FwdUntil(p,q) holds at any state "t", such that there exists a path through "t" from some state at which
      // p holds, and q holds at all states before "t" on the path.
      
      result = (getNextRel() +Transition::id) (fixpoint ( (rightStates * getNextRel()) + its::Transition::id ) ( leftStates )) ;
      break;
    case Ctlp_FwdG_c:
      {
	// EH (p) is the subset of states verifying "p" that are reachable through a cycle in p
	// EH = fixpoint ( p * getNextRel() ) (getReachable);

	// Reachable (p,q) : states that verify "q" reachable from states verifying "p and q" 
	// (while constantly verifying "q")
	// Reach (p,q) = fixpoint (  (q * next) + id )  (p * q)

	// FwdGlobal(p,q) = EH ( Reachable (p,q) )

	// states reachable by an infinite path of f
	result = fixpoint (  getNextRel() 
			     * ( fixpoint ( (rightStates * getNextRel()) + Transition::id  ) ( leftStates * rightStates)  )
			     ) ( getReachable() );



	// FwdGlobal(p,q) = EH ( Reachable (p,q) )
	// Start from states p, S = p
	// Keep only those satisfying q. S = S * q
	// Add any states satisfying q, q reachable from S. S = fix( Id +  q*Next ) (S)
	// Reduce to states in cycles + suffix thereof. S = fix ( Next * Id ) (S)
	break;
      }
    case Ctlp_EY_c:
      // exists yesterday : states that have a predescessor that verifies f
      // take states verifying f, then compute successors
      // EY f  <->  Next & f (reach)
      result = getNextRel() ( leftStates ) ;
      break;
      
    default: 
      fail("Encountered unknown type of CTL formula\n");
    }

    // std::cerr << "Obtained Homomorphism : " << result << std::endl;
    
  } else {
    result = it->second;
  }  
  return result;
}

  its::Transition CTLChecker::getSelectorAP (Label apname, Label val) const {

  vLabel predicate = apname +  val;
  its::Transition pred = model.getPredicate(predicate);
//  std::cout << pred << std::endl;
  return pred;

// BEFORE PREDICATE API INTRODUCED WAS :
/**
  //  std::cerr << "asked for selector AP for :" << apname << std::endl;
  pType type =  model.getInstance()->getType();
  labels_t labs = type->getTransLabels();
  if ( find (labs.begin() , labs.end() , apname) == labs.end() ) {
    std::cerr << "Your atomic proposition identifier \"" << apname << "\" does not correspond to a known predicate. Check that it is the name of a place in your Petri net, or relevant with respect to your ITS type declaration." << std::endl;
    std::cerr << "Error is fatal, sorry." << std::endl;
    exit(1);
  }
  labels_t touse ;
  {
    touse.push_back(apname);
    
    Transition t = type->getSuccs (touse);
    if (! t.is_selector() ) {
      std::cerr << "Your atomic proposition identifier " << apname << " does not correspond to a state based predicate (it is not a selector). Check that it is the name of a place in your Petri net, or relevant with respect to your ITS type declaration." << std::endl;
      std::cerr << "Error is fatal, sorry." << std::endl;
      exit(1);
    }
    return t;
  }

  return Transition::id;
*/
}

its::Transition CTLChecker::getPredRel () const
{
  return model.getPredRel();
}

its::Transition CTLChecker::getNextRel () const
{
  return model.getNextRel() ;
}

its::State CTLChecker::getReachable () const {
  // this call is cached in ITSModel
  return model.computeReachable();
}

its::State CTLChecker::getInitialState () const {
  // this call is cached in ITSModel
  return model.getInitialState();
}

its::State CTLChecker::getReachableDeadlocks () const {
  return getReachable() - (getPredRel() ( getReachable()));
}
