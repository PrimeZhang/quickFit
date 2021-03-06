#include "inc/CommonHead.h"
#include "inc/RooFitHead.h" 
#include "inc/RooStatsHead.h"

#include <boost/program_options.hpp>

#include "inc/fitTool.h"
#include "inc/auxUtils.h"


std::string _ssname = "ucmles";
std::string _outputFile = "";
std::string _inputFile = "";
std::string _minAlgo  = "Minuit2";
std::string _dataName = "combData";
std::string _wsName = "combWS";
std::string _mcName = "ModelConfig";
std::string _snapshot = "";

std::string _poiStr = "";
std::string _fixNPStr = "";

bool _saveWS = false;
bool _saveErrors = false;
bool _checkWS = false;
bool _useHESSE = false;
bool _useMINOS = false;
bool _useSIMPLEX = false;
bool _nllOffset = true;
bool _fixStarCache = false;
float _minTolerance = 0.001;
int _minStrategy = 1;
int _optConst = 2;
int _printLevel = 2;
int _nCPU = 1;

string OKGREEN = "\033[92m";
string FAIL = "\033[91m";
string ENDC = "\033[0m";

int main( int argc, char** argv )
{
  namespace po = boost::program_options;
  po::options_description desc( "quickFit options" );
  desc.add_options()
    // IO Options 
    ( "inputFile,f",   po::value<std::string>(&_inputFile),  "Specify the input TFile (REQUIRED)" )

    ( "outputFile,o",  po::value<std::string>(&_outputFile), "Save fit results to output TFile" )

    ( "dataName,d",    po::value<std::string>(&_dataName)->default_value(_dataName),   
                         "Name of the dataset" )
    ( "wsName,w",      po::value<std::string>(&_wsName)->default_value(_wsName),
                         "Name of the workspace" )
    ( "mcName,m",      po::value<std::string>(&_mcName)->default_value(_mcName), 
                         "Name of the model config" )
    ( "snapshot,s",    po::value<std::string>(&_snapshot)->default_value(_snapshot), 
                         "Load snapshot from workspace" )
    ( "ssname,k",      po::value<std::string>(&_ssname)->default_value(_ssname), 
                         "Name of snapshot to save to output workspace" )
    // Model Options
    ( "poi,p",         po::value<std::string>(&_poiStr),     "Specify POIs to be used in fit" )

    ( "fixNP,n",       po::value<std::string>(&_fixNPStr),   "Specify NPs to be used in fit" )

    // Fit Options
    ( "simplex",       po::value<bool>(&_useSIMPLEX)->default_value(_useSIMPLEX),
                         "Estimate central values with SIMPLEX" )
    ( "hesse",         po::value<bool>(&_useHESSE)->default_value(_useHESSE),
                         "Estimate errors with HESSE after fit" )
    ( "minos",         po::value<bool>(&_useMINOS)->default_value(_useMINOS),
                         "Get asymmetric errors with MINOS fit" )
    ( "nllOffset",     po::value<bool>(&_nllOffset)->default_value(_nllOffset),         
                         "Set NLL offset" )
    ( "numCPU",      po::value<int>(&_nCPU)->default_value(_nCPU),
                         "Set number of CPUs for fit" )
    ( "minStrat",      po::value<int>(&_minStrategy)->default_value(_minStrategy),
                         "Set minimizer strategy" )
    ( "optConst",      po::value<int>(&_optConst)->default_value(_optConst),
                         "Set optimize constant" ) ( "printLevel",    po::value<int>(&_printLevel)->default_value(_printLevel),
                         "Set minimizer print level" )
    ( "minTolerance",  po::value<float>(&_minTolerance)->default_value(_minTolerance),
                         "Set minimizer tolerance" )
    ( "saveWS",        po::value<bool>(&_saveWS)->default_value(_saveWS),
                         "Save postfit workspace to the output file" )
    ( "saveErrors",    po::value<bool>(&_saveErrors)->default_value(_saveErrors),
                         "Save errors in the TTree" )
    // Other
    ( "help,h",          "Print help message")

    ( "checkWS",       po::value<bool>(&_checkWS)->default_value(_checkWS),
                         "Perform sanity checks on workspace before fit." )
    ( "fixStarCache",  po::value<bool>(&_fixStarCache)->default_value(_fixStarCache),
                         "Fix cache in RooStarMomentMorph." )
    ;

  po::variables_map vm;
  try
  {
    po::store( po::command_line_parser( argc, argv ).options( desc ).run(), vm );
    po::notify( vm );
  }
  catch ( std::exception& ex )
  {
    std::cerr << "Invalid options: " << ex.what() << std::endl;
    std::cout << "Invalid options: " << ex.what() << std::endl;
    std::cout << "Use manager --help to get a list of all the allowed options"  << std::endl;
    return 999;
  }
  catch ( ... )
  {
    std::cerr << "Unidentified error parsing options." << std::endl;
    return 1000;
  }

  // if help, print help
  if ( !vm.count("inputFile") || vm.count( "help" ) )
  {
    std::cout << "Usage: manager [options]\n";
    std::cout << desc;
    return 0;
  }

  RooMsgService::instance().getStream(1).removeTopic(RooFit::NumIntegration) ;
  RooMsgService::instance().getStream(1).removeTopic(RooFit::Fitting) ;
  RooMsgService::instance().getStream(1).removeTopic(RooFit::Minimization) ;
  RooMsgService::instance().getStream(1).removeTopic(RooFit::InputArguments) ;
  RooMsgService::instance().getStream(1).removeTopic(RooFit::Eval) ;
  RooMsgService::instance().setGlobalKillBelow(RooFit::ERROR);
  
  // Set fit options
  fitTool *fitter = new fitTool();
  fitter->setMinAlgo( (TString) _minAlgo );
  fitter->useHESSE( _useHESSE );
  fitter->useMINOS( _useMINOS );
  fitter->useSIMPLEX( _useSIMPLEX );
  fitter->setNLLOffset( _nllOffset );
  fitter->setTolerance( _minTolerance );
  fitter->setStrategy( _minStrategy );
  fitter->setOptConst( _optConst );
  fitter->setPrintLevel( _printLevel );
  fitter->setNCPU( _nCPU );
  fitter->setOutputFile( (TString) _outputFile );
  fitter->setSnapshotName( (TString) _ssname );
  fitter->saveWorkspace( _saveWS );
  fitter->saveErrors(_saveErrors);
  fitter->setFixStarCache( _fixStarCache );

  // Get workspace, model, and data from file
  TFile *tf = TFile::Open( (TString) _inputFile );
  if (not tf->IsOpen()) {
    std::cout << "Error: TFile \'" << _inputFile << "\' was not found." << endl;
    return 0;
  }

  RooWorkspace *ws = (RooWorkspace*)tf->Get( (TString) _wsName );
  if (ws == nullptr) {
    std::cout << "Error: Workspace \'" << _wsName << "\' does not exist in the TFile." << endl;
    return 0;
  }
  
  RooStats::ModelConfig *mc = (RooStats::ModelConfig*)ws->obj( (TString) _mcName );
  if (mc == nullptr) {
    std::cout << "Error: ModelConfig \'" << _mcName << "\' does not exist in workspace." << endl;
    return 0;
  }
  
  RooAbsData *data = ws->data( (TString) _dataName );
  if (data == nullptr) {
    std::cout << "Error: Dataset \'" << _dataName << "\' does not exist in workspace." << endl;
    return 0;
  }

  if (_snapshot != "") {
    if (not ws->loadSnapshot( (TString) _snapshot )) {
      std::cout << "Error: Unable to load snapshot " << _snapshot << " from workspace." << endl;
      return 0;
    }
  }
  
  // save a snapshot of everything as is
  RooArgSet everything;
  utils::collectEverything(mc, &everything);
  ws->saveSnapshot( "original", everything);

  // Prepare model as expected
  utils::setAllConstant( mc->GetGlobalObservables(), true );
  utils::setAllConstant( mc->GetNuisanceParameters(), false );
  utils::setAllConstant( mc->GetParametersOfInterest(), true );

  // Sanity checks on model 
  if (_checkWS) {
    cout << "Performing sanity checks on model..." << endl;
    bool validModel = fitter->checkModel( *mc, true );
    cout << "Sanity checks on the model: " << (validModel ? "OK" : "FAIL") << endl;
  }

  // Fix nuisance narameters
  if ( vm.count("fixNP") ) {
    cout << endl << "Fixing nuisance parameters : " << endl;
    std::vector<std::string> fixNPStrs = auxUtils::Tokenize( _fixNPStr, "," );
    for( unsigned int inp(0); inp < fixNPStrs.size(); inp++ ) {
      RooAbsCollection *fixNPs = mc->GetNuisanceParameters()->selectByName( (TString) fixNPStrs[inp]);
      for (RooLinkedListIter it = fixNPs->iterator(); RooRealVar* NP = dynamic_cast<RooRealVar*>(it.Next());) {
        cout << "   Fixing nuisance parameter " << NP->GetName() << endl;
        NP->setConstant( kTRUE );
      }
    }
  }

  // Prepare parameters of interest
  RooArgSet fitPOIs;
  if ( vm.count("poi") ) {
    cout << endl << "Preparing parameters of interest :" << endl;
    std::vector<std::string> poiStrs = auxUtils::Tokenize( _poiStr, "," );
    for( unsigned int ipoi(0); ipoi < poiStrs.size(); ipoi++ ) {
      std::vector<std::string> poiTerms = auxUtils::Tokenize( poiStrs[ipoi], "=" );
      TString poiName = (TString) poiTerms[0];

      // check if variable is in workspace
      if (not ws->var(poiName))  {
        cout << FAIL << "Variable " << poiName << " not in workspace. Skipping." << ENDC << endl;
        continue;
      }

      // set variable for fit
      fitPOIs.add( *(ws->var(poiName)) );
      if (poiTerms.size() > 1) {
        std::vector<std::string> poiVals = auxUtils::Tokenize( poiTerms[1], "_" );
        if (poiVals.size() == 3) {
          ws->var(poiName)->setRange( std::stof(poiVals[1]), std::stof(poiVals[2]) );
          ws->var(poiName)->setVal( std::stof(poiVals[0]) );
          ws->var(poiName)->setConstant( kFALSE );
        } else {
          if ( std::stof(poiVals[0]) > ws->var(poiName)->getMax() ) {
            ws->var(poiName)->setRange( ws->var(poiName)->getMin(), 2*std::stof(poiVals[0]) );
          }
          if ( std::stof(poiVals[0]) < ws->var(poiName)->getMin() ) {
            ws->var(poiName)->setRange( -2*abs(std::stof(poiVals[0])), ws->var(poiName)->getMax() );
          }
          ws->var(poiName)->setVal( std::stof(poiVals[0]) );
          ws->var(poiName)->setConstant( kTRUE );
        }
        cout << "   ";
        ws->var(poiName)->Print();
      } else {
        ws->var(poiName)->setConstant( kFALSE );
        cout << "   ";
        ws->var(poiName)->Print();
      }
    }

  } else {
    RooRealVar *firstPOI = (RooRealVar*)mc->GetParametersOfInterest()->first();
    cout << endl << "No POIs specified. Will only float the first POI " << firstPOI->GetName() << endl;
    firstPOI->setConstant(kFALSE);
    cout << "   ";
    firstPOI->Print();
    fitPOIs.add( *firstPOI );
  }

  mc->SetParametersOfInterest( fitPOIs );

  // Fitting 
  TStopwatch timer;
  cout << endl << "Starting fit..." << endl;
  int status = fitter->profileToData( mc, data ); // Perform fit
  timer.Stop();
  double t_cpu_ = timer.CpuTime()/60.;
  double t_real_ = timer.RealTime()/60.;
  printf("\nAll fits done in %.2f min (cpu), %.2f min (real)\n", t_cpu_, t_real_);

  string STATMSG = (status) ? "\033[91m STATUS FAILED \033[0m" : "\033[92m STATUS OK \033[0m" ;

  // Print summary 
  cout << endl << "  Fit Summary of POIs (" << STATMSG << ")" << endl;
  cout << "------------------------------------------------" << endl;
  for (RooLinkedListIter it = fitPOIs.iterator(); RooRealVar* POI = dynamic_cast<RooRealVar*>(it.Next());) {
    if (POI->isConstant()) continue;
    POI->Print();
  }

  if (status) {
    cout << FAIL << endl;
    cout << "   *****************************************" << endl;
    cout << "          WARNING: Fit status failed.       " << endl;
    cout << "   *****************************************" << ENDC << endl;
  }

  cout << endl;
  return 1;
}
