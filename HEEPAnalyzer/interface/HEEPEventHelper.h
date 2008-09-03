#ifndef SHARPER_HEEPANALYZER_HEEPEVENTHELPER
#define SHARPER_HEEPANALYZER_HEEPEVENTHELPER

//class: heep::EventHelper
//Description: an event format usefull for the HEEP group
//
//Implimentation: 
// 

#include "FWCore/ParameterSet/interface/InputTag.h"

#include "SHarper/HEEPAnalyzer/interface/HEEPEleSelector.h"
#include "SHarper/HEEPAnalyzer/interface/HEEPEle.h"

#include <vector>

namespace reco{
  class BasicCluster;
  class GsfElectron;
}

namespace edm{
  class Event;
  class EventSetup;
  class ParameterSet;
}

namespace heep {
  class EvtHandles;
  class Event;

  class EventHelper {
    
     //first necessary labels
    //physics objects
    edm::InputTag eleLabel_;
    edm::InputTag muoLabel_;
    edm::InputTag jetLabel_;
    edm::InputTag tauLabel_;
    edm::InputTag metLabel_;
    edm::InputTag phoLabel_;
    //Ecal Hits
    edm::InputTag ecalRecHitsEBTag_;
    edm::InputTag ecalRecHitsEETag_;
    
    //the selection object we need
    heep::EleSelector cuts_;
    
  public:
    EventHelper(){}
    ~EventHelper(){}
    //we own nothing so default copy/assignments are fine

    void setup(const edm::ParameterSet& conf);    
    void setHandles(const edm::Event& event,const edm::EventSetup& setup,heep::EvtHandles& handles);
    

    void fillHEEPElesFromPat(const heep::EvtHandles& handles,std::vector<heep::Ele>& heepEles); 
    void addHEEPEle(const reco::GsfElectron& gsfEle,const heep::EvtHandles& handles,std::vector<heep::Ele>& heepEles);
    void fillClusShapeData(const reco::BasicCluster& seedClus,const heep::EvtHandles& handles,heep::Ele::ClusShapeData& clusShapeData);
   
    //this function just calles setHandles and then fillHEEPElesFromPat but it allows us to make the HEEPEvent in one function
    void makeHeepEvent(const edm::Event& edmEvent,const edm::EventSetup& setup,heep::Event& heepEvent);

  };
}


#endif
