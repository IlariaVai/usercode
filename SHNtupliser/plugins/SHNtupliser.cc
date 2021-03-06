#include "SHarper/SHNtupliser/interface/SHNtupliser.h"

#include "SHarper/SHNtupliser/interface/SHEvent.hh"
#include "SHarper/SHNtupliser/interface/SHCaloGeom.hh"
#include "SHarper/SHNtupliser/interface/GeomFuncs.hh"
#include "SHarper/SHNtupliser/interface/SHGeomFiller.h"
#include "SHarper/SHNtupliser/interface/TrigDebugObjHelper.h"
#include "SHarper/SHNtupliser/interface/SHTrigObjContainer.hh"
#include "SHarper/SHNtupliser/interface/SHPFCandContainer.hh"

#include "SHarper/HEEPAnalyzer/interface/HEEPDebug.h"


#include "DataFormats/EgammaReco/interface/SuperClusterFwd.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/L1Trigger/interface/L1EmParticleFwd.h"
#include "DataFormats/L1Trigger/interface/L1EmParticle.h"

#include "CondFormats/L1TObjects/interface/L1GtTriggerMenu.h"
#include "CondFormats/DataRecord/interface/L1GtTriggerMenuRcd.h"
#include "DataFormats/L1GlobalTrigger/interface/L1GlobalTriggerReadoutSetupFwd.h"
#include "DataFormats/L1GlobalTrigger/interface/L1GlobalTriggerReadoutSetup.h"
#include "DataFormats/L1GlobalTrigger/interface/L1GlobalTriggerReadoutRecord.h"

#include "RecoEgamma/EgammaIsolationAlgos/interface/EgammaTowerIsolation.h"
#include "RecoEcal/EgammaCoreTools/interface/EcalClusterTools.h"
#include "DataFormats/EcalDetId/interface/EEDetId.h"
#include "TFile.h"
#include "TTree.h"

#include "SimDataFormats/PileupSummaryInfo/interface/PileupSummaryInfo.h" 
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

void filterHcalHits(const SHEvent* event,double maxDR,const SHCaloHitContainer& inputHits,SHCaloHitContainer& outputHits);
void filterEcalHits(const SHEvent* event,double maxDR,const SHCaloHitContainer& inputHits,SHCaloHitContainer& outputHits);
void filterCaloTowers(const SHEvent* event,double maxDR,const SHCaloTowerContainer& inputHits,SHCaloTowerContainer& outputHits);
void fillPFCands(const SHEvent* event,double maxDR,SHPFCandContainer& shPFCands,const std::vector<reco::PFCandidate>& pfCands,const reco::VertexRef mainVtx,const edm::Handle< reco::VertexCollection > vertices);
reco::VertexRef chargedHadronVertex( const reco::PFCandidate& pfcand,edm::Handle< reco::VertexCollection > verticesColl);

void SHNtupliser::initSHEvent()
{
  if(shEvt_) delete shEvt_;
  shEvt_ = new SHEvent;
}

void SHNtupliser::fillTree()
{
  evtTree_->Fill();
}

//some hacky functions to allow use to reset the trigger energies
void addInDeLaseredTriggerNrgys(const heep::Event& heepEvent,SHEvent& shEvent);
void setTrigObsToNewNrgy(const reco::SuperClusterCollection& ebSCs,const reco::SuperClusterCollection& eeSCs,trigger::TriggerObjectCollection& trigObjs);


SHNtupliser::SHNtupliser(const edm::ParameterSet& iPara):
  evtHelper_(),heepEvt_(),shEvtHelper_(),shEvt_(NULL),evtTree_(NULL),outFile_(NULL),nrTot_(0),nrPass_(0),initGeom_(false),trigDebugHelper_(NULL),shTrigObjs_(NULL),shTrigObjs2ndTrig_(NULL),shEvt2ndTrig_(NULL),puSummary_(NULL),writePUInfo_(true),shPFCands_(NULL)
{
  evtHelper_.setup(iPara);
  shEvtHelper_.setup(iPara);

  outputFilename_ = iPara.getParameter<std::string>("outputFilename");
  
  double eventWeight = iPara.getParameter<double>("sampleWeight");
  int datasetCode = iPara.getParameter<int>("datasetCode");  
  outputGeom_ = iPara.getParameter<bool>("outputGeom");
   
  minSCEtToPass_ = iPara.getParameter<double>("minSCEtToPass");
  minNrSCToPass_ = iPara.getParameter<int>("minNrSCToPass");
  
  minJetEtToPass_ = iPara.getParameter<double>("minJetEtToPass");
  minNrJetToPass_ = iPara.getParameter<int>("minNrJetToPass");
  
  shEvtHelper_.setDatasetCode(datasetCode);
  shEvtHelper_.setEventWeight(eventWeight);
 
  useHLTDebug_ = iPara.getParameter<bool>("useHLTDebug");
  compTwoMenus_ = iPara.getParameter<bool>("compTwoMenus");
  hltTag_ = iPara.getParameter<std::string>("hltProcName");
  secondHLTTag_ = iPara.getParameter<std::string>("secondHLTTag");
  addCaloTowers_ = iPara.getParameter<bool>("addCaloTowers");
  addCaloHits_ = iPara.getParameter<bool>("addCaloHits");
  addPFCands_=iPara.getParameter<bool>("addPFCands");
  addIsolTrks_ = iPara.getParameter<bool>("addIsolTrks");
  addPreShowerClusters_ = false;
  writePDFInfo_ = iPara.getParameter<bool>("writePDFInfo");
  if(useHLTDebug_){
    trigDebugHelper_ = new TrigDebugObjHelper(iPara);
  }
}

SHNtupliser::~SHNtupliser()
{
  if(shEvt_) delete shEvt_;
  //if(outFile_) delete outFile_;
  if(trigDebugHelper_) delete trigDebugHelper_;
  if(shTrigObjs_) delete shTrigObjs_;
  if(puSummary_) delete puSummary_;
  if(shPFCands_) delete shPFCands_;
}

void SHNtupliser::beginJob()
{
  initSHEvent();
  shCaloTowers_ = &(shEvt_->getCaloTowers());
  shCaloHits_= &(shEvt_->getCaloHits());
  shIsolTrks_= &(shEvt_->getIsolTrks());
  shPreShowerClusters_ = &(shEvt_->getPreShowerClusters());
 
  std::cout <<"opening file "<<outputFilename_.c_str()<<std::endl;
  //  outFile_ = new TFile(outputFilename_.c_str(),"RECREATE");
  edm::Service<TFileService> fs;
  outFile_ = &fs->file();
  outFile_->cd();
  evtTree_= new TTree("evtTree","Event Tree");
 
  int splitLevel=2;
  evtTree_->SetCacheSize(1024*1024*100);
					       
  evtTree_->Branch("EventBranch",shEvt_->GetName(),&shEvt_,32000,splitLevel);
  
  if(writePUInfo_) {
    puSummary_ = new SHPileUpSummary;
    evtTree_->Branch("PUInfoBranch","SHPileUpSummary",&puSummary_,32000,splitLevel);
  }
  if(addCaloTowers_) {
    evtTree_->Branch("CaloTowersBranch","SHCaloTowerContainer",&shCaloTowers_,32000,splitLevel);
  }
  if(addCaloHits_){
    evtTree_->Branch("CaloHitsBranch","SHCaloHitContainer",&shCaloHits_,32000,splitLevel);
  }
  if(addIsolTrks_){
    evtTree_->Branch("IsolTrksBranch","TClonesArray",&shIsolTrks_,32000,splitLevel);
  }
  if(addPreShowerClusters_){
    evtTree_->Branch("PreShowerClustersBranch","TClonesArray",&shPreShowerClusters_,32000,splitLevel);
  }
  

  if(addPFCands_){ 
    shPFCands_= new SHPFCandContainer;
    evtTree_->Branch("PFCandsBranch","SHPFCandContainer",&shPFCands_,32000,splitLevel);
  }
  if(compTwoMenus_){
    shEvt2ndTrig_ = new SHEvent;
    evtTree_->Branch("Event2ndTrig","SHEvent",&shEvt2ndTrig_,32000,splitLevel);
  }
  if(useHLTDebug_) {
    shTrigObjs_ = new SHTrigObjContainer;
    evtTree_->Branch("HLTDebugObjs","SHTrigObjContainer",&shTrigObjs_,32000,splitLevel); 
    
    if(compTwoMenus_){
      shTrigObjs2ndTrig_ = new SHTrigObjContainer;
      evtTree_->Branch("HLTDebugObjs2","SHTrigObjContainer",&shTrigObjs2ndTrig_,32000,splitLevel);
    }

  }

  if(writePDFInfo_){
    evtTree_->Branch("PDFWeights",&pdfWeightsVec_);
  }

  // scTree_=new TTree("scTree","tree");
  // scTree_->Branch("sc",&oldSigmaIEtaIEta_,"oldSigmaIEtaIEta/F:newSigmaIEtaIEta:affectedByCaloNavBug:scNrgy:scEta:scPhi:scEt");


 
 
} 

void SHNtupliser::beginRun(const edm::Run& run,const edm::EventSetup& iSetup)
{ 
  std::cout <<"begin run "<<initGeom_<<std::endl;
  if(!initGeom_){
  //write out calogeometry
   
    SHGeomFiller geomFiller(iSetup);  
    SHCaloGeom ecalGeom(SHCaloGeom::ECAL);
    SHCaloGeom hcalGeom(SHCaloGeom::HCAL);
    geomFiller.fillEcalGeom(ecalGeom);
    geomFiller.fillHcalGeom(hcalGeom);
    if(outputGeom_){
      std::cout <<"writing geom "<<std::endl;
      outFile_->WriteObject(&ecalGeom,"ecalGeom");
      outFile_->WriteObject(&hcalGeom,"hcalGeom");
    }
    GeomFuncs::loadCaloGeom(ecalGeom,hcalGeom);
    initGeom_=true;
  }
  std::cout <<"end begin run "<<std::endl;
  
  heepEvt_.initHLTConfig(run,iSetup,hltTag_);
}

void SHNtupliser::analyze(const edm::Event& iEvent,const edm::EventSetup& iSetup)
{
  
  if(fillSHEvent(iEvent,iSetup)) evtTree_->Fill();
}

bool SHNtupliser::fillSHEvent(const edm::Event& iEvent,const edm::EventSetup& iSetup)
{
  //std::cout <<"heep eventing" <<std::endl;
  evtHelper_.makeHeepEvent(iEvent,iSetup,heepEvt_);
 
  //even easier to convert from heep to shEvt
  //std::cout <<"converting eventing" <<std::endl;
  pdfWeightsVec_.clear();
  
  nrTot_++;
 
  
  shEvtHelper_.makeSHEvent(heepEvt_,*shEvt_);

  if(addPFCands_) shPFCands_->clear();
  // std::cout <<"adding PF Cands "<<addPFCands_<<" is valid "<<heepEvt_.handles().pfCandidate.isValid()<<std::endl;
  if(heepEvt_.handles().vertices.isValid()){
    reco::VertexRef mainVtx(heepEvt_.handles().vertices,0);
    if(addPFCands_ && heepEvt_.handles().pfCandidate.isValid()) fillPFCands(shEvt_,0.5,*shPFCands_,heepEvt_.pfCands(),mainVtx,heepEvt_.handles().vertices);
  }
  

  // std::cout <<"made even "<<std::endl;
  if(useHLTDebug_) trigDebugHelper_->fillDebugTrigObjs(iEvent,shTrigObjs_);
  if(compTwoMenus_){ //ugly hack alert...
    shEvt2ndTrig_->clear();
    if(useHLTDebug_){
      trigDebugHelper_->setHLTTag(secondHLTTag_);
      trigDebugHelper_->fillDebugTrigObjs(iEvent,shTrigObjs2ndTrig_);
      trigDebugHelper_->setHLTTag(hltTag_);
    }
    shEvtHelper_.addEventPara(heepEvt_,*shEvt2ndTrig_);
    edm::Handle<trigger::TriggerEvent> trigEvt2nd;
    edm::Handle<edm::TriggerResults> trigResults2nd;
    iEvent.getByLabel(edm::InputTag("hltTriggerSummaryAOD","",secondHLTTag_),trigEvt2nd);
    iEvent.getByLabel(edm::InputTag("TriggerResults","",secondHLTTag_),trigResults2nd);
    const edm::TriggerNames& trigNames2nd = iEvent.triggerNames(*trigResults2nd);
    shEvtHelper_.addTrigInfo(*trigEvt2nd,*trigResults2nd,trigNames2nd,*shEvt2ndTrig_);
  }

  if(writePUInfo_){ //naughty but its almost 1am...
    puSummary_->clear();
    edm::InputTag PileupSrc_("addPileupInfo");
    edm::Handle<std::vector< PileupSummaryInfo > >  PupInfo;
    iEvent.getByLabel(PileupSrc_, PupInfo);
    if(PupInfo.isValid()){
      std::vector<PileupSummaryInfo>::const_iterator PVI;
      // (then, for example, you can do)
      for(PVI = PupInfo->begin(); PVI != PupInfo->end(); ++PVI) {
	puSummary_->addPUInfo( PVI->getBunchCrossing(),PVI->getPU_NumInteractions(),PVI->getTrueNumInteractions());
	//std::cout << " Pileup Information: bunchXing, nvtx: " << PVI->getBunchCrossing() << " " << PVI->getPU_NumInteractions() << std::endl;	
      }
    }
  }
  
  if(writePDFInfo_){
    edm::Handle<std::vector<double> > pdfWeightsHandle;
    edm::InputTag pdfTag("pdfWeights:cteq66");
    iEvent.getByLabel(pdfTag,pdfWeightsHandle);
    if(pdfWeightsHandle.isValid()) pdfWeightsVec_ = *pdfWeightsHandle;
  }
  
//   bool passSC=false;
  
//   int nrSCPassing=0;
//   for(int scNr=0;scNr<shEvt_->nrSuperClus();scNr++){
//     if(shEvt_->getSuperClus(scNr)->et()>minSCEtToPass_){
//       nrSCPassing++;
//     }
//   }
//   if(nrSCPassing>=minNrSCToPass_) passSC=true;
  
    
//   bool passJet=false;
//   int nrJetPassing=0;
//   for(int jetNr=0;jetNr<shEvt_->nrJets();jetNr++){
//     if(shEvt_->getJet(jetNr)->et()>minJetEtToPass_){
//       nrJetPassing++;
//     }
//   }
//   if(nrJetPassing>=minNrJetToPass_) passJet=true;
  
  
  int nrEle=0;
  for(int eleNr=0;eleNr<shEvt_->nrElectrons();eleNr++){
    const SHElectron* ele = shEvt_->getElectron(eleNr);
    if(ele->isEcalDriven() && ele->et()>25 && ele->trkPt()>0.2) nrEle++;
  }
  bool passEle=nrEle>=1;
  
  
  SHCaloHitContainer outputHits;
  filterHcalHits(shEvt_,0.5,shEvt_->getCaloHits(),outputHits);  
  filterEcalHits(shEvt_,0.5,shEvt_->getCaloHits(),outputHits);
  shEvt_->addCaloHits(outputHits);
    
  // SHCaloTowerContainer outputTowers;
  //  filterCaloTowers(shEvt_,0.5,shEvt_->getCaloTowers(),outputTowers);  
  //shEvt_->addCaloTowers(outputTowers);
  
  // addInDeLaseredTriggerNrgys(heepEvt_,*shEvt_);

  if(shEvt_->datasetCode()>130 && shEvt_->datasetCode()<700){ //for all non Z MC
    shEvt_->getCaloHits().clear();
    shEvt_->clearTrigs();
  }  
  passEle=true; //moved to a seperate filter run first
  if(passEle || !(shEvt_->datasetCode()>=120 && shEvt_->datasetCode()<700)){ //only for phoJet, qcdJet, actually sod it everything but Z
    nrPass_++;
    return true;
    
  }else return false;
  

}



//detPhi,detEta = SC eta/phi (HLT uses SC eta/phi except for the EleID trigger which uses Ele eta/phi)
//maxDeltaR = size of cone to match in (only works for HLT, not L1...)
//filterName = name of filter, note unless you have HLT debug, only last filter is stored
//hltTag = the process name of the HLT, usually HLT but may be different if the HLT was re-run
//note this is whether an object passes the filter, NOT if the filter was passed (the two are different in the case of multi object filters)
bool passFilter(const edm::Event& iEvent,float detEta,float detPhi,std::string filterName,const std::string hltTag,const double maxDeltaR=0.1)
{    
  
  edm::Handle<trigger::TriggerEvent> trigEvt;
  iEvent.getByLabel("hltTriggerSummaryAOD",hltTag,trigEvt);
  

  size_t filterNrInEvt = trigEvt->filterIndex(edm::InputTag(filterName,"",hltTag).encode());
  if(filterNrInEvt<trigEvt->sizeFilters()){ //filter found in event

    const trigger::Keys& trigKeys = trigEvt->filterKeys(filterNrInEvt);  //trigger::Keys is actually a vector<uint16_t> holding the position of trigger objects in the trigger collection passing the filter
    const trigger::TriggerObjectCollection & trigObjColl(trigEvt->getObjects());
    for(trigger::Keys::const_iterator keyIt=trigKeys.begin();keyIt!=trigKeys.end();++keyIt){ //we now have access to all trigger objects passing filter
      float trigObjEta = trigObjColl[*keyIt].eta();
      float trigObjPhi = trigObjColl[*keyIt].phi();
      if (reco::deltaR(detEta,detPhi,trigObjEta,trigObjPhi) < maxDeltaR){
	return true;
      }//end dR<maxDeltaR trig obj match test
    }//end loop over all objects passing filter
  }//check filter is present in event
  return false;
}




void SHNtupliser::endJob()
{ 
  outFile_->cd();
  //quick and dirty hack as writing ints directly isnt working
  TTree* tree = new TTree("eventCountTree","Event count");
  tree->Branch("nrPass",&nrPass_,"nrPass/I");
  tree->Branch("nrTot",&nrTot_,"nrTot/I");
  tree->Fill();

  std::cout <<"job ended "<<std::endl;
}

void filterHcalHits(const SHEvent* event,double maxDR,const SHCaloHitContainer& inputHits,SHCaloHitContainer& outputHits)
{

  std::vector<std::pair<float,float> > eleEtaPhi;
  for(int eleNr=0;eleNr<event->nrElectrons();eleNr++){
    const SHElectron* ele = event->getElectron(eleNr);
    eleEtaPhi.push_back(std::make_pair(ele->detEta(),ele->detPhi()));
  }
  
  // outputHits.clear();
  double maxDR2 = maxDR*maxDR;
  for(size_t hitNr=0;hitNr<inputHits.nrHcalHitsStored();hitNr++){
    int detId = inputHits.getHcalHitByIndx(hitNr).detId();
    double cellEta=0,cellPhi=0;
    GeomFuncs::getCellEtaPhi(detId,cellEta,cellPhi);
    
    bool accept =false;
    for(size_t eleNr=0;eleNr<eleEtaPhi.size();eleNr++){
      if(MathFuncs::calDeltaR2(eleEtaPhi[eleNr].first,eleEtaPhi[eleNr].second,
			       cellEta,cellPhi)<maxDR2){
	accept=true;
	break;
      }
    }//end ele loop
    if(accept) outputHits.addHit(inputHits.getHcalHitByIndx(hitNr));
    
  }//end hit loop


}

void filterEcalHits(const SHEvent* event,double maxDR,const SHCaloHitContainer& inputHits,SHCaloHitContainer& outputHits)
{

  std::vector<std::pair<float,float> > eleEtaPhi;
  for(int eleNr=0;eleNr<event->nrElectrons();eleNr++){
    const SHElectron* ele = event->getElectron(eleNr);
    eleEtaPhi.push_back(std::make_pair(ele->detEta(),ele->detPhi()));
  }
  
  //outputHits.clear();
  double maxDR2 = maxDR*maxDR;
  for(size_t hitNr=0;hitNr<inputHits.nrEcalHitsStored();hitNr++){
    int detId = inputHits.getEcalHitByIndx(hitNr).detId();
    double cellEta=0,cellPhi=0;
    GeomFuncs::getCellEtaPhi(detId,cellEta,cellPhi);
    
    bool accept =false;
    for(size_t eleNr=0;eleNr<eleEtaPhi.size();eleNr++){
      if(MathFuncs::calDeltaR2(eleEtaPhi[eleNr].first,eleEtaPhi[eleNr].second,
			       cellEta,cellPhi)<maxDR2){
	accept=true;
	break;
      }
    }//end ele loop
    if(accept) outputHits.addHit(inputHits.getEcalHitByIndx(hitNr));
    
  }//end hit loop


}
  
void filterCaloTowers(const SHEvent* event,double maxDR,const SHCaloTowerContainer& inputHits,SHCaloTowerContainer& outputHits)
{

  std::vector<std::pair<float,float> > eleEtaPhi;
  for(int eleNr=0;eleNr<event->nrElectrons();eleNr++){
    const SHElectron* ele = event->getElectron(eleNr);
    eleEtaPhi.push_back(std::make_pair(ele->detEta(),ele->detPhi()));
  }
  
  outputHits.clear();
  double maxDR2 = maxDR*maxDR;
  for(size_t hitNr=0;hitNr<inputHits.nrCaloTowersStored();hitNr++){
    float towerEta = inputHits.getCaloTowerByIndx(hitNr).eta();
    float towerPhi = inputHits.getCaloTowerByIndx(hitNr).phi(); 
  
    bool accept =false;
    for(size_t eleNr=0;eleNr<eleEtaPhi.size();eleNr++){
      if(MathFuncs::calDeltaR2(eleEtaPhi[eleNr].first,eleEtaPhi[eleNr].second,
			       towerEta,towerPhi)<maxDR2){
	accept=true;
	break;
      }
    }//end ele loop
    if(accept) outputHits.addTower(inputHits.getCaloTowerByIndx(hitNr));
    
  }//end hit loop


}

void fillPFCands(const SHEvent* event,double maxDR,SHPFCandContainer& shPFCands,const std::vector<reco::PFCandidate>& pfCands,const reco::VertexRef mainVtx,const edm::Handle<reco::VertexCollection> vertices)
{
  //  std::cout <<"filling candidates "<<std::endl;

  const double maxDR2 = maxDR*maxDR;
  std::vector<std::pair<float,float> > eleEtaPhi;
  for(int eleNr=0;eleNr<event->nrElectrons();eleNr++){
    const SHElectron* ele = event->getElectron(eleNr);
    if(ele->et()>20){
      eleEtaPhi.push_back(std::make_pair(ele->detEta(),ele->detPhi()));
    }
  }

  for(size_t candNr=0;candNr<pfCands.size();candNr++){
    const reco::PFCandidate& pfParticle = pfCands[candNr];
    int scSeedCrysId=0;
    if(pfParticle.superClusterRef().isNonnull()) scSeedCrysId=pfParticle.superClusterRef()->seed()->seed().rawId();
  
    bool accept =false;
    for(size_t eleNr=0;eleNr<eleEtaPhi.size();eleNr++){
      if(MathFuncs::calDeltaR2(eleEtaPhi[eleNr].first,eleEtaPhi[eleNr].second,
			       pfParticle.eta(),pfParticle.phi())<maxDR2){
	accept=true;
	break;
      }
    }//end ele loop
    //std::cout <<"cand nr "<<candNr<<" / "<<pfCands.size()<<" accept "<<std::endl;

    if(accept){
      if(pfParticle.pdgId()==22){   
	shPFCands.addPhoton(pfParticle.pt(),pfParticle.eta(),pfParticle.phi(),pfParticle.mass(),pfParticle.mva_nothing_gamma(),scSeedCrysId);
      }else if(abs(pfParticle.pdgId())==130){
	shPFCands.addNeutralHad(pfParticle.pt(),pfParticle.eta(),pfParticle.phi(),pfParticle.mass(),pfParticle.mva_nothing_gamma(),scSeedCrysId);
	
      }else if(abs(pfParticle.pdgId()) == 211){
	reco::VertexRef pfCandVtx= chargedHadronVertex(pfParticle,vertices);

	//	float dz = fabs(pfCandVtx->z()-mainVtx->z());
	if(pfCandVtx==mainVtx){
	
	  SHPFCandidate& shPFCand =shPFCands.addChargedHad(pfParticle.pt(),pfParticle.eta(),pfParticle.phi(),pfParticle.mass(),pfParticle.mva_nothing_gamma(),scSeedCrysId);
	  shPFCand.setVertex(pfCandVtx->x(),pfCandVtx->y(),pfCandVtx->z());
	}
      }
    }	 
  }
}

//stolen from EGamma/PFIsolationEstimator
reco::VertexRef chargedHadronVertex( const reco::PFCandidate& pfcand,edm::Handle< reco::VertexCollection > verticesColl)
{

  //code copied from Florian's PFNoPU class
    
  reco::TrackBaseRef trackBaseRef( pfcand.trackRef() );

  size_t  iVertex = 0;
  unsigned index=0;
  unsigned nFoundVertex = 0;

  float bestweight=0;
  
  const reco::VertexCollection& vertices = *(verticesColl.product());

  for( reco::VertexCollection::const_iterator iv=vertices.begin(); iv!=vertices.end(); ++iv, ++index) {
    
    const reco::Vertex& vtx = *iv;
    
    // loop on tracks in vertices
    for(reco::Vertex::trackRef_iterator iTrack=vtx.tracks_begin();iTrack!=vtx.tracks_end(); ++iTrack) {

      const reco::TrackBaseRef& baseRef = *iTrack;

      // one of the tracks in the vertex is the same as 
      // the track considered in the function
      if(baseRef == trackBaseRef ) {
        float w = vtx.trackWeight(baseRef);
        //select the vertex for which the track has the highest weight
        if (w > bestweight){
          bestweight=w;
          iVertex=index;
          nFoundVertex++;
        }
      }
    }
    
  }
 
 
  
  if (nFoundVertex>0){
    //if (nFoundVertex!=1)
      //  edm::LogWarning("TrackOnTwoVertex")<<"a track is shared by at least two verteces. Used to be an assert";
    return  reco::VertexRef( verticesColl, iVertex);
  }
  // no vertex found with this track. 

  // optional: as a secondary solution, associate the closest vertex in z
  bool checkClosestZVertex=true;
  if ( checkClosestZVertex ) {

    double dzmin = 10000.;
    double ztrack = pfcand.vertex().z();
    bool foundVertex = false;
    index = 0;
    for( reco::VertexCollection::const_iterator  iv=vertices.begin(); iv!=vertices.end(); ++iv, ++index) {

      double dz = fabs(ztrack - iv->z());
      if(dz<dzmin) {
        dzmin = dz;
        iVertex = index;
        foundVertex = true;
      }
    }

    if( foundVertex ) 
      return  reco::VertexRef( verticesColl, iVertex);  
  
  }
   
  return  reco::VertexRef( );
}


void addInDeLaseredTriggerNrgys(const heep::Event& heepEvent,SHEvent& shEvent)
{
  edm::InputTag newSCEETag("hltCorrectedMulti5x5SuperClustersWithPreshowerActivity");
  edm::InputTag newSCEBTag("hltCorrectedHybridSuperClustersActivity");
  edm::Handle<reco::SuperClusterCollection> newSCEEHandle;
  heepEvent.event().getByLabel(newSCEETag,newSCEEHandle);
  edm::Handle<reco::SuperClusterCollection> newSCEBHandle;
  heepEvent.event().getByLabel(newSCEBTag,newSCEBHandle);
 
  trigger::TriggerObjectCollection trigObjsWithNewNrgy(heepEvent.triggerEvent().getObjects());
  setTrigObsToNewNrgy(*newSCEBHandle,*newSCEEHandle,trigObjsWithNewNrgy);
  
  const trigger::TriggerEvent& trigEvt = heepEvent.triggerEvent();
  //  const edm::TriggerResults& trigResults = *heepEvent.handles().trigResults;
  // const edm::TriggerNames& trigNames = heepEvent.event().triggerNames(trigResults); 

  for(size_t filterNr=0;filterNr<trigEvt.sizeFilters();filterNr++){
    SHTrigInfo trigInfo;
    trigInfo.setTrigId(-1);
    trigInfo.setTrigName(trigEvt.filterTag(filterNr).label()+"NewNrgy");
    const trigger::Keys& trigKeys = trigEvt.filterKeys(filterNr);  //trigger::Keys is actually a vector<uint16_t> holding the position of trigger objects in the trigger collection passing the filter
  
    for(trigger::Keys::const_iterator keyIt=trigKeys.begin();keyIt!=trigKeys.end();++keyIt){
      const trigger::TriggerObject& obj = trigObjsWithNewNrgy[*keyIt];
      TLorentzVector p4;
      p4.SetPtEtaPhiM(obj.pt(),obj.eta(),obj.phi(),obj.mass());
      trigInfo.addObj(p4);
    }
    if(!trigKeys.empty()) shEvent.addTrigInfo(trigInfo); //only adding triggers which actually have objects passing
  } 
}

void setTrigObsToNewNrgy(const reco::SuperClusterCollection& ebSCs,const reco::SuperClusterCollection& eeSCs,trigger::TriggerObjectCollection& trigObjs)
{
  
  for(size_t trigNr=0;trigNr<trigObjs.size();trigNr++){ //so trigObj.id() is only set if its actually ided as a particle (ie muon (possibly), ele, tau) so have to do all of them
    if(abs(trigObjs[trigNr].id())!=11 && trigObjs[trigNr].id()!=0) continue; //it will be iether 0 or 11, sadly most things are zero

    const reco::SuperClusterCollection& scColl = fabs(trigObjs[trigNr].eta())<1.5 ? ebSCs : eeSCs;
    float bestDR2 = 0.02*.02;
    const reco::SuperCluster* bestSC = 0;
    for(size_t scNr=0;scNr<scColl.size();scNr++){
      float dR2=reco::deltaR2(scColl[scNr].eta(),scColl[scNr].phi(),trigObjs[trigNr].eta(),trigObjs[trigNr].phi());
      //std::cout <<"scNr "<<scNr<<" dR2 "<<dR2<<std::endl;
      if(dR2<bestDR2){
	bestDR2 = dR2;
	bestSC = &scColl[scNr];
      }
    } //end loop over sc
    if(bestSC){
      //std::cout <<" new energy "<<bestSC->energy()<<" old energy "<<trigObjs[trigNr].energy()<<std::endl;
      // trigObjs[trigNr].setPt(bestSC->energy()/trigObjs[trigNr].energy()*trigObjs[trigNr].pt()); 
      trigObjs[trigNr].setPt(bestSC->energy()*sin(bestSC->position().theta()));
      trigObjs[trigNr].setEta(bestSC->eta());
      trigObjs[trigNr].setPhi(bestSC->phi());
      
    }else trigObjs[trigNr].setPt(-1*trigObjs[trigNr].pt());
    
    
    //std::cout <<"trigObj "<<trigNr<<" id "<<trigObjs[trigNr].id()<<" et "<<trigObjs[trigNr].pt()<<std::endl;
  }//end loop over 
}



//define this as a plug-in
DEFINE_FWK_MODULE(SHNtupliser);
