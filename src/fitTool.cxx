#include "fitTool.h"

fitTool::fitTool() {
  // Default values for the minimizer
  _minAlgo = "Minuit2";
  _minTolerance = 1E-03;
  _minStrat = 0;
  _nllOffset = true;
  _optConst = 0;
  _printLevel = 2;
  _useHESSE = true;
  _useMINOS = true;
}



bool fitTool::checkModel(const RooStats::ModelConfig &model, bool throwOnFail) {
// ----------------------------------------------------------------------------------------------------- 
  bool ok = true; 
  std::ostringstream errors;
  std::auto_ptr<TIterator> iter;
  RooAbsPdf *pdf = model.GetPdf(); 
  if (pdf == 0) throw std::invalid_argument("Model without Pdf");
  
  RooArgSet allowedToFloat;
  
  // Check model observables
  if (model.GetObservables() == 0) {
    ok = false; errors << "ERROR: model does not define observables.\n";
    std::cout << errors.str() << std::endl;
    if (throwOnFail) throw std::invalid_argument(errors.str()); else return false;
  } else {
    allowedToFloat.add(*model.GetObservables());
  }

  // Check model parameters of interset
  if (model.GetParametersOfInterest() == 0) {
    ok = false; errors << "ERROR: model does not define parameters of interest.\n";
  } else {
    iter.reset(model.GetParametersOfInterest()->createIterator());
    for (RooAbsArg *a = (RooAbsArg *) iter->Next(); a != 0; a = (RooAbsArg *) iter->Next()) {
      RooRealVar *v = dynamic_cast<RooRealVar *>(a);
      if (!v) { 
        errors << "ERROR: parameter of interest " << a->GetName() << " is a " << a->ClassName() << " and not a RooRealVar\n"; 
        ok = false; continue; 
      }
      if (!pdf->dependsOn(*v)) { 
        errors << "ERROR: pdf does not depend on parameter of interest " << a->GetName() << "\n"; 
        ok = false; continue; 
      }
      allowedToFloat.add(*v);
    }
  }
  
  // Check model nuisance parameters 
  if (model.GetNuisanceParameters() != 0) {
    iter.reset(model.GetNuisanceParameters()->createIterator());
    for (RooAbsArg *a = (RooAbsArg *) iter->Next(); a != 0; a = (RooAbsArg *) iter->Next()) {
      RooRealVar *v = dynamic_cast<RooRealVar *>(a);
      if (!v) { 
        errors << "ERROR: nuisance parameter " << a->GetName() << " is a " << a->ClassName() << " and not a RooRealVar\n"; 
        ok = false; continue; 
      }
      if (v->isConstant()) { 
        errors << "ERROR: nuisance parameter " << a->GetName() << " is constant\n"; 
        ok = false; continue; 
      }
      if (!pdf->dependsOn(*v))
      {
          errors << "WARNING: pdf does not depend on nuisance parameter, removing " << a->GetName() << "\n";
          const_cast<RooArgSet*>(model.GetNuisanceParameters())->remove(*a);
          continue;
      }
      allowedToFloat.add(*v);
    }
  }

  // check model global observables 
  if (model.GetGlobalObservables() != 0) {
    iter.reset(model.GetGlobalObservables()->createIterator());
    for (RooAbsArg *a = (RooAbsArg *) iter->Next(); a != 0; a = (RooAbsArg *) iter->Next()) {
      RooRealVar *v = dynamic_cast<RooRealVar *>(a);
      if (!v) { ok = false; errors << "ERROR: global observable " << a->GetName() << " is a " << a->ClassName() << " and not a RooRealVar\n"; continue; }
      if (!v->isConstant()) { ok = false; errors << "ERROR: global observable " << a->GetName() << " is not constant\n"; continue; }
      if (!pdf->dependsOn(*v)) { errors << "WARNING: pdf does not depend on global observable " << a->GetName() << "\n"; continue; }
    }
  }

  // check the rest of the pdf
  std::auto_ptr<RooArgSet> params(pdf->getParameters(*model.GetObservables()));
  iter.reset(params->createIterator());
  for (RooAbsArg *a = (RooAbsArg *) iter->Next(); a != 0; a = (RooAbsArg *) iter->Next()) {
    if (a->isConstant() || allowedToFloat.contains(*a)) continue;
    if (a->getAttribute("flatParam")) continue;
    errors << "WARNING: pdf parameter " << a->GetName() << " (type " << a->ClassName() << ")"
            << " is not allowed to float (it's not nuisance, poi, observable or global observable\n";
  }
  iter.reset();
  std::cout << errors.str() << std::endl;
  if (!ok && throwOnFail) throw std::invalid_argument(errors.str());
  return ok;
}



int fitTool::profileToData(ModelConfig *mc, RooAbsData *data){
// ----------------------------------------------------------------------------------------------------- 
  RooAbsPdf *pdf=mc->GetPdf();
  RooWorkspace *w=mc->GetWS();
  RooArgSet funcs = w->allPdfs();
  std::auto_ptr<TIterator> iter(funcs.createIterator());
  for ( RooAbsPdf* v = (RooAbsPdf*)iter->Next(); v!=0; v = (RooAbsPdf*)iter->Next() ) {
    std::string name = v->GetName();
    if (v->IsA() == RooRealSumPdf::Class()) {
      std::cout << "\tset binned likelihood for: " << v->GetName() << std::endl;
      v->setAttribute("BinnedLikelihood", true);
    }
  }

  unique_ptr<RooAbsReal> nll( pdf->createNLL( *data, 
          Constrain(*mc->GetNuisanceParameters()), GlobalObservables(*mc->GetGlobalObservables()) ));
  nll->enableOffsetting(_nllOffset);

  RooMinimizer minim(*nll);
  minim.setStrategy( _minStrat );
  minim.setPrintLevel( _printLevel-1 );
  minim.setProfile(); /* print out time */
  minim.setEps( _minTolerance / 0.001 );
  minim.optimizeConst( _optConst );

  //int status = minim.simplex();
  int status = minim.minimize( _minAlgo );

  if ( _useHESSE ) {
    cout << endl << "Starting fit with HESSE..." << endl;
    status &= minim.hesse();
  }

  if ( _useMINOS ) {
    cout << endl << "Starting fit with MINOS..." << endl;
    status &= minim.minos( *mc->GetParametersOfInterest() );
  }

  return status;
}


