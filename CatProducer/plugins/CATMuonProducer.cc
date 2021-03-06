#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/Common/interface/Association.h"
#include "DataFormats/Common/interface/RefToPtr.h"

#include "DataFormats/PatCandidates/interface/Muon.h"
#include "CATTools/DataFormats/interface/Muon.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidate.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidateFwd.h"
#include "DataFormats/PatCandidates/interface/PackedCandidate.h"
#include "CommonTools/UtilAlgos/interface/StringCutObjectSelector.h"
#include "RecoEcal/EgammaCoreTools/interface/EcalClusterLazyTools.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "FWCore/Utilities/interface/isFinite.h"

using namespace edm;
using namespace std;

namespace cat {

  class CATMuonProducer : public edm::EDProducer {
  public:
    explicit CATMuonProducer(const edm::ParameterSet & iConfig);
    virtual ~CATMuonProducer() { }

    virtual void produce(edm::Event & iEvent, const edm::EventSetup & iSetup);

    bool mcMatch( const reco::Candidate::LorentzVector& lepton, Handle<reco::GenParticleCollection> genParticles );
    bool MatchObjects( const reco::Candidate::LorentzVector& pasObj, const reco::Candidate::LorentzVector& proObj, bool exact );

  private:
    edm::EDGetTokenT<pat::MuonCollection> src_;
    edm::EDGetTokenT<reco::GenParticleCollection> mcLabel_;
    edm::EDGetTokenT<reco::VertexCollection> vertexLabel_;
    edm::EDGetTokenT<reco::BeamSpot> beamLineSrc_;
    bool runOnMC_;

  };

} // namespace

cat::CATMuonProducer::CATMuonProducer(const edm::ParameterSet & iConfig) :
  src_(consumes<pat::MuonCollection>(iConfig.getParameter<edm::InputTag>("src"))),
  mcLabel_(consumes<reco::GenParticleCollection>(iConfig.getParameter<edm::InputTag>("mcLabel"))),
  vertexLabel_(consumes<reco::VertexCollection>(iConfig.getParameter<edm::InputTag>("vertexLabel"))),
  beamLineSrc_(consumes<reco::BeamSpot>(iConfig.getParameter<edm::InputTag>("beamLineSrc"))),
  runOnMC_(iConfig.getParameter<bool>("runOnMC"))
{
  produces<std::vector<cat::Muon> >();
}

void 
cat::CATMuonProducer::produce(edm::Event & iEvent, const edm::EventSetup & iSetup) 
{
  Handle<pat::MuonCollection> src;
  iEvent.getByToken(src_, src);
 
  Handle<reco::GenParticleCollection> genParticles;
  if (runOnMC_) iEvent.getByToken(mcLabel_,genParticles);
    
  Handle<reco::VertexCollection> recVtxs;
  iEvent.getByToken(vertexLabel_,recVtxs);

  Handle<reco::BeamSpot> beamSpotHandle;
  iEvent.getByToken(beamLineSrc_, beamSpotHandle);

  reco::Vertex pv = recVtxs->at(0);
   
  reco::BeamSpot beamSpot = *beamSpotHandle;
  reco::TrackBase::Point beamPoint(beamSpot.x0(), beamSpot.y0(), beamSpot.z0());
  // //  beamPoint = reco::TrackBase::Point ( beamSpot.x0(), beamSpot.y0(), beamSpot.z0() );  
 
  auto_ptr<vector<cat::Muon> >  out(new vector<cat::Muon>());

  for (const pat::Muon & aPatMuon : *src) {
    cat::Muon aMuon(aPatMuon);

    double pt    = aPatMuon.pt() ;

    double chIso04 = aPatMuon.chargedHadronIso();
    double nhIso04 = aPatMuon.neutralHadronIso();
    double phIso04 = aPatMuon.photonIso();
    double puIso04 = aPatMuon.puChargedHadronIso();
    aMuon.setrelIso(0.4, chIso04, nhIso04, phIso04, puIso04, pt);

    double chIso03 = aPatMuon.userIsolation("pat::User1Iso");
    double nhIso03 = aPatMuon.userIsolation("pat::User2Iso");
    double phIso03 = aPatMuon.userIsolation("pat::User3Iso");
    double puIso03 = aPatMuon.userIsolation("pat::User4Iso");
    aMuon.setrelIso(0.3, chIso03, nhIso03, phIso03, puIso03, pt);

    aMuon.setIsGlobalMuon( aPatMuon.isGlobalMuon() );
    aMuon.setIsPFMuon( aPatMuon.isPFMuon() );
    aMuon.setIsTightMuon( aPatMuon.isTightMuon(pv) );
    aMuon.setIsLooseMuon( aPatMuon.isLooseMuon() );
    aMuon.setIsSoftMuon( aPatMuon.isSoftMuon(pv) );

    if (runOnMC_)
      aMuon.setMCMatched( mcMatch( aPatMuon.p4(), genParticles ) );
    
    aMuon.setNumberOfMatchedStations( aPatMuon.numberOfMatchedStations() );

    if ( aPatMuon.globalTrack().isNonnull() && aPatMuon.globalTrack().isAvailable() ) {
      aMuon.setNormalizedChi2( aPatMuon.normChi2() );
      aMuon.setNumberOfValidMuonHits( aPatMuon.globalTrack()->hitPattern().numberOfValidMuonHits() );
    }

    if ( aPatMuon.innerTrack().isNonnull() && aPatMuon.innerTrack().isAvailable() ){
      aMuon.setNumberOfValidHits( aPatMuon.numberOfValidHits() );
      aMuon.setNumberOfValidPixelHits( aPatMuon.innerTrack()->hitPattern().numberOfValidPixelHits() );
      aMuon.setTackerLayersWithMeasurement( aPatMuon.innerTrack()->hitPattern().trackerLayersWithMeasurement() ); 
    }
    double dxy = fabs(aPatMuon.muonBestTrack()->dxy(pv.position()));
    aMuon.setDxy( dxy );
    double dz = fabs(aPatMuon.muonBestTrack()->dz(pv.position()));
    aMuon.setDz( dz ); 
 
    out->push_back(aMuon);
  }

  iEvent.put(out);
}

bool cat::CATMuonProducer::mcMatch( const reco::Candidate::LorentzVector& lepton, Handle<reco::GenParticleCollection> genParticles ){
  bool out = false;

  for (const reco::GenParticle & aGenPart : *genParticles){
    if( abs(aGenPart.pdgId()) != 13 ) continue;

    bool match = MatchObjects(lepton, aGenPart.p4(), false);

    if( match != true) continue;
   
    const reco::Candidate* mother = aGenPart.mother();
    while( mother != 0 ){
      if( abs(mother->pdgId()) == 23 || abs(mother->pdgId()) == 24 ) {
        out = true;
      }
      mother = mother->mother();
    }
  }

  return out;
}


bool cat::CATMuonProducer::MatchObjects( const reco::Candidate::LorentzVector& pasObj, const reco::Candidate::LorentzVector& proObj, bool exact ) {
  double proEta = proObj.eta();
  double proPhi = proObj.phi();
  double proPt = proObj.pt();
  double pasEta = pasObj.eta();
  double pasPhi = pasObj.phi();
  double pasPt = pasObj.pt();

  double dRval = deltaR(proEta, proPhi, pasEta, pasPhi);
  double dPtRel = 999.0;
  if( proPt > 0.0 ) dPtRel = fabs( pasPt - proPt )/proPt;
  // If we are comparing two objects for which the candidates should
  // be exactly the same, cut hard. Otherwise take cuts from user.
  if( exact ) return ( dRval < 1e-3 && dPtRel < 1e-3 );
  else return ( dRval < 0.025 && dPtRel < 0.025 );
}



#include "FWCore/Framework/interface/MakerMacros.h"
using namespace cat;
DEFINE_FWK_MODULE(CATMuonProducer);
