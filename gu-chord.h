#ifndef GU_CHORD_H
#define GU_CHORD_H

#include "ns3/gu-application.h"
#include "ns3/gu-chord-message.h"
#include "ns3/ping-request.h"
#include "ns3/ipv4-address.h"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <openssl/sha.h>
#include "ns3/socket.h"
#include "ns3/nstime.h"
#include "ns3/timer.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"

using namespace ns3;

class GUChord : public GUApplication
{
  public:
    static TypeId GetTypeId (void);
    GUChord ();
    virtual ~GUChord ();
    void SendPing (Ipv4Address destAddress, std::string pingMessage);
    void RecvMessage (Ptr<Socket> socket);
    void ProcessPingReq (GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    void ProcessPingRsp (GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    void AuditPings ();
    uint32_t GetNextTransactionId ();
    void StopChord ();

    // Callback with Application Layer (add more when required)
    void SetPingSuccessCallback (Callback <void, Ipv4Address, std::string> pingSuccessFn);
    void SetPingFailureCallback (Callback <void, Ipv4Address, std::string> pingFailureFn);
    void SetPingRecvCallback (Callback <void, Ipv4Address, std::string> pingRecvFn);

    // From GUApplication
    virtual void ProcessCommand (std::vector<std::string> tokens);

    //send a message asking other nodes to find your successor (other nodes keep forwarding until someone figures out originalNodeNum's successor)
    void SendSuccessorReq(Ipv4Address destAddress, uint32_t originalNodeNum, uint32_t chordKey);
    //process a successorReq message: tell original node about it's successor through SendSuccessorRsp if you know it's successor or forward to your successor if not
    void ProcessSuccessorReq(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    //send a message to a node telling them about their successor
    void SendSuccessorRsp(Ipv4Address destAddress, uint32_t successorNodeNum, uint32_t successorChordKey);
    //process successorRsp message to update successor
    void ProcessSuccessorRsp(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    //tell successor you think they are your successor
    void NotifySuccessor(Ipv4Address destAddress);
    //process notify-successor message, check your predecessor
    void ProcessNotifySuccessor(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    //tell predecessor you think they are your predecessor
    void NotifyPredecessor(Ipv4Address destAddress);
    //process notify-predecessor message, check your successor
    void ProcessNotifyPredecessor(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    //send a request to successor asking for their predecessor
    void SendPredecessorOfSuccReq(Ipv4Address destAddress);
    //process request for predecessor
    void ProcessPredecessorOfSuccReq(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    //reply with your predecessor info
    void SendPredecessorOfSuccRsp(Ipv4Address destAddress, uint32_t predecessorNodeNum, uint32_t predecessorChordKey);
    //process successor's predecessor data to help stabilize links
    void ProcessPredecessorOfSuccRsp(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    //send a request for ring state outputting
    void SendRingstateReq(Ipv4Address destAddress, uint32_t originalNodeNum);
    //process a request for ring state outputting; if request initiated by current node, stop forwarding
    void ProcessRingstateReq(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);

    void NodeLeave();
    void SendNodeLeaveMsg(uint8_t isSuccessor);
    void ProcessNodeLeave(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);

    //wrapper only used for non-finger fixing searches
    void LookupWrapper(uint32_t transactionNum, uint32_t originalNodeNum, uint32_t lookupKey, uint8_t fromChord);
    
    //hardcodes some information for the wrapper to make GU search code cleaner
    void GUSearchLookup(uint32_t transactionNum, uint32_t lookupKey);

    void SendLookupReq(uint32_t transactionNum, uint32_t originalNodeNum, uint32_t lookupKey, uint8_t fromChord);
    
    void ProcessLookupReq(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    
    void SendLookupRsp(Ipv4Address destAddress, uint32_t transactionNum, uint32_t successorNodeNum, uint32_t lookupKey, uint8_t fromChord);
    
    void ProcessLookupRsp(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    
    Ipv4Address closestPrecedingNode(uint32_t targetKey);

    //used by SEARCH layer to determine if our ring is just us
    bool isAlone();

    //Callbacks for GUSearch
    void SetLookupCallback (Callback <void, Ipv4Address, uint32_t> lookup);
    void SetRedistributeKeysCallback (Callback <void,uint8_t, Ipv4Address, uint32_t> redistribute);
    //wrap inOrder to check if a key maps to ourselves.
    bool isOwn(uint32_t hashedKey);

    //get local addr in search layer
    uint32_t getLocalAddr() {return m_local.Get();}

    uint32_t getNumRingSlots() const {return numRingSlots;}
  protected:
    virtual void DoDispose ();
    
  private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    //starts chord ring with one node
    void CreateChord();
    //joins pre-existing chord ring through parameter node number
    void JoinChord(uint32_t);
    //gets hashed value from ip address parameter
    uint32_t hashIP(Ipv4Address);
    //returns true if b is between a and c (clockwise, inclusive of right side) on ring, false otherwise
    bool inOrder(uint32_t a, uint32_t b, uint32_t c);
    
    //check that predecessor of successor is ourselves (not in-between, otherwise update successor)
    void stabilize();

    //true if inside chord ring, false otherwise
    bool m_inChord;
    //local node number (not hashed value)
    uint32_t m_localNodeNum;
    //chord hash key (computed from local ip address)
    uint32_t m_localHashNum;
    //number of indices in chord ring
    // static const uint32_t numRingSlots = 1048576; //2^20;
    static const uint32_t numRingSlots = 4096;
    static const uint32_t fingerTableSize = 12;
    //hashNode is made of a node's ip address and the hash chordkey the ip is calculated to
    struct hashNode
      {
        Ipv4Address ip;
        uint32_t hashValue;
      };
    //successor of local node
    hashNode successor;
    //predecessor of local node
    hashNode predecessor;

    //New timers for stabilization
    Timer m_stabilizeTimer;
    Time m_stabilizeTimeOut;

    void PrintFingerTable();
    
    std::vector<hashNode> fingerTable;
    void FixFingerTable();

    //returns true if b is between a and c (clockwise) on ring, false otherwise
    bool inBetween(uint32_t a, uint32_t b, uint32_t c);

    //key: transactionId, value: index of fingerTable
    std::map<uint32_t, uint32_t> fingerRequestTracker;

    //New timers for finger table fixing
    Timer m_fixFingersTimer;
    Time m_fixFingersTimeOut;

    Callback <void, Ipv4Address, uint32_t> m_lookup;
    Callback <void, uint8_t, Ipv4Address, uint32_t> m_redistribute;

    uint32_t m_currentTransactionId;
    Ptr<Socket> m_socket;
    Time m_pingTimeout;
    uint16_t m_appPort;
    // Timers
    Timer m_auditPingsTimer;
    // Ping tracker
    std::map<uint32_t, Ptr<PingRequest> > m_pingTracker;
    // Callbacks
    Callback <void, Ipv4Address, std::string> m_pingSuccessFn;
    Callback <void, Ipv4Address, std::string> m_pingFailureFn;
    Callback <void, Ipv4Address, std::string> m_pingRecvFn;
};
#endif