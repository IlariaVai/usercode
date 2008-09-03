
#include "SHarper/HEEPAnalyzer/interface/HEEPEventHelper.h"

#include "SHarper/HEEPAnalyzer/interface/HEEPEvtHandles.h"
#include "SHarper/HEEPAnalyzer/interface/HEEPEvent.h"

#include "DataFormats/EcalDetId/interface/EcalSubdetector.h"
#include "DataFormats/EgammaReco/interface/BasicCluster.h"
#include "DataFormats/EgammaCandidates/interface/GsfElectron.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "Geometry/Records/interface/CaloTopologyRecord.h"
#include "RecoEcal/EgammaCoreTools/interface/EcalClusterTools.h"

void heep::EventHelper::setup(const edm::ParameterSet& conf)
{

  eleLabel_ = conf.getUntrackedParameter<edm::InputTag>("electronTag");
  muoLabel_ = conf.getUntrackedParameter<edm::InputTag>("muonTag");
  jetLabel_ = conf.getUntrackedParameter<edm::InputTag>("jetTag");
  tauLabel_ = conf.getUntrackedParameter<edm::InputTag>("tauTag");
  metLabel_ = conf.getUntrackedParameter<edm::InputTag>("metTag");
  phoLabel_ = conf.getUntrackedParameter<edm::InputTag>("photonTag");
  ecalRecHitsEBTag_ = conf.getParameter<edm::InputTag>("barrelRecHitCollection");
  ecalRecHitsEETag_ = conf.getParameter<edm::InputTag>("endcapRecHitCollection");

}


void heep::EventHelper::setHandles(const edm::Event& event,const edm::EventSetup& setup,heep::EvtHandles& handles)
{
  
  //yay, now in 2_1 we dont have to program by exception
  event.getByLabel(muoLabel_,handles.muon);
  event.getByLabel(jetLabel_,handles.jet);
  event.getByLabel(eleLabel_,handles.electron);
  event.getByLabel(metLabel_,handles.met);
  event.getByLabel(phoLabel_,handles.pho);
  event.getByLabel(tauLabel_,handles.tau);
  event.getByLabel(ecalRecHitsEBTag_,handles.ebRecHits);
  event.getByLabel(ecalRecHitsEETag_,handles.eeRecHits);
  setup.get<CaloGeometryRecord>().get(handles.caloGeom);
  setup.get<CaloTopologyRecord>().get(handles.caloTopology);
 
}

//fills the heepEles vector using pat electrons as starting point
void heep::EventHelper::fillHEEPElesFromPat(const heep::EvtHandles& handles,std::vector<heep::Ele>& heepEles)
{
  heepEles.clear();
  const edm::View<pat::Electron> eles = *handles.electron;
  for(edm::View<pat::Electron>::const_iterator eleIt = eles.begin(); eleIt!=eles.end(); ++eleIt){
    addHEEPEle(*eleIt,handles,heepEles);
  }
}

//this converts the gsfElectron into a heep::Ele and adds it to the vector
//the reason we are passing in one at a time is so we can use pat or normal electrons
//as a pat electron inherits from GsfElectron but a pat collection wouldnt
void heep::EventHelper::addHEEPEle(const reco::GsfElectron& gsfEle,const heep::EvtHandles& handles,std::vector<heep::Ele>& heepEles)
{
 
  //for now use dummy isolation data
  heep::Ele::IsolData isolData;
  isolData.nrTrks=999;
  isolData.ptTrks=999.;
  isolData.em= 999.;
  isolData.hadDepth1=999.;
  isolData.hadDepth2=999.;
    
  const reco::BasicCluster& seedClus = *(gsfEle.superCluster()->seed());
  heep::Ele::ClusShapeData clusShapeData;
  fillClusShapeData(seedClus,handles,clusShapeData);
  
  heepEles.push_back(heep::Ele(gsfEle,clusShapeData,isolData));
  
  //now we would like to set the cut results
  heep::Ele& ele =  heepEles.back();
  ele.setCutCode(cuts_.getCutCode(ele));
  

}


void heep::EventHelper::fillClusShapeData(const reco::BasicCluster& seedClus,const heep::EvtHandles& handles,heep::Ele::ClusShapeData& clusShapeData)
{
  clusShapeData.sigmaEtaEta=999.;
  clusShapeData.sigmaIEtaIEta=999.;
  clusShapeData.e2x5MaxOver5x5=-1.; 
  clusShapeData.e1x5Over5x5=-1.;

  
  const DetId firstDetId = seedClus.getHitsByDetId()[0]; //note this  not actually be the seed hit but it doesnt matter because all hits will be in the barrel OR endcap (it is also incredably inefficient as it getHitsByDetId passes the vector by value not reference)

  const CaloGeometry* caloGeom = handles.caloGeom.product();
  const CaloTopology* caloTopology = handles.caloTopology.product();
  const EcalRecHitCollection* ebRecHits = handles.ebRecHits.product();
  const EcalRecHitCollection* eeRecHits = handles.eeRecHits.product();

  if(firstDetId.subdetId()==EcalBarrel){
    std::vector<float> stdCov = EcalClusterTools::covariances(seedClus,ebRecHits,caloTopology,caloGeom);
    std::vector<float> crysCov = EcalClusterTools::localCovariances(seedClus,ebRecHits,caloTopology,caloGeom);
    clusShapeData.sigmaEtaEta = sqrt(stdCov[0]);
    clusShapeData.sigmaIEtaIEta =  sqrt(crysCov[0]); 
    float e5x5 =  EcalClusterTools::e5x5(seedClus,ebRecHits,caloTopology);
    if(e5x5!=0.) {
      clusShapeData.e2x5MaxOver5x5 = EcalClusterTools::e2x5Max(seedClus,ebRecHits,caloTopology)/e5x5;
      clusShapeData.e1x5Over5x5 = EcalClusterTools::e5x1(seedClus,ebRecHits,caloTopology)/e5x5; //dont ask
    }
  }else if(firstDetId.subdetId()==EcalEndcap){ //only fill sigmaEtaEta at the moment
    std::vector<float> stdCov = EcalClusterTools::covariances(seedClus,eeRecHits,caloTopology,caloGeom);
    clusShapeData.sigmaEtaEta = sqrt(stdCov[0]);
  }
}

void heep::EventHelper::makeHeepEvent(const edm::Event& edmEvent,const edm::EventSetup& setup,heep::Event& heepEvent)
{
  setHandles(edmEvent,setup,heepEvent.handles());
  fillHEEPElesFromPat(heepEvent.handles(),heepEvent.heepElectrons());
}
